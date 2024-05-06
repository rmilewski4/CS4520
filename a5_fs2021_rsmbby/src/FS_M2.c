#include "dyn_array.h"
#include "bitmap.h"
#include "block_store.h"
#include "FS_M2.h"

#define BLOCK_STORE_NUM_BLOCKS 65536    // 2^16 blocks.
#define BLOCK_STORE_AVAIL_BLOCKS 65534  // Last 2 blocks consumed by the FBM
#define BLOCK_SIZE_BITS 32768           // 2^12 BYTES per block *2^3 BITS per BYTES
#define BLOCK_SIZE_BYTES 4096           // 2^12 BYTES per block
#define BLOCK_STORE_NUM_BYTES (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES)  // 2^16 blocks of 2^12 bytes.


// You might find this handy.  I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)


#define number_inodes 256
#define inode_size 64
#define number_fd 256
#define fd_size 6	// any number as you see fit

#define folder_number_entries 31

// each inode represents a regular file or a directory file
struct inode 
{
    uint32_t vacantFile;    // this parameter is only for directory. Used as a bitmap denoting availibility of entries in a directory file.
    char owner[18];         // for alignment purpose only   

    char fileType;          // 'r' denotes regular file, 'd' denotes directory file

    size_t inodeNumber;			// for FS, the range should be 0-255
    size_t fileSize; 			  // the unit is in byte	
    size_t linkCount;

    // to realize the 16-bit addressing, pointers are acutally block numbers, rather than 'real' pointers.
    uint16_t directPointer[6];
    uint16_t indirectPointer[1];
    uint16_t doubleIndirectPointer;
};


struct fileDescriptor 
{
    uint8_t inodeNum;	// the inode # of the fd

    // usage, locate_order and locate_offset together locate the exact byte at which the cursor is 
    uint8_t usage; 		// inode pointer usage info. Only the lower 3 digits will be used. 1 for direct, 2 for indirect, 4 for dbindirect
    uint16_t locate_order;		// serial number or index of the block within direct, indirect, or dbindirect range
    uint16_t locate_offset;		// offset of the cursor within a block
};


struct directoryFile {
    char filename[127];
    uint8_t inodeNumber;
};


struct FS {
    block_store_t * BlockStore_whole;
    block_store_t * BlockStore_inode;
    block_store_t * BlockStore_fd;
};




/// Formats (and mounts) an FS file for use
/// \param fname The file to format
/// \return Mounted FS object, NULL on error
///
FS_t *fs_format(const char *path)
{
    if(path != NULL && strlen(path) != 0)
    {
        FS_t * ptr_FS = (FS_t *)calloc(1, sizeof(FS_t));	// get started
        ptr_FS->BlockStore_whole = block_store_create(path);				// pointer to start of a large chunck of memory

        // reserve the 1st block for bitmap of inode
        size_t bitmap_ID = block_store_allocate(ptr_FS->BlockStore_whole);
        //		printf("bitmap_ID = %zu\n", bitmap_ID);

        // 2rd - 5th block for inodes, 4 blocks in total
        size_t inode_start_block = block_store_allocate(ptr_FS->BlockStore_whole);
        //		printf("inode_start_block = %zu\n", inode_start_block);		
        for(int i = 0; i < 3; i++)
        {
            block_store_allocate(ptr_FS->BlockStore_whole);
            //			printf("all the way with block %zu\n", block_store_allocate(ptr_FS->BlockStore_whole));
        }

        // install inode block store inside the whole block store
        ptr_FS->BlockStore_inode = block_store_inode_create(block_store_Data_location(ptr_FS->BlockStore_whole) + bitmap_ID * BLOCK_SIZE_BYTES, block_store_Data_location(ptr_FS->BlockStore_whole) + inode_start_block * BLOCK_SIZE_BYTES);

        // the first inode is reserved for root dir
        block_store_sub_allocate(ptr_FS->BlockStore_inode);
        //		printf("first inode ID = %zu\n", block_store_sub_allocate(ptr_FS->BlockStore_inode));

        // update the root inode info.
        uint8_t root_inode_ID = 0;	// root inode is the first one in the inode table
        inode_t * root_inode = (inode_t *) calloc(1, sizeof(inode_t));
        //		printf("size of inode_t = %zu\n", sizeof(inode_t));
        root_inode->vacantFile = 0x00000000;
        root_inode->fileType = 'd';								
        root_inode->inodeNumber = root_inode_ID;
        root_inode->linkCount = 1;
        //		root_inode->directPointer[0] = root_data_ID;	// not allocate date block for it until it has a sub-folder or file
        block_store_inode_write(ptr_FS->BlockStore_inode, root_inode_ID, root_inode);		
        free(root_inode);

        // now allocate space for the file descriptors
        ptr_FS->BlockStore_fd = block_store_fd_create();

        return ptr_FS;
    }

    return NULL;	
}

///
/// Mounts an FS object and prepares it for use
/// \param fname The file to mount

/// \return Mounted FS object, NULL on error

///
FS_t *fs_mount(const char *path)
{
    if(path != NULL && strlen(path) != 0)
    {
        FS_t * ptr_FS = (FS_t *)calloc(1, sizeof(FS_t));	// get started
        ptr_FS->BlockStore_whole = block_store_open(path);	// get the chunck of data	

        // the bitmap block should be the 1st one
        size_t bitmap_ID = 0;

        // the inode blocks start with the 2nd block, and goes around until the 5th block, 4 in total
        size_t inode_start_block = 1;

        // attach the bitmaps to their designated place
        ptr_FS->BlockStore_inode = block_store_inode_create(block_store_Data_location(ptr_FS->BlockStore_whole) + bitmap_ID * BLOCK_SIZE_BYTES, block_store_Data_location(ptr_FS->BlockStore_whole) + inode_start_block * BLOCK_SIZE_BYTES);

        // since file descriptors are allocated outside of the whole blocks, we can simply reallocate space for it.
        ptr_FS->BlockStore_fd = block_store_fd_create();

        return ptr_FS;
    }

    return NULL;		
}




///
/// Unmounts the given object and frees all related resources
/// \param fs The FS object to unmount
/// \return 0 on success, < 0 on failure
///
int fs_unmount(FS_t *fs)
{
    if(fs != NULL)
    {	
        block_store_inode_destroy(fs->BlockStore_inode);

        block_store_destroy(fs->BlockStore_whole);
        block_store_fd_destroy(fs->BlockStore_fd);

        free(fs);
        return 0;
    }
    return -1;
}
void free_str_array(char** path_elems, int number_of_path_elems) {
    for(int abort = 0; abort < number_of_path_elems;abort++) {
        free(path_elems[abort]);
        path_elems[abort] = NULL;
    }
    free(path_elems);
    path_elems = NULL;
}

int fs_create(FS_t *fs, const char *path, file_t type)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    next, check if path exists already (i.e. make sure file/dir doesn't already exist)
    if not the case, then we can continue. Starting at root, which should already exist.
    Allocate blocks for root inode pointers if it hasn't already
    Grab part of the path after the root (/), and see if this directory already exists.
    If it does, continue, if not, then create the inode & allocate space for it as well.
    Continue down the path until the last part is reached. Allocate inode and space for this entry based on the type given
    Update block store and return the finished file system.
    */
    if(fs == NULL || path == NULL || type < 0 || type > 1) {
        return -1;
    }
    if(*path != '/') {
        //return error if path doesn't start with root directory.
        return -1;
    }
    char lastchar = path[strlen(path)-1];
    if(lastchar == '/') {
        //return error if path ends with trailing slash.
        return -1;
    }
    //we need to break up the path into the separate pieces in between slashes. We know that for extended paths, the first couple will all be directories, and the last will be of type specified by type
    //store each path elem. in array of strings
    int number_of_path_elems = 0;
    //need to copy file path to get elements out since it is a const.
    char* copied_path = calloc(1,strlen(path) + 1);
    strcpy(copied_path,path);
    //temp elem to be used with strtok
    char* temp_elem;
    //using strtok to figure out how many elements there are
    temp_elem = strtok(copied_path,"/");
    while(temp_elem != NULL) {
        //loop to count # of elems in path
        number_of_path_elems++;
        //error check to make sure no element is larger than 127 characters.
        if(strlen(temp_elem) > 127) {
            free(copied_path);
            return -1;
        }
        temp_elem = strtok(NULL,"/");
    }
    if(number_of_path_elems == 0) {
        //no path given, can't recreate root, as that was done in format.
        free(copied_path);
        copied_path = NULL;
        return -1;
    }
    //array of strings to hold # of elemnents. Now need to loop back through and pull out individual path elements
    char ** path_elems = calloc(1,sizeof(char*) * number_of_path_elems);
    if(path_elems == NULL) {
        free(copied_path);
        copied_path = NULL;
        return -1;
    }
    //get original path to loop through again
    strcpy(copied_path,path);
    temp_elem = strtok(copied_path, "/");
    for(int i = 0; i < number_of_path_elems; i++) {
        //we know that maximum file name is 127 characters, so only store that many.
        path_elems[i] = calloc(1,sizeof(char) * 127);
        if(path_elems[i] == NULL) {
            free(path_elems);
            free(copied_path);
            copied_path = NULL;
            return -1;
        }
        //copy elements into path array
        strcpy(path_elems[i],temp_elem);
        temp_elem = strtok(NULL, "/");
    }


    free(copied_path);
    copied_path = NULL;
    //now need to traverse down the path until we reach the parent directory inode, as that is where this file will be referenced from
    //Start at root, we need to get it's details first
    size_t parent_inode_num = 0;
    inode_t* parent_inode = calloc(1,sizeof(inode_t));
    if(parent_inode == NULL) {
        //malloc failed, return error.
        free_str_array(path_elems, number_of_path_elems);
        return -1;
    }
    //directory files occupy one block.
    directoryFile_t* parent_data = NULL;
    parent_data = calloc(1,BLOCK_SIZE_BYTES);
    if(parent_data == NULL) {
        //malloc failed, return error.
        free_str_array(path_elems, number_of_path_elems);
        free(parent_inode);
        parent_inode = NULL;
        return -1;
    }
    block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);

    //we stop one before bc we want parent directory inode
    for(int traverse_count = 0; traverse_count < number_of_path_elems -1; traverse_count++) {
        block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);
        //we know if this inode is a directory (as it should be, it should have flag setup). The directory should also not be vacant, as we are not creating directories along the given path.
        if(parent_inode->fileType  == 'd' && parent_inode->vacantFile != 0) {
            //read the data which should be stored in the first direct pointer block
            block_store_read(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
            //at this point, we have a parent inode, so we need to look through the inode directory file (sequential search on it until we have a hit. If path is invalid (i.e. couldn't find filename in data, return error))
            int file_counter = 0;
            for(file_counter = 0; file_counter < 31; file_counter++) {
                if(strcmp((parent_data + file_counter)->filename,path_elems[traverse_count])==0 && ((parent_inode->vacantFile>>file_counter) & 1)==1) {
                    parent_inode_num = (parent_data+file_counter)->inodeNumber;
                    break;
                }
            }
            //if we traverse all the way through the bitmap and can't find the file, then the given path does not exist, and we return an error.
            if(file_counter == 31) {
                free_str_array(path_elems,number_of_path_elems);
                free(parent_inode);
                free(parent_data);
                parent_inode = NULL;
                parent_data = NULL;
                return -1;
            }
        }
        else {
            //uh oh, given parent inode is a file, not a directory, which is an error. Free and return error!
            free_str_array(path_elems,number_of_path_elems);
            free(parent_inode);
            free(parent_data);
            parent_inode = NULL;
            parent_data = NULL;
            return -1;
        }
    }
    //we now can read parent inode
    block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);
    //check if parent is actually directory, return error if not
    if(parent_inode->fileType == 'r') {
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        parent_inode = NULL;
        parent_data = NULL;
        return -1;
    }
    //we now need to check if file/dir already exists in parent inode. If vacant file, obviously it won't exist so we don't need to do this
    if(parent_inode->vacantFile != 0) {
        //read parent data
        block_store_read(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
        for(int exists_counter = 0; exists_counter < 31; exists_counter++) {
            char* filename = (parent_data + exists_counter)->filename;
            if(strcmp(filename,path_elems[number_of_path_elems-1])==0 && ((parent_inode->vacantFile>>exists_counter) & 1)==1) {
                free_str_array(path_elems,number_of_path_elems);
                free(parent_inode);
                free(parent_data);
                parent_inode = NULL;
                parent_data = NULL;
                return -1;
            }
        } 
    }
    //since vacant file, we allocate space for the directory file since it will be needed later.
    else {
        size_t dir_file_num = block_store_allocate(fs->BlockStore_whole);
        if(dir_file_num == SIZE_MAX) {
            free_str_array(path_elems,number_of_path_elems);
            free(parent_inode);
            free(parent_data);
            parent_inode = NULL;
            parent_data = NULL;
            return -1;
        }
        parent_inode->directPointer[0] = dir_file_num;

    }
    //we now know that the parent inode has a valid path, and that space in the directory file can be allocated. We may run into cases we need to catch if the directory file is full, etc.
    //we need to find space in the bitmap located at vacant file
    int looking_for_space;
    for(looking_for_space = 0; looking_for_space < 31; looking_for_space++) {
        if((parent_inode->vacantFile >> looking_for_space) == 0) {
            break;
        }
    }
    //if we get to the end & still no open space was found, then the directory is full, so we return an error.
    if(looking_for_space == 31) {
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        parent_inode = NULL;
        parent_data = NULL;
        return -1;
    }
    //we made it! We now have a location in the directory file (looking_for_space) to hold our file/directory. Now we just need to create a child inode, and update our parent!
    size_t child_inode_num = block_store_sub_allocate(fs->BlockStore_inode);
    //out of space in inode table
    if(child_inode_num == SIZE_MAX) {
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        parent_inode = NULL;
        parent_data = NULL;
        return -1;
    }
    inode_t* child_inode = calloc(1,sizeof(inode_t));
    child_inode->inodeNumber = child_inode_num;
    if(type == FS_DIRECTORY) {
        child_inode->fileType = 'd';
        child_inode->vacantFile = 0;
    }
    else {
        child_inode->fileType = 'r';
    }
    child_inode->fileSize = 0;
    child_inode->linkCount = 1;
    //all fields of child up to date. Just need to update parent, write parent and child back to table!
    //set bit of vacant file bitmap to indicate new space is filled.
    parent_inode->vacantFile |= (1 << looking_for_space);
    strcpy((parent_data + looking_for_space)->filename,path_elems[number_of_path_elems-1]);
    (parent_data + looking_for_space)->inodeNumber = child_inode_num;
    //write the parent data back to its block
    block_store_write(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
    free(parent_data);
    parent_data = NULL;


    block_store_inode_write(fs->BlockStore_inode,parent_inode_num,parent_inode);
    block_store_inode_write(fs->BlockStore_inode,child_inode_num,child_inode);
    //we're done, so free temp stuff.
    free_str_array(path_elems,number_of_path_elems);
    free(parent_inode);
    free(child_inode);
    parent_inode = NULL;
    return 0;
}

int fs_open(FS_t *fs, const char *path)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    next, check if path exists already (i.e. make sure file already exists). Return an error if it doesn't exist, or if the path leads to a dir
    Create file descriptor using the fd struct, link to inode & any other variables needed, give file descriptor an id (limited to 256)
    Set seek position to 0 (beginning of file)
    Return the file descriptor #
    */
    if(fs == NULL || path == NULL) {
        return -1;
    }
    if(*path != '/') {
        //return error if path doesn't start with root directory.
        return -1;
    }
    char lastchar = path[strlen(path)-1];
    if(lastchar == '/') {
        //return error if path ends with trailing slash.
        return -1;
    }
    //we need to break up the path into the separate pieces in between slashes. We know that for extended paths, the first couple will all be directories, and the last will be of type specified by type
    //store each path elem. in array of strings
    int number_of_path_elems = 0;
    //need to copy file path to get elements out since it is a const.
    char* copied_path = calloc(1,strlen(path) + 1);
    strcpy(copied_path,path);
    //temp elem to be used with strtok
    char* temp_elem;
    //using strtok to figure out how many elements there are
    temp_elem = strtok(copied_path,"/");
    while(temp_elem != NULL) {
        //loop to count # of elems in path
        number_of_path_elems++;
        //error check to make sure no element is larger than 127 characters.
        if(strlen(temp_elem) > 127) {
            free(copied_path);
            return -1;
        }
        temp_elem = strtok(NULL,"/");
    }
    if(number_of_path_elems == 0) {
        //no path given, can't recreate root, as that was done in format.
        free(copied_path);
        copied_path = NULL;
        return -1;
    }
    //array of strings to hold # of elemnents. Now need to loop back through and pull out individual path elements
    char ** path_elems = calloc(1,sizeof(char*) * number_of_path_elems);
    if(path_elems == NULL) {
        free(copied_path);
        copied_path = NULL;
        return -1;
    }
    //get original path to loop through again
    strcpy(copied_path,path);
    temp_elem = strtok(copied_path, "/");
    for(int i = 0; i < number_of_path_elems; i++) {
        //we know that maximum file name is 127 characters, so only store that many.
        path_elems[i] = calloc(1,sizeof(char) * 127);
        if(path_elems[i] == NULL) {
            free(path_elems);
            free(copied_path);
            copied_path = NULL;
            return -1;
        }
        //copy elements into path array
        strcpy(path_elems[i],temp_elem);
        temp_elem = strtok(NULL, "/");
    }

    free(copied_path);
    copied_path = NULL;
    //now need to traverse down the path until we reach the parent directory inode, as that is where this file will be referenced from
    //Start at root, we need to get it's details first
    size_t parent_inode_num = 0;
    inode_t* parent_inode = calloc(1,sizeof(inode_t));
    if(parent_inode == NULL) {
        //malloc failed, return error.
        free_str_array(path_elems, number_of_path_elems);
        return -1;
    }
    //directory files occupy one block.
    directoryFile_t* parent_data = NULL;
    parent_data = calloc(1,BLOCK_SIZE_BYTES);
    if(parent_data == NULL) {
        //malloc failed, return error.
        free_str_array(path_elems, number_of_path_elems);
        free(parent_inode);
        parent_inode = NULL;
        return -1;
    }
    block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);

    //we stop one before bc we want parent directory inode, this will make sure these directories exist
    for(int traverse_count = 0; traverse_count < number_of_path_elems -1; traverse_count++) {
        block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);
        //we know if this inode is a directory (as it should be, it should have flag setup). The directory should also not be vacant, as we are not creating directories along the given path.
        if(parent_inode->fileType  == 'd' && parent_inode->vacantFile != 0) {
            //read the data which should be stored in the first direct pointer block
            block_store_read(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
            //at this point, we have a parent inode, so we need to look through the inode directory file (sequential search on it until we have a hit. If path is invalid (i.e. couldn't find filename in data, return error))
            int file_counter = 0;
            for(file_counter = 0; file_counter < 31; file_counter++) {
                if(strcmp((parent_data + file_counter)->filename,path_elems[traverse_count])==0 && ((parent_inode->vacantFile>>file_counter) & 1)==1) {
                    parent_inode_num = (parent_data+file_counter)->inodeNumber;
                    break;
                }
            }
            //if we traverse all the way through the bitmap and can't find the file, then the given path does not exist, and we return an error.
            if(file_counter == 31) {
                free_str_array(path_elems,number_of_path_elems);
                free(parent_inode);
                free(parent_data);
                parent_inode = NULL;
                parent_data = NULL;
                return -1;
            }
        }
        else {
            //uh oh, given parent inode is a file, not a directory, which is an error. Free and return error!
            free_str_array(path_elems,number_of_path_elems);
            free(parent_inode);
            free(parent_data);
            parent_inode = NULL;
            parent_data = NULL;
            return -1;
        }
    }
    //we now can read parent inode
    block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);
    block_store_read(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
    size_t file_inode_number;
    //find child file doing similar as above, sequentially searching through parent dir until we find given file
    int parent_counter = 0;
    for(parent_counter = 0; parent_counter < 31; parent_counter++) {
        if(strcmp((parent_data + parent_counter)->filename,path_elems[number_of_path_elems-1])==0 && ((parent_inode->vacantFile>>parent_counter) & 1)==1) {
            //once we find file, copy its inode number so we can read its inode
            file_inode_number = (parent_data+parent_counter)->inodeNumber;
            break;
        }
    }
    //if we traverse all the way through the bitmap and can't find the file, then the given path does not exist, and we return an error.
    if(parent_counter == 31) {
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        parent_inode = NULL;
        parent_data = NULL;
        return -1;
    }
    inode_t* file_inode = calloc(1,sizeof(inode_t));
    //we now know that inode exists & we have inode number, so we can read the inode
    block_store_inode_read(fs->BlockStore_inode,file_inode_number,file_inode);
    if(file_inode->fileType == 'd') {
        //can't read directory, so return error.
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        free(file_inode);
        file_inode = NULL;
        parent_inode = NULL;
        parent_data = NULL;
        return -1;
    }
    //now we can create our file descriptor!
    size_t file_descriptor_num = block_store_sub_allocate(fs->BlockStore_fd);
    if(file_descriptor_num == SIZE_MAX) {
        //out of space in fd table, so we can't proceed.
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        free(file_inode);
        file_inode = NULL;
        parent_inode = NULL;
        parent_data = NULL;
        return -1;
    }
    fileDescriptor_t* file_descriptor = calloc(1, sizeof(fileDescriptor_t));
    //we have our fd! now we just initalize it.
    file_descriptor->inodeNum = file_inode_number;
    file_descriptor->locate_offset = 0;
    file_descriptor->locate_order = 0;
    //start at 1.
    file_descriptor->usage = 1;
    //now write updates back to table.
    block_store_fd_write(fs->BlockStore_fd,file_descriptor_num,file_descriptor);
    //free temp stuff
    free_str_array(path_elems,number_of_path_elems);
    free(parent_inode);
    free(parent_data);
    free(file_inode);
    free(file_descriptor);
    file_descriptor = NULL;
    file_inode = NULL;
    parent_inode = NULL;
    parent_data = NULL;
    //return the fd number.
    return file_descriptor_num;
}

int fs_close(FS_t *fs, int fd)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    if fd is valid, then we can delete the file descriptor from the fd table, and update any associated data to indicate that it was removed
    return success
    */
   //error checking parameters
   if(fs == NULL || fd < 0 || fd > 256) {
        return -1;
   }
   //make sure block is actually being used.
   if(block_store_sub_test(fs->BlockStore_fd,fd)==false) {
       return -1;
   }
    //if we have a valid file descriptor and file system, we can proceed to remove the fd.
    block_store_sub_release(fs->BlockStore_fd,fd);
    return 0;
}

off_t fs_seek(FS_t *fs, int fd, off_t offset, seek_t whence)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all not null or invalid
    next, check to make sure the file descriptor is valid, and that it points to a valid file
    At this point, seeking can commence. We first look at the whence parameter to see where we start seeking from. 
    If we're seeking from the current position, this can be recieved from the fd. Otherwise, we assume the beginning/end of file.
    We then open up the inode of the given file. We know that each given block is 4096 bytes. So we first need to get to the correct block
    This can be done by seeing the current position and doing modular division on it by 4096 to figure out which block to start in.
    If this is greater than the 6th block, we will have to index into the indirect pointers. Once we know which block we start at, we seek by the offset.
    If this offset is greater than one block, then we will need to identify the new block in the same way we did previously. We get to the ending block, and find the new position,
    and finally return that offset. If we are at the EOF or BOF, we return that.
    */
    UNUSED(fs);
    UNUSED(fd);
    UNUSED(offset);
    UNUSED(whence);
    return 0;
}

ssize_t fs_read(FS_t *fs, int fd, void *dst, size_t nbyte)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all not null or invalid
    next, check to make sure the file descriptor is valid, and that it points to a valid file
    At this point, reading can commence. We start at the start at the given block/offset indicated by the fd.
    We read nbytes into a buffer of size nbytes. If we run to the end of a block, we grab the next block and continue reading.
    If we run into eof, we stop and return what we have. We finally return what was written to the buffer.
    */
    UNUSED(fs);
    UNUSED(fd);
    UNUSED(dst);
    UNUSED(nbyte);
    return 0;
}

ssize_t fs_write(FS_t *fs, int fd, const void *src, size_t nbyte)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all not null or invalid
    next, check to make sure the file descriptor is valid, and that it points to a valid file
    At this point, writing can commence. We start at the start at the given block/offset indicated by the fd.
    We write nbytes into the given block starting at the given offset using the given src. If we run to the end of a block, we ask for another block, update the inode and continue writing.
    If we run out of blocks return an error, we stop and return what we have. We finally return what was how many bytes were written.
    */
    UNUSED(fs);
    UNUSED(fd);
    UNUSED(src);
    UNUSED(nbyte);
    return 0;
}

int fs_remove(FS_t *fs, const char *path)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    next, check if path exists already (i.e. make sure file/dir already exists). Return an error if it doesn't exist
    Verify that if this is a directory, that its vacant file is set to 0 to indicate it is empty, return an error if it is not empty.
    We also need to verify that the link count is set to one, as we can't delete a file/dir that is referenced elsewhere.
    After this, we know we can delete. We first will close any open descriptors. This will involve going through all open descriptors, checking the inode #, and if it matches
    the inode number that we are about to delete, we close that fd.
    If this is a file, we first go through any direct pointer/indrect pointer and free those blocks that are referenced.
    We can then delete the given inode and return success.
    */
    UNUSED(fs);
    UNUSED(path);
    return 0;
}

dyn_array_t *fs_get_dir(FS_t *fs, const char *path)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    next, check if path exists already (i.e. make sure directory already exists). Return an error if it doesn't exist, or if the path leads to a file instead
    once at the point to inspect along the path, we get the inode of this given directory
    create the dyn_array with a max of 15 elems as specified, data size is the sizeof(file_record_t)
    At this point, we just look at all data pointers, as those will contain the information of the children of this directory.
    Once we finish scanning through all data pointers, or 15 have been scanned (hit max), we return the populated dyn_array_t.
    */
   //error check
    if(fs == NULL || path == NULL) {
        return NULL;
    }
    if(*path != '/') {
        //return error if path doesn't start with root directory.
        return NULL;
    }
    char lastchar = path[strlen(path)-1];
    if(lastchar == '/' && strlen(path) > 1) {
        //return error if path ends with trailing slash. Remove case when just entering root dir.
        return NULL;
    }
    //we need to break up the path into the separate pieces in between slashes. We know that for extended paths, the first couple will all be directories, and the last will be of type specified by type
    //store each path elem. in array of strings
    int number_of_path_elems = 0;
    //need to copy file path to get elements out since it is a const.
    char* copied_path = calloc(1,strlen(path) + 1);
    strcpy(copied_path,path);
    //temp elem to be used with strtok
    char* temp_elem;
    //using strtok to figure out how many elements there are
    temp_elem = strtok(copied_path,"/");
    while(temp_elem != NULL) {
        //loop to count # of elems in path
        number_of_path_elems++;
        //error check to make sure no element is larger than 127 characters.
        if(strlen(temp_elem) > 127) {
            free(copied_path);
            return NULL;
        }
        temp_elem = strtok(NULL,"/");
    }
    //array of strings to hold # of elemnents. Now need to loop back through and pull out individual path elements
    char ** path_elems = calloc(1,sizeof(char*) * number_of_path_elems);
    if(path_elems == NULL) {
        free(copied_path);
        copied_path = NULL;
        return NULL;
    }
    //get original path to loop through again
    strcpy(copied_path,path);
    temp_elem = strtok(copied_path, "/");
    for(int i = 0; i < number_of_path_elems; i++) {
        //we know that maximum file name is 127 characters, so only store that many.
        path_elems[i] = calloc(1,sizeof(char) * 127);
        if(path_elems[i] == NULL) {
            free(path_elems);
            free(copied_path);
            copied_path = NULL;
            return NULL;
        }
        //copy elements into path array
        strcpy(path_elems[i],temp_elem);
        temp_elem = strtok(NULL, "/");
    }

    free(copied_path);
    copied_path = NULL;
    //now need to traverse down the path until we reach the parent directory inode, as that is where this file will be referenced from
    //Start at root, we need to get it's details first
    size_t parent_inode_num = 0;
    inode_t* parent_inode = calloc(1,sizeof(inode_t));
    if(parent_inode == NULL) {
        //malloc failed, return error.
        free_str_array(path_elems, number_of_path_elems);
        return NULL;
    }
    //directory files occupy one block.
    directoryFile_t* parent_data = NULL;
    parent_data = calloc(1,BLOCK_SIZE_BYTES);
    if(parent_data == NULL) {
        //malloc failed, return error.
        free_str_array(path_elems, number_of_path_elems);
        free(parent_inode);
        parent_inode = NULL;
        return NULL;
    }
    block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);

    //we stop one before bc we want parent directory inode, this will make sure these directories exist
    for(int traverse_count = 0; traverse_count < number_of_path_elems -1; traverse_count++) {
        block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);
        //we know if this inode is a directory (as it should be, it should have flag setup). The directory should also not be vacant, as we are not creating directories along the given path.
        if(parent_inode->fileType  == 'd' && parent_inode->vacantFile != 0) {
            //read the data which should be stored in the first direct pointer block
            block_store_read(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
            //at this point, we have a parent inode, so we need to look through the inode directory file (sequential search on it until we have a hit. If path is invalid (i.e. couldn't find filename in data, return error))
            int file_counter = 0;
            for(file_counter = 0; file_counter < 31; file_counter++) {
                if(strcmp((parent_data + file_counter)->filename,path_elems[traverse_count])==0 && ((parent_inode->vacantFile>>file_counter) & 1)==1) {
                    parent_inode_num = (parent_data+file_counter)->inodeNumber;
                    break;
                }
            }
            //if we traverse all the way through the bitmap and can't find the file, then the given path does not exist, and we return an error.
            if(file_counter == 31) {
                free_str_array(path_elems,number_of_path_elems);
                free(parent_inode);
                free(parent_data);
                parent_inode = NULL;
                parent_data = NULL;
                return NULL;
            }
        }
        else {
            //uh oh, given parent inode is a file, not a directory, which is an error. Free and return error!
            free_str_array(path_elems,number_of_path_elems);
            free(parent_inode);
            free(parent_data);
            parent_inode = NULL;
            parent_data = NULL;
            return NULL;
        }
    }
    //we now can read parent inode
    block_store_inode_read(fs->BlockStore_inode,parent_inode_num,parent_inode);
    block_store_read(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
    size_t dir_inode_number;
    //find child file doing similar as above, sequentially searching through parent dir until we find given directory
    int parent_counter = 0;
    //if we're reading the root directory, we don't want to search for children, so just set the inode automatically to 0 since we know this.
    if(number_of_path_elems == 0) {
        dir_inode_number = 0;
    }
    else {
        for (parent_counter = 0; parent_counter < 31; parent_counter++) {
            if (strcmp((parent_data + parent_counter)->filename, path_elems[number_of_path_elems - 1]) == 0 &&
                ((parent_inode->vacantFile >> parent_counter) & 1) == 1) {
                //once we find file, copy its inode number so we can read its inode
                dir_inode_number = (parent_data + parent_counter)->inodeNumber;
                break;
            }
        }
        //if we traverse all the way through the bitmap and can't find the file, then the given dir does not exist, and we return an error.
        if (parent_counter == 31) {
            free_str_array(path_elems, number_of_path_elems);
            free(parent_inode);
            free(parent_data);
            parent_inode = NULL;
            parent_data = NULL;
            return NULL;
        }
    }
    inode_t* dir_inode = calloc(1, sizeof(inode_t));
    block_store_inode_read(fs->BlockStore_inode,dir_inode_number,dir_inode);
    if(dir_inode->fileType == 'r') {
        //uh oh, can't read directory file from regular file. 
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        free(dir_inode);
        dir_inode = NULL;
        parent_inode = NULL;
        parent_data = NULL;
        return NULL;
    }
    if(dir_inode->vacantFile == 0) {
        //if file is vacant, then we just return an empty dyn_array
        free_str_array(path_elems,number_of_path_elems);
        free(parent_inode);
        free(parent_data);
        free(dir_inode);
        dir_inode = NULL;
        parent_inode = NULL;
        parent_data = NULL;
        dyn_array_t* file_array = dyn_array_create(15,sizeof(file_record_t),NULL);
        return file_array;
    }
    //read directory data
    block_store_read(fs->BlockStore_whole,dir_inode->directPointer[0],parent_data);
    //now we go through the given dir and return all files in a dyn array.
    file_record_t* temp_file = calloc(1, sizeof(file_record_t));
    dyn_array_t* file_array = dyn_array_create(15,sizeof(file_record_t),NULL);
    inode_t* temp_inode = calloc(1,sizeof(inode_t));
    for(int file_counter = 0; file_counter < 31; file_counter++) {
        if(((dir_inode->vacantFile>>file_counter) & 1)==1) {
            //if bit occupied, then we should pull out that element.
            //copy directory element filename in
            strcpy(temp_file->name,(parent_data+file_counter)->filename);
            //read the inode at that element so we can extract the file type
            block_store_inode_read(fs->BlockStore_inode,(parent_data+file_counter)->inodeNumber,temp_inode);
            if(temp_inode->fileType == 'r') {
                //set filetype based on inode field
                temp_file->type = FS_REGULAR;
            }
            else {
                temp_file->type = FS_DIRECTORY;
            }
            //if dyn array is not full yet, we add the given element.
            if(dyn_array_size(file_array) < 15) {
                dyn_array_push_back(file_array,temp_file);
            }
        }
    }
    //free temp storage
    free_str_array(path_elems,number_of_path_elems);
    free(parent_inode);
    free(parent_data);
    free(dir_inode);
    free(temp_file);
    free(temp_inode);
    temp_inode = NULL;
    temp_file = NULL;
    dir_inode = NULL;
    parent_inode = NULL;
    parent_data = NULL;
    //return dyn array.
    return file_array;
}

int fs_move(FS_t *fs, const char *src, const char *dst)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    next, check if path exists already (i.e. make sure file already exists). Return an error if the src path doesn't exist, or if the destination path already exists
    If the given source does not lead to a file, we also return a error
    Now we can start the move. We need to update the parent inode of the src directory, and then the parent directory of the destination
    We will start by getting the file's current parent directory. We will get this inode, look through the vacant file & filename until we find the location within the directory struct
    At this point, we will set the vacant bit back to 0 to indicate it is open for writing, set the fields back to default, and now it can actually be moved.
    We then go into the destination and find the file's new parent. We go through the vacant field and find a open spot in the bitmap. If the bitmap is full, then we return an error.
    If there is an open spot, we update the fields to indicate the new inode # and file name
    We can finally return success.
    */
    UNUSED(fs);
    UNUSED(src);
    UNUSED(dst);
    return 0;
}

int fs_link(FS_t *fs, const char *src, const char *dst)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    next, check if path exists already (i.e. make sure file already exists). Return an error if the source doesn't exist
    We will then make sure that the destination is also a directory, if the given source is a directory, if they are different return an error.
    If both are directories, we will identify the source inode number, then go into the destination's inode, find space in the vacant bitmap & store the source directory's
    name and inode # to indicate a link. Return an error if the dst bitmap is full.
    If both are files, then process will be similar to above, except instead of going all the way to destination, the file will be stored in the destination's parent directory bitmap.
    The name and inode number will be copied to the parent inode's directory struct.
    We return success once this is done.
    */
    UNUSED(fs);
    UNUSED(src);
    UNUSED(dst);
    return 0;
}

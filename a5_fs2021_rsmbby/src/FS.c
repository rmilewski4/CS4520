#include "dyn_array.h"
#include "bitmap.h"
#include "block_store.h"
#include "FS.h"

#define BLOCK_STORE_NUM_BLOCKS 65536    // 2^16 blocks.
#define BLOCK_STORE_AVAIL_BLOCKS 65534  // Last 2 blocks consumed by the FBM
#define BLOCK_SIZE_BITS 32768           // 2^12 BYTES per block *2^3 BITS per BYTES
#define BLOCK_SIZE_BYTES 4096           // 2^12 BYTES per block
#define BLOCK_STORE_NUM_BYTES (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES)  // 2^16 blocks of 2^12 bytes.


// You might find this handy.  I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)

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


// check if the input filename is valid or not
bool isValidFileName(const char *filename)
{
    if(!filename || strlen(filename) == 0 || strlen(filename) > 127 || '/'==filename[0])
    {
        return false;
    }

    return true;
}



// use str_split to decompose the input string into filenames along the path, '/' as delimiter
char** str_split(char* a_str, const char a_delim, size_t * count)
{
    if(*a_str != '/')
    {
        return NULL;
    }
    char** result    = 0;
    char* tmp        = a_str;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = '\0';

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            (*count)++;
        }
        tmp++;
    }

    result = (char**)calloc(1, sizeof(char*) * (*count));
    for(size_t i = 0; i < (*count); i++)
    {
        *(result + i) = (char*)calloc(1, 200);
    }

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            strcpy(*(result + idx++), token);
            //    *(result + idx++) = strdup(token);
            token = strtok(NULL, delim);
        }

    }
    return result;
}



///
/// Creates a new file at the specified location
///   Directories along the path that do not exist are not created
/// \param fs The FS containing the file
/// \param path Absolute path to file to create
/// \param type Type of file to create (regular/directory)
/// \return 0 on success, < 0 on failure
///
int fs_create(FS_t *fs, const char *path, file_t type)
{
    if(fs != NULL && path != NULL && strlen(path) != 0 && (type == FS_REGULAR || type == FS_DIRECTORY))
    {
        char* copy_path = (char*)calloc(1, 65535);
        strcpy(copy_path, path);
        char** tokens;		// tokens are the directory names along the path. The last one is the name for the new file or dir
        size_t count = 0;
        tokens = str_split(copy_path, '/', &count);
        free(copy_path);
        if(tokens == NULL)
        {
            return -1;
        }

        // let's check if the filenames are valid or not
        for(size_t n = 0; n < count; n++)
        {	
            if(isValidFileName(*(tokens + n)) == false)
            {
                // before any return, we need to free tokens, otherwise memory leakage
                for (size_t i = 0; i < count; i++)
                {
                    free(*(tokens + i));
                }
                free(tokens);
                //printf("invalid filename\n");
                return -1;
            }
        }

        size_t parent_inode_ID = 0;	// start from the 1st inode, ie., the inode for root directory
        // first, let's find the parent dir
        size_t indicator = 0;

        // we declare parent_inode and parent_data here since it will still be used after the for loop
        directoryFile_t * parent_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);
        inode_t * parent_inode = (inode_t *) calloc(1, sizeof(inode_t));	

        for(size_t i = 0; i < count - 1; i++)
        {
            block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);	// read out the parent inode
            // in case file and dir has the same name
            if(parent_inode->fileType == 'd')
            {
                block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);

                for(int j = 0; j < folder_number_entries; j++)
                {
                    if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + i)) == 0 )
                    {
                        parent_inode_ID = (parent_data + j) -> inodeNumber;
                        indicator++;
                    }					
                }
            }					
        }
        //		printf("indicator = %zu\n", indicator);
        //		printf("parent_inode_ID = %lu\n", parent_inode_ID);

        // read out the parent inode
        block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);
        if(indicator == count - 1 && parent_inode->fileType == 'd')
        {
            // same file or dir name in the same path is intolerable
            for(int m = 0; m < folder_number_entries; m++)
            {
                // rid out the case of existing same file or dir name
                if( ((parent_inode->vacantFile >> m) & 1) == 1)
                {
                    // before read out parent_data, we need to make sure it does exist!
                    block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
                    if( strcmp((parent_data + m) -> filename, *(tokens + count - 1)) == 0 )
                    {
                        free(parent_data);
                        free(parent_inode);	
                        // before any return, we need to free tokens, otherwise memory leakage
                        for (size_t i = 0; i < count; i++)
                        {
                            free(*(tokens + i));
                        }
                        free(tokens);
                        //printf("filename already exists\n");
                        return -1;											
                    }
                }
            }	

            // cannot declare k inside for loop, since it will be used later.
            int k = 0;
            for( ; k < folder_number_entries; k++)
            {
                if( ((parent_inode->vacantFile >> k) & 1) == 0 )
                    break;
            }

            // if k == 0, then we have to declare a new parent data block
            //			printf("k = %d\n", k);
            if(k == 0)
            {
                size_t parent_data_ID = block_store_allocate(fs->BlockStore_whole);
                //					printf("parent_data_ID = %zu\n", parent_data_ID);
                if(parent_data_ID < BLOCK_STORE_AVAIL_BLOCKS)
                {
                    parent_inode->directPointer[0] = parent_data_ID;
                }
                else
                {
                    free(parent_inode);
                    free(parent_data);
                    // before any return, we need to free tokens, otherwise memory leakage
                    for (size_t i = 0; i < count; i++)
                    {
                        free(*(tokens + i));
                    }
                    free(tokens);
                    //printf("No available blocks\n");
                    return -1;												
                }
            }

            if(k < folder_number_entries)	// k == folder_number_entries means this directory is full
            {
                size_t child_inode_ID = block_store_sub_allocate(fs->BlockStore_inode);
                //printf("new child_inode_ID = %zu\n", child_inode_ID);
                // ugh, inodes are used up
                if(child_inode_ID == SIZE_MAX)
                {
                    free(parent_data);
                    free(parent_inode);
                    // before any return, we need to free tokens, otherwise memory leakage
                    for (size_t i = 0; i < count; i++)
                    {
                        free(*(tokens + i));
                    }
                    free(tokens);
                    //printf("could not allocate block for child\n");
                    return -1;	
                }

                // wow, at last, we make it!				
                // update the parent inode
                parent_inode->vacantFile |= (1 << k);
                // in the following cases, we should allocate parent data first: 
                // 1)the parent dir is not the root dir; 
                // 2)the file or dir to create is to be the 1st in the parent dir

                block_store_inode_write(fs->BlockStore_inode, parent_inode_ID, parent_inode);	

                // update the parent directory file block
                block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
                strcpy((parent_data + k)->filename, *(tokens + count - 1));
                //printf("the newly created file's name is: %s\n", (parent_data + k)->filename);
                (parent_data + k)->inodeNumber = child_inode_ID;
                block_store_write(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);

                // update the newly created inode
                inode_t * child_inode = (inode_t *) calloc(1, sizeof(inode_t));
                child_inode->vacantFile = 0;
                if(type == FS_REGULAR)
                {
                    child_inode->fileType = 'r';
                }
                else if(type == FS_DIRECTORY)
                {
                    child_inode->fileType = 'd';
                }	

                child_inode->inodeNumber = child_inode_ID;
                child_inode->fileSize = 0;
                child_inode->linkCount = 1;
                block_store_inode_write(fs->BlockStore_inode, child_inode_ID, child_inode);

                //printf("after creation, parent_inode->vacantFile = %d\n", parent_inode->vacantFile);



                // free the temp space
                free(parent_inode);
                free(parent_data);
                free(child_inode);
                // before any return, we need to free tokens, otherwise memory leakage
                for (size_t i = 0; i < count; i++)
                {
                    free(*(tokens + i));
                }
                free(tokens);					
                return 0;
            }				
        }
        // before any return, we need to free tokens, otherwise memory leakage
        for (size_t i = 0; i < count; i++)
        {
            free(*(tokens + i));
        }
        free(tokens); 
        free(parent_inode);	
        free(parent_data);
    }
    return -1;
}



///
/// Opens the specified file for use
///   R/W position is set to the beginning of the file (BOF)
///   Directories cannot be opened
/// \param fs The FS containing the file
/// \param path path to the requested file
/// \return file descriptor to the requested file, < 0 on error
///
int fs_open(FS_t *fs, const char *path)
{
    if(fs != NULL && path != NULL && strlen(path) != 0)
    {
        char* copy_path = (char*)calloc(1, 65535);
        strcpy(copy_path, path);
        char** tokens;		// tokens are the directory names along the path. The last one is the name for the new file or dir
        size_t count = 0;
        tokens = str_split(copy_path, '/', &count);
        free(copy_path);
        if(tokens == NULL)
        {
            return -1;
        }

        // let's check if the filenames are valid or not
        for(size_t n = 0; n < count; n++)
        {	
            if(isValidFileName(*(tokens + n)) == false)
            {
                // before any return, we need to free tokens, otherwise memory leakage
                for (size_t i = 0; i < count; i++)
                {
                    free(*(tokens + i));
                }
                free(tokens);
                return -1;
            }
        }	

        size_t parent_inode_ID = 0;	// start from the 1st inode, ie., the inode for root directory
        // first, let's find the parent dir
        size_t indicator = 0;

        inode_t * parent_inode = (inode_t *) calloc(1, sizeof(inode_t));
        directoryFile_t * parent_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);			

        // locate the file
        for(size_t i = 0; i < count; i++)
        {		
            block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);	// read out the parent inode
            if(parent_inode->fileType == 'd')
            {
                block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
                //printf("parent_inode->vacantFile = %d\n", parent_inode->vacantFile);
                for(int j = 0; j < folder_number_entries; j++)
                {
                    //printf("(parent_data + j) -> filename = %s\n", (parent_data + j) -> filename);
                    if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + i)) == 0 )
                    {
                        parent_inode_ID = (parent_data + j) -> inodeNumber;
                        indicator++;
                    }					
                }
            }					
        }		
        free(parent_data);			
        free(parent_inode);	
        //printf("indicator = %zu\n", indicator);
        //printf("count = %zu\n", count);
        // now let's open the file
        if(indicator == count)
        {
            size_t fd_ID = block_store_sub_allocate(fs->BlockStore_fd);
            //printf("fd_ID = %zu\n", fd_ID);
            // it could be possible that fd runs out
            if(fd_ID < number_fd)
            {
                size_t file_inode_ID = parent_inode_ID;
                inode_t * file_inode = (inode_t *) calloc(1, sizeof(inode_t));
                block_store_inode_read(fs->BlockStore_inode, file_inode_ID, file_inode);	// read out the file inode	

                // it's too bad if file to be opened is a dir 
                if(file_inode->fileType == 'd')
                {
                    free(file_inode);
                    // before any return, we need to free tokens, otherwise memory leakage
                    for (size_t i = 0; i < count; i++)
                    {
                        free(*(tokens + i));
                    }
                    free(tokens);
                    return -1;
                }

                // assign a file descriptor ID to the open behavior
                fileDescriptor_t * fd = (fileDescriptor_t *)calloc(1, sizeof(fileDescriptor_t));
                fd->inodeNum = file_inode_ID;
                fd->usage = 1;
                fd->locate_order = 0; // R/W position is set to the beginning of the file (BOF)
                fd->locate_offset = 0;
                block_store_fd_write(fs->BlockStore_fd, fd_ID, fd);

                free(file_inode);
                free(fd);
                // before any return, we need to free tokens, otherwise memory leakage
                for (size_t i = 0; i < count; i++)
                {
                    free(*(tokens + i));
                }
                free(tokens);			
                return fd_ID;
            }	
        }
        // before any return, we need to free tokens, otherwise memory leakage
        for (size_t i = 0; i < count; i++)
        {
            free(*(tokens + i));
        }
        free(tokens);
    }
    return -1;
}


///
/// Closes the given file descriptor
/// \param fs The FS containing the file
/// \param fd The file to close
/// \return 0 on success, < 0 on failure
///
int fs_close(FS_t *fs, int fd)
{
    if(fs != NULL && fd >=0 && fd < number_fd)
    {
        // first, make sure this fd is in use
        if(block_store_sub_test(fs->BlockStore_fd, fd))
        {
            block_store_sub_release(fs->BlockStore_fd, fd);
            return 0;
        }	
    }
    return -1;
}



///
/// Populates a dyn_array with information about the files in a directory
///   Array contains up to 31 file_record_t structures
/// \param fs The FS containing the file
/// \param path Absolute path to the directory to inspect
/// \return dyn_array of file records, NULL on error
///
dyn_array_t *fs_get_dir(FS_t *fs, const char *path)
{
    if(fs != NULL && path != NULL && strlen(path) != 0)
    {	
        char* copy_path = (char*)malloc(200);
        strcpy(copy_path, path);
        char** tokens;		// tokens are the directory names along the path. The last one is the name for the new file or dir
        size_t count = 0;
        tokens = str_split(copy_path, '/', &count);
        free(copy_path);

        if(strlen(*tokens) == 0)
        {
            // a spcial case: only a slash, no dir names
            count -= 1;
        }
        else
        {
            for(size_t n = 0; n < count; n++)
            {	
                if(isValidFileName(*(tokens + n)) == false)
                {
                    // before any return, we need to free tokens, otherwise memory leakage
                    for (size_t i = 0; i < count; i++)
                    {
                        free(*(tokens + i));
                    }
                    free(tokens);		
                    return NULL;
                }
            }			
        }		

        // search along the path and find the deepest dir
        size_t parent_inode_ID = 0;	// start from the 1st inode, ie., the inode for root directory
        // first, let's find the parent dir
        size_t indicator = 0;

        inode_t * parent_inode = (inode_t *) calloc(1, sizeof(inode_t));
        directoryFile_t * parent_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);
        for(size_t i = 0; i < count; i++)
        {
            block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);	// read out the parent inode
            // in case file and dir has the same name. But from the test cases we can see, this case would not happen
            if(parent_inode->fileType == 'd')
            {			
                block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
                for(int j = 0; j < folder_number_entries; j++)
                {
                    if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + i)) == 0 )
                    {
                        parent_inode_ID = (parent_data + j) -> inodeNumber;
                        indicator++;
                    }					
                }	
            }					
        }	
        free(parent_data);
        free(parent_inode);	

        // now let's enumerate the files/dir in it
        if(indicator == count)
        {
            inode_t * dir_inode = (inode_t *) calloc(1, sizeof(inode_t));
            block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, dir_inode);	// read out the file inode			
            if(dir_inode->fileType == 'd')
            {
                // prepare the data to be read out
                directoryFile_t * dir_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);
                block_store_read(fs->BlockStore_whole, dir_inode->directPointer[0], dir_data);

                // prepare the dyn_array to hold the data
                dyn_array_t * dynArray = dyn_array_create(folder_number_entries, sizeof(file_record_t), NULL);

                for(int j = 0; j < folder_number_entries; j++)
                {
                    if( ((dir_inode->vacantFile >> j) & 1) == 1 )
                    {
                        file_record_t* fileRec = (file_record_t *)calloc(1, sizeof(file_record_t));
                        strcpy(fileRec->name, (dir_data + j) -> filename);

                        // to know fileType of the member in this dir, we have to refer to its inode
                        inode_t * member_inode = (inode_t *) calloc(1, sizeof(inode_t));
                        block_store_inode_read(fs->BlockStore_inode, (dir_data + j) -> inodeNumber, member_inode);
                        if(member_inode->fileType == 'd')
                        {
                            fileRec->type = FS_DIRECTORY;
                        }
                        else if(member_inode->fileType == 'f')
                        {
                            fileRec->type = FS_REGULAR;
                        }

                        // now insert the file record into the dyn_array
                        dyn_array_push_front(dynArray, fileRec);
                        free(fileRec);
                        free(member_inode);
                    }					
                }
                free(dir_data);
                free(dir_inode);
                // before any return, we need to free tokens, otherwise memory leakage
                if(strlen(*tokens) == 0)
                {
                    // a spcial case: only a slash, no dir names
                    count += 1;
                }
                for (size_t i = 0; i < count; i++)
                {
                    free(*(tokens + i));
                }
                free(tokens);	
                return(dynArray);
            }
            free(dir_inode);
        }
        // before any return, we need to free tokens, otherwise memory leakage
        if(strlen(*tokens) == 0)
        {
            // a spcial case: only a slash, no dir names
            count += 1;
        }
        for (size_t i = 0; i < count; i++)
        {
            free(*(tokens + i));
        }
        free(tokens);	
    }
    return NULL;
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
    //error check fs
    if(fs == NULL){
        return -1;
    }
    //make sure we have valid fd
    if(!block_store_sub_test(fs->BlockStore_fd, fd))
    {
        return -1;
    }
    //pull down file descriptor based on num given
    fileDescriptor_t* fileDescr = calloc(1,sizeof(fileDescriptor_t));
    if(fileDescr == NULL ) {
        return -1;
    }
    size_t fd_bytes_read = block_store_fd_read(fs->BlockStore_fd,fd,fileDescr);
    if(fd_bytes_read != sizeof(fileDescriptor_t) || fileDescr->inodeNum == 0) {
        //if we read less than the # of bytes or the inode # is 0 (which should never happen), then we must have been given invalid fd.
        free(fileDescr);
        return -1;
    }
    inode_t* fileInode = calloc(1, sizeof(inode_t));
    if(fileInode == NULL) {
        free(fileDescr);
        return -1;
    }
    //get inode we are writing to.
    block_store_inode_read(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
    if(offset == 0 && whence == FS_SEEK_CUR) {
        //if no offset, just get current offset to return.
        off_t current = fileDescr->locate_order*BLOCK_SIZE_BYTES + fileDescr->locate_offset;
        free(fileDescr);
        free(fileInode);
        return current;
    }
    if(whence == FS_SEEK_CUR) {
        if(offset < 0) {
            //we need to make sure we are not going before BOF if this is the case.
            off_t pos_offset = offset * -1;
            uint16_t numblocks = pos_offset / BLOCK_SIZE_BYTES;
            if(fileDescr->usage == 1) {
                if(numblocks - fileDescr->locate_order < 0) {
                    //going past beginning so set to 0 for each
                    fileDescr->usage = 1;
                    fileDescr->locate_order = 0;
                    fileDescr->locate_offset = 0;
                    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                    free(fileDescr);
                    free(fileInode);
                    return 0;
                }
                else if(fileDescr->locate_order == 0 && fileDescr->locate_offset - (pos_offset % BLOCK_SIZE_BYTES) < 0) {
                    //going past beginning
                    fileDescr->usage = 1;
                    fileDescr->locate_order = 0;
                    fileDescr->locate_offset = 0;
                    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                    free(fileDescr);
                    free(fileInode);
                    return 0;
                }
            }
            else if(fileDescr->usage == 2) {
                if(numblocks - (fileDescr->locate_order + 6) < 0) {
                    fileDescr->usage = 1;
                    fileDescr->locate_order = 0;
                    fileDescr->locate_offset = 0;
                    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                    free(fileDescr);
                    free(fileInode);
                    return 0;
                }
            }
            else {
                if(numblocks - (fileDescr->locate_order + 6) < 0) {
                    fileDescr->usage = 1;
                    fileDescr->locate_order = 0;
                    fileDescr->locate_offset = 0;
                    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                    free(fileDescr);
                    free(fileInode);
                    return 0;
                }
            }
            fileDescr->locate_order -= numblocks;
            fileDescr->locate_offset -= (pos_offset % BLOCK_SIZE_BYTES);
            if(fileDescr->locate_order > UINT16_MAX - BLOCK_SIZE_BYTES && fileDescr->usage > 1) {
                if(fileDescr->usage == 2) {
                    fileDescr->usage = 1;
                }
                else {
                    fileDescr->usage = 2;
                }
                fileDescr->locate_order = 2048;
            }
            //if we wrap around due to negative overflow, then...
            if(fileDescr->locate_offset > UINT16_MAX - BLOCK_SIZE_BYTES) {
                //smaller than a block, so decrease block count by one and set to end of that block.
                uint16_t offset_to_subtract = fileDescr->locate_offset * -1;
                fileDescr->locate_order--;
                fileDescr->locate_offset = BLOCK_SIZE_BYTES - offset_to_subtract;
            }
        }
        else {
            //going forward in file from current pos.
            uint16_t numblocks = offset / BLOCK_SIZE_BYTES;
            fileDescr->locate_order += numblocks;
            fileDescr->locate_offset += (offset % BLOCK_SIZE_BYTES);
            if(fileDescr->locate_order > 2048 && fileDescr->usage < 4) {
                if(fileDescr->usage == 2) {
                    fileDescr->usage = 4;
                    fileDescr->locate_order -= 2048 + 6;
                }
                else {
                    fileDescr->usage = 2;
                    fileDescr->locate_order -= 6;
                }
                if(fileDescr->locate_order > 2048 + 6  && fileDescr->usage == 2) {
                    fileDescr->usage = 4;
                    fileDescr->locate_order -= 2048;
                }
            }
            if(fileDescr->locate_offset > BLOCK_SIZE_BYTES) {
                fileDescr->locate_order++;
                fileDescr->locate_offset -= BLOCK_SIZE_BYTES;
            }
        }
    }
    else if(whence == FS_SEEK_SET) {
        if(offset < 0) {
            //can't have negative offset when setting, as we will go before BOF.
            free(fileDescr);
            free(fileInode);
            return 0;
        }
        fileDescr->locate_offset = offset%BLOCK_SIZE_BYTES;
        if(offset / BLOCK_SIZE_BYTES < 6) {
            //we are within direct pointer
            fileDescr->usage = 1;
            fileDescr->locate_order = offset/BLOCK_SIZE_BYTES;
        }
        else if(offset / BLOCK_SIZE_BYTES < 2048 + 6) {
            //within indirect
            fileDescr->usage = 2;
            fileDescr->locate_order = (offset/BLOCK_SIZE_BYTES) - 6;
        }
        else {
            //within double indirect
            fileDescr->usage = 4;
            fileDescr->locate_order = (offset/BLOCK_SIZE_BYTES) - 6;

        }
    }
    else if(whence == FS_SEEK_END) {
        if(offset > 0) {
            //can't go past eof
            free(fileDescr);
            free(fileInode);
            return 0;
        }
        off_t pos_offset = offset * -1;
        uint16_t numblocks = pos_offset / BLOCK_SIZE_BYTES;
        fileDescr->locate_order -= numblocks;
        fileDescr->locate_offset -= (pos_offset % BLOCK_SIZE_BYTES);
        if(fileDescr->locate_order > UINT16_MAX - BLOCK_SIZE_BYTES && fileDescr->usage > 1) {
            if(fileDescr->usage == 2) {
                fileDescr->usage = 1;
            }
            else {
                fileDescr->usage = 2;
            }
            fileDescr->locate_order = 2048;
        }
        //if we wrap around due to negative overflow, then...
        if(fileDescr->locate_offset > UINT16_MAX - BLOCK_SIZE_BYTES) {
            //smaller than a block, so decrease block count by one and set to end of that block.
            uint16_t offset_to_subtract = fileDescr->locate_offset * -1;
            fileDescr->locate_order--;
            fileDescr->locate_offset = BLOCK_SIZE_BYTES - offset_to_subtract;
        }
    }
    else {
        //invalid whence, free & return error.
        free(fileDescr);
        free(fileInode);
        return -1;
    }
    //need to see if we went past EOF so set the offset to -1 to the most recent writable bit
    if(fileDescr->usage == 4 && (fileDescr->locate_order+6+2048) > 65000) {
        //very close to EOF, so decreement by 1.
        fileDescr->locate_order--;
        fileDescr->locate_offset = BLOCK_SIZE_BYTES - 1;
    }
    //write back file descr when done
    off_t from_beginning = 0;
    if(fileDescr->usage == 1){
        from_beginning = fileDescr->locate_order*BLOCK_SIZE_BYTES + fileDescr->locate_offset;
    }
    else if(fileDescr->usage == 2) {
        from_beginning = (fileDescr->locate_order+6)*BLOCK_SIZE_BYTES + fileDescr->locate_offset;
    }
    else {
        from_beginning = (fileDescr->locate_order+6+2048)*BLOCK_SIZE_BYTES + fileDescr->locate_offset;
    }
    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
    free(fileDescr);
    free(fileInode);
    return from_beginning;
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
    //error check parameters
    if(fs == NULL || dst == NULL) {
        return -1;
    }
    //check and make sure the fd is valid
    if(!block_store_sub_test(fs->BlockStore_fd, fd))
    {
        return -1;
    }
    //pull down file descriptor based on num given
    fileDescriptor_t* fileDescr = calloc(1,sizeof(fileDescriptor_t));
    if(fileDescr == NULL ) {
        return -1;
    }
    size_t fd_bytes_read = block_store_fd_read(fs->BlockStore_fd,fd,fileDescr);
    if(fd_bytes_read != sizeof(fileDescriptor_t) || fileDescr->inodeNum == 0) {
        //if we read less than the # of bytes or the inode # is 0 (which should never happen), then we must have been given invalid fd.
        free(fileDescr);
        return -1;
    }
    inode_t* fileInode = calloc(1, sizeof(inode_t));
    if(fileInode == NULL) {
        free(fileDescr);
        return -1;
    }
    //get inode we are writing to.
    block_store_inode_read(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
    void* current_block = calloc(1,BLOCK_SIZE_BYTES);
    size_t bytes_read = 0;
    for(bytes_read = 0; bytes_read != nbyte;) {
        uint16_t block_index = 0;
        block_index = fileDescr->locate_order;
        if(fileDescr->usage == 1) {
            //using direct
            if(fileInode->directPointer[block_index] == 0) {
                //can't read empty block, error somewhere, so indicate that.
                free(fileDescr);
                free(fileInode);
                free(current_block);
                return -1;
            }
            block_store_read(fs->BlockStore_whole,fileInode->directPointer[block_index],current_block);
        }
        else if(fileDescr->usage == 2) {
            if(fileInode->indirectPointer[0] == 0) {
                //can't read empty block, error somewhere, so indicate that.
                free(fileDescr);
                free(fileInode);
                free(current_block);
                return -1;
            }
            uint16_t indirectArr[2048] = {0};
            block_store_read(fs->BlockStore_whole,fileInode->indirectPointer[0],indirectArr);
            block_store_read(fs->BlockStore_whole,indirectArr[block_index],current_block);
        }
        else {
            //double indirect
            if(fileInode->doubleIndirectPointer == 0) {
                //can't read empty block, error somewhere, so indicate that.
                free(fileDescr);
                free(fileInode);
                free(current_block);
                return -1;
            }
            uint16_t doubleIndirectArr[2048] = {0};
            block_store_read(fs->BlockStore_whole,fileInode->indirectPointer[0],doubleIndirectArr);
            uint16_t indirectArr[2048] = {0};
            //not sure if this is right...
            block_store_read(fs->BlockStore_whole,block_index / 2048,indirectArr);
            block_store_read(fs->BlockStore_whole,indirectArr[block_index % 2048], current_block);
        }
        if(fileDescr->locate_offset != 0) {
            //cursor within a block, so let's just scan to the end of the block
            uint16_t loc = nbyte - bytes_read;
            if(loc < (BLOCK_SIZE_BYTES - fileDescr->locate_offset)) {
                //staying within current block this read.
                memcpy((bytes_read + dst),(current_block + fileDescr->locate_offset),nbyte-bytes_read);
                fileDescr->locate_offset = nbyte-bytes_read;
                bytes_read += nbyte-bytes_read;
            }
            else {
                //reading to end of block
                size_t bytes_to_read_this_iter = BLOCK_SIZE_BYTES - fileDescr->locate_offset;
                memcpy((bytes_read + dst), (current_block + fileDescr->locate_offset), bytes_to_read_this_iter);
                bytes_read += bytes_to_read_this_iter;
                //now need to increment locate_order & set locate_offset to 0
                fileDescr->locate_order++;
                fileDescr->locate_offset = 0;
            }
        }
        else {
            //cursor at start of block, so let's read to tne end of the block, or if we're almost done, we read to nbyte
            if(nbyte - bytes_read < BLOCK_SIZE_BYTES) {
                //reading less than a block
                memcpy((bytes_read+ dst),(current_block),nbyte-bytes_read);
                fileDescr->locate_offset = nbyte-bytes_read;
                bytes_read += nbyte-bytes_read;
            }
            else {
                //reading a block
                memcpy((bytes_read+dst),current_block,BLOCK_SIZE_BYTES);
                fileDescr->locate_order++;
                bytes_read += BLOCK_SIZE_BYTES;
            }
        }
        if(fileDescr->locate_order == 6 && fileDescr->usage == 1) {
            //need to go up to indirect blocks
            fileDescr->locate_order = 0;
            fileDescr->usage = 2;
        }
        else if(fileDescr->locate_order == 2048 && fileDescr->usage == 2) {
            //go up to double indirect
            fileDescr->locate_order = 0;
            fileDescr->usage = 4;
        }
    }
    //now just update fileDescr & free temp space
    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
    free(fileInode);
    free(fileDescr);
    free(current_block);
    return bytes_read;
}
size_t findFirstDoubleOpen(uint16_t* doubleIndirectPtrArr) {
    for(int i = 0; i < 2048; i++) {
        //we will run out of blocks before we hit the end of this array, so its okay to check i+1
        if(doubleIndirectPtrArr[i+1]==0) {
            return i;
        }
    }
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
    //error check parameters
    if(fs == NULL || src == NULL) {
        return -1;
    }
    //check and make sure the fd is valid
    if(!block_store_sub_test(fs->BlockStore_fd, fd))
    {
        return -1;
    }
    //pull down file descriptor based on num given
    fileDescriptor_t* fileDescr = calloc(1,sizeof(fileDescriptor_t));
    if(fileDescr == NULL ) {
        return -1;
    }
    size_t fd_bytes_read = block_store_fd_read(fs->BlockStore_fd,fd,fileDescr);
    if(fd_bytes_read != sizeof(fileDescriptor_t) || fileDescr->inodeNum == 0) {
        //if we read less than the # of bytes or the inode # is 0 (which should never happen), then we must have been given invalid fd.
        free(fileDescr);
        return -1;
    }
    inode_t* fileInode = calloc(1, sizeof(inode_t));
    if(fileInode == NULL) {
        free(fileDescr);
        return -1;
    }
    //get inode we are writing to.
    block_store_inode_read(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);

    //array for double indirect, holding pointers to indirect block & indirect array
    //uint16_t doubleIndirectPtrArr[2048] = {0};
    uint16_t indirectPtrArr[2048] = {0};
    if(nbyte == 0) {
        //if we aren't writing at all, just return at this point. Don't need to update anything but overwrite inode just in case.
        block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
        free(fileInode);
        free(fileDescr);
        return 0;
    }
    //get temp buffer that allows each block's data to be written individually
    void* tempBuffer = calloc(1, BLOCK_SIZE_BYTES);
    //pointer blocks setup now if needed, now we just loop through data and write to it
    //at this point, we passed all error checking, so if we allocate a block and it fails, we are out of space, so return bytes written.
    size_t bytes_written = 0;
    for(bytes_written = 0; bytes_written != nbyte;) {
        //if offset is already advanced into current block, then we might not need another block
        if(fileDescr->locate_offset == 0) {
            //need new block allocated since offset is 0
            size_t block_num = 0;
            if(fileDescr->usage ==1) {
                //we know we are still using direct ptr blocks, so find open block.
                int count;
                for(count = 0; count < 6; count++) {
                    if(fileInode->directPointer[count] == 0) {
                        break;
                    }
                }
                //found space in direct ptr
                block_num = block_store_allocate(fs->BlockStore_whole);
                fileInode->directPointer[count] = block_num;
                if (block_num == SIZE_MAX) {
                    //uh oh error, ran out of blocks, so free everything, and write back what was done so far.
                    //write updated inode back to bs
                    block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
                    free(fileInode);
                    fileInode = NULL;
                    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                    free(fileDescr);
                    free(tempBuffer);
                    return bytes_written;
                }

            }
            else if(fileDescr->usage == 2) {
                //using indirect
                block_store_read(fs->BlockStore_whole,fileInode->indirectPointer[0],indirectPtrArr);
                int indirect_block_array_num = 0;
                for(indirect_block_array_num = 0; indirect_block_array_num < 2048; indirect_block_array_num++) {
                    //look for open spot in indirectArr
                    if(indirectPtrArr[indirect_block_array_num] == 0) {
                        break;
                    }
                }
                    block_num = block_store_allocate(fs->BlockStore_whole);
                    if(block_num == SIZE_MAX) {
                        //write updated inode back to bs
                        //uh oh error, ran out of blocks, so free everything, and write back what was done so far.
                        block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
                        free(fileInode);
                        fileInode = NULL;
                        block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                        free(fileDescr);
                        free(tempBuffer);
                        return bytes_written;
                    }
                    //set pointer in array, write back to block
                    indirectPtrArr[indirect_block_array_num] = block_num;
                    block_store_write(fs->BlockStore_whole,fileInode->indirectPointer[0],indirectPtrArr);
            }
            else {
                //writing to double indirect
                uint16_t doubleIndirectArr[2048];
                block_store_read(fs->BlockStore_whole,fileInode->doubleIndirectPointer,doubleIndirectArr);
                int double_indirect_block_array_num = 0;
                for(double_indirect_block_array_num = 0; double_indirect_block_array_num < 2048; double_indirect_block_array_num++) {
                    //look for edge occupied spot in double_indirectArr
                    if(doubleIndirectArr[double_indirect_block_array_num] != 0 && doubleIndirectArr[double_indirect_block_array_num+1] == 0) {
                        break;
                    }
                }
                //now read given indirect at block num
                block_store_read(fs->BlockStore_whole,doubleIndirectArr[double_indirect_block_array_num],indirectPtrArr);
                int indirect_block_array_num = 0;
                for(indirect_block_array_num = 0; indirect_block_array_num < 2048; indirect_block_array_num++) {
                    //look for open spot in indirectArr
                    if(indirectPtrArr[indirect_block_array_num] == 0) {
                        break;
                    }
                }
                if(indirect_block_array_num == 2048) {
                    //out of space in given block, so let's allocate space in the next
                    size_t next_block = block_store_allocate(fs->BlockStore_whole);
                    uint16_t* blank_indirect = calloc(2048,sizeof(uint16_t));
                    if(next_block == SIZE_MAX) {
                        //write updated inode back to bs
                        //uh oh error, ran out of blocks, so free everything, and write back what was done so far.
                        block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
                        free(fileInode);
                        fileInode = NULL;
                        block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                        free(fileDescr);
                        free(tempBuffer);
                        return bytes_written;
                    }
                    doubleIndirectArr[double_indirect_block_array_num+1] = next_block;
                    //write back changes
                    block_store_write(fs->BlockStore_whole,fileInode->doubleIndirectPointer,doubleIndirectArr);
                    block_store_write(fs->BlockStore_whole,next_block,blank_indirect);
                    free(blank_indirect);
                    //restart this loop iteration
                    continue;
                }
                //we found space, so lets allocate it.
                block_num = block_store_allocate(fs->BlockStore_whole);
                if(block_num == SIZE_MAX) {
                    //write updated inode back to bs
                    //uh oh error, ran out of blocks, so free everything, and write back what was done so far.
                    block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
                    free(fileInode);
                    fileInode = NULL;
                    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                    free(fileDescr);
                    free(tempBuffer);
                    return bytes_written;
                }
                //set pointer in array, write back to block
                indirectPtrArr[indirect_block_array_num] = block_num;
                block_store_write(fs->BlockStore_whole,doubleIndirectArr[double_indirect_block_array_num],indirectPtrArr);
            }
            //actually write to block
            if(nbyte - bytes_written < BLOCK_SIZE_BYTES) {
                //writing to less than block
                memcpy(tempBuffer,(src+bytes_written),nbyte-bytes_written);
                //now update fd
                fileDescr->locate_offset += nbyte-bytes_written;
                bytes_written += nbyte-bytes_written;
            }
            else {
                //writing to whole block
                memcpy(tempBuffer,(src+bytes_written),BLOCK_SIZE_BYTES);
                fileDescr->locate_order++;
                bytes_written += BLOCK_SIZE_BYTES;
            }
            //actually physically write to given block
            block_store_write(fs->BlockStore_whole, block_num, tempBuffer);
        }
        else {
            //lets just write as much as we can to this existing block based on offset
            void* current_block = calloc(1,BLOCK_SIZE_BYTES);
            uint16_t block_index = 0;
            uint16_t block_id = 0;
            block_index = fileDescr->locate_order;
            if(fileDescr->usage == 1) {
                //using direct
                if(fileInode->directPointer[block_index] == 0) {
                    //can't read empty block, error somewhere, so indicate that.
                    free(fileDescr);
                    free(fileInode);
                    free(current_block);
                    return -1;
                }
                block_id = fileInode->directPointer[block_index];
                block_store_read(fs->BlockStore_whole,fileInode->directPointer[block_index],current_block);
            }
            else if(fileDescr->usage == 2) {
                if(fileInode->indirectPointer[0] == 0) {
                    //can't read empty block, error somewhere, so indicate that.
                    free(fileDescr);
                    free(fileInode);
                    free(current_block);
                    return -1;
                }
                uint16_t indirectArr[2048] = {0};
                block_store_read(fs->BlockStore_whole,fileInode->indirectPointer[0],indirectArr);
                block_id = indirectArr[block_index];
                block_store_read(fs->BlockStore_whole,indirectArr[block_index],current_block);
            }
            else {
                //double indirect
                if(fileInode->doubleIndirectPointer == 0) {
                    //can't read empty block, error somewhere, so indicate that.
                    free(fileDescr);
                    free(fileInode);
                    free(current_block);
                    return -1;
                }
                uint16_t doubleIndirectArr[2048] = {0};
                block_store_read(fs->BlockStore_whole,fileInode->indirectPointer[0],doubleIndirectArr);
                uint16_t indirectArr[2048] = {0};
                block_store_read(fs->BlockStore_whole,block_index / 2048,indirectArr);
                block_id = indirectArr[block_index % 2048];
                block_store_read(fs->BlockStore_whole,indirectArr[block_index % 2048], current_block);
            }
            //we have current block, lets just write data all the way to the end of it.
            uint16_t loc = nbyte - bytes_written;
            if(loc < (BLOCK_SIZE_BYTES - fileDescr->locate_offset)) {
                //staying within current block this write.
                memcpy( (current_block + fileDescr->locate_offset),(bytes_written + src),nbyte-bytes_written);
                fileDescr->locate_offset = nbyte-bytes_written;
                bytes_written += nbyte-bytes_written;
            }
            else {
                //writing to end of block
                size_t bytes_to_write_this_iter = BLOCK_SIZE_BYTES - fileDescr->locate_offset;
                memcpy( (current_block + fileDescr->locate_offset),(bytes_written + src),bytes_to_write_this_iter);
                bytes_written += bytes_to_write_this_iter;
                //now need to increment locate_order & set locate_offset to 0
                fileDescr->locate_order++;
                fileDescr->locate_offset = 0;
            }
            //write back block
            block_store_write(fs->BlockStore_whole,block_id,current_block);
            free(current_block);
        }
        if(fileDescr->locate_order == 6 && fileDescr->usage == 1) {
            //need to go up to indirect blocks, so let's allocate space for indirect
            uint16_t blank_indirect[2048] = {0};
            size_t indirect_block = block_store_allocate(fs->BlockStore_whole);
            if(indirect_block == SIZE_MAX) {
                //write updated inode back to bs
                //uh oh error, ran out of blocks, so free everything, and write back what was done so far.
                block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
                free(fileInode);
                fileInode = NULL;
                block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                free(fileDescr);
                free(tempBuffer);
                return bytes_written;
            }
            fileInode->indirectPointer[0] = indirect_block;
            block_store_write(fs->BlockStore_whole,indirect_block,blank_indirect);
            fileDescr->locate_order = 0;
            fileDescr->usage = 2;
        }
        else if(fileDescr->locate_order == 2048 && fileDescr->usage == 2) {
            //go up to double indirect
            //need to go up to indirect blocks, so let's allocate space for indirect
            uint16_t blank_double_indirect[2048] = {0};
            size_t double_indirect_block = block_store_allocate(fs->BlockStore_whole);
            if(double_indirect_block == SIZE_MAX) {
                //write updated inode back to bs
                //uh oh error, ran out of blocks, so free everything, and write back what was done so far.
                block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
                free(fileInode);
                fileInode = NULL;
                block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                free(fileDescr);
                free(tempBuffer);
                return bytes_written;
            }
            fileInode->doubleIndirectPointer = double_indirect_block;
            //allocate space for indirect block as well
            uint16_t blank_indirect[2048] = {0};
            size_t indirect_block = block_store_allocate(fs->BlockStore_whole);
            if(indirect_block == SIZE_MAX) {
                //write updated inode back to bs
                //uh oh error, ran out of blocks, so free everything, and write back what was done so far.
                block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
                free(fileInode);
                fileInode = NULL;
                block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
                free(fileDescr);
                free(tempBuffer);
                return bytes_written;
            }
            blank_double_indirect[0] = indirect_block;
            block_store_write(fs->BlockStore_whole,indirect_block,blank_indirect);
            block_store_write(fs->BlockStore_whole,double_indirect_block,blank_double_indirect);
            fileDescr->locate_order = 0;
            fileDescr->usage = 4;
        }
    }

    //wrote everything back, so we can update everything and return how many bytes we wrote.
    //write updated inode back to bs
    block_store_inode_write(fs->BlockStore_inode,fileDescr->inodeNum,fileInode);
    free(fileInode);
    fileInode = NULL;
    //fileDescr->locate_order = numOfBlocksToWrite;
    //fileDescr->locate_offset = nbyte % BLOCK_SIZE_BYTES;
    block_store_fd_write(fs->BlockStore_fd,fd,fileDescr);
    free(fileDescr);
    free(tempBuffer);
    return bytes_written;
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
    if(fs == NULL || path == NULL) {
        return -1;
    }
    char* copy_path = (char*)calloc(1, 65535);
    strcpy(copy_path, path);
    char** tokens;		// tokens are the directory names along the path. The last one is the name for the new file or dir
    size_t count = 0;
    tokens = str_split(copy_path, '/', &count);
    free(copy_path);
    if(tokens == NULL)
    {
        return -1;
    }

    // let's check if the filenames are valid or not
    for(size_t n = 0; n < count; n++)
    {
        if(isValidFileName(*(tokens + n)) == false)
        {
            // before any return, we need to free tokens, otherwise memory leakage
            for (size_t i = 0; i < count; i++)
            {
                free(*(tokens + i));
            }
            free(tokens);
            return -1;
        }
    }

    size_t parent_inode_ID = 0;	// start from the 1st inode, ie., the inode for root directory
    // first, let's find the parent dir
    size_t indicator = 0;

    inode_t * parent_inode = (inode_t *) calloc(1, sizeof(inode_t));
    directoryFile_t * parent_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);

    // locate the file
    for(size_t i = 0; i < count; i++)
    {
        block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);	// read out the parent inode
        if(parent_inode->fileType == 'd')
        {
            block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
            //printf("parent_inode->vacantFile = %d\n", parent_inode->vacantFile);
            for(int j = 0; j < folder_number_entries; j++)
            {
                //printf("(parent_data + j) -> filename = %s\n", (parent_data + j) -> filename);
                if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + i)) == 0 )
                {
                    parent_inode_ID = (parent_data + j) -> inodeNumber;
                    indicator++;
                }
            }
        }
    }
    //if below is not true, file path does not exist.
    if(indicator==count) {
        size_t child_inode_ID = parent_inode_ID;
        inode_t * child_inode = (inode_t *) calloc(1, sizeof(inode_t));
        block_store_inode_read(fs->BlockStore_inode, child_inode_ID, child_inode);	// read out the child inode
        if(child_inode->fileType == 'd') {
            //if directory, verify empty by checking vacancy, can also confirm by checking if direct pointer is set for file
            if(child_inode->vacantFile == 0) {
                for(int j = 0; j < folder_number_entries; j++)
                {
                    //printf("(parent_data + j) -> filename = %s\n", (parent_data + j) -> filename);
                    if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + (count-1))) == 0 )
                    {
                        //found entry in vacant file, flip that bit in parent vacancy to indicate it is now available to be used. Now we just need to free the child block
                        uint32_t currentVacantFile = (1 << j);
                        currentVacantFile ^= UINT32_MAX;
                        parent_inode->vacantFile &= currentVacantFile;
                        //we are going to clear the parent data file as well to be safe...
                        memset((parent_data+j)->filename,0,127);
                        (parent_data+j)->inodeNumber = 0;
                        block_store_write(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
                        //write back parent inode to indicate updates to its vacant file
                        block_store_inode_write(fs->BlockStore_inode,parent_inode->inodeNumber,parent_inode);
                        //finally we can free the child block & all associated data. If it was set to vacant, it still might have a directory file, so check for that, otherwise, all pointers should not be set
                        if(child_inode->directPointer[0] != 0) {
                            block_store_release(fs->BlockStore_whole,child_inode->directPointer[0]);
                        }
                        //block should now be empty, so we can free it.
                        block_store_sub_release(fs->BlockStore_inode,child_inode_ID);
                        //now we can finish & return
                        free(parent_inode);
                        free(child_inode);
                        // before any return, we need to free tokens, otherwise memory leakage
                        for (size_t i = 0; i < count; i++)
                        {
                            free(*(tokens + i));
                        }
                        free(tokens);
                        free(parent_data);
                        return 0;
                    }
                }
            }
            else {
                //file not vacant, so can't be removed free everything and indicate error.
                free(parent_inode);
                free(child_inode);
                // before any return, we need to free tokens, otherwise memory leakage
                for (size_t i = 0; i < count; i++)
                {
                    free(*(tokens + i));
                }
                free(tokens);
                free(parent_data);
                return -1;
            }
        }
        else {
            //dealing with file then...
            //since it's a file, we need to go through all pointers & free all associated data back.
            //start w/ direct pointers.
            for(int direct_count = 0; direct_count < 6; direct_count++) {
                if(child_inode->directPointer[direct_count] != 0) {
                    block_store_release(fs->BlockStore_whole,child_inode->directPointer[direct_count]);
                }
            }
            //next go through any indirects...
            if(child_inode->indirectPointer[0] != 0) {
                uint16_t indirectArr[2048];
                block_store_read(fs->BlockStore_whole,child_inode->indirectPointer[0],indirectArr);
                for(int indirect_count = 0; indirect_count < 2048; indirect_count++) {
                    if(indirectArr[indirect_count] != 0) {
                        //if indirect was found in use, release that thing
                        block_store_release(fs->BlockStore_whole,indirectArr[indirect_count]);
                    }
                }
            }
            //finally go through the double indirects
            if(child_inode->doubleIndirectPointer != 0) {
                uint16_t doubleIndirectArr[2048];
                block_store_read(fs->BlockStore_whole,child_inode->doubleIndirectPointer,doubleIndirectArr);
                for(int double_indirect_count = 0; double_indirect_count < 2048; double_indirect_count++) {
                    //now check each individual double indirect for use
                    if(doubleIndirectArr[double_indirect_count] != 0) {
                        //in use, so pull it out
                        uint16_t indirectArr[2048];
                        block_store_read(fs->BlockStore_whole,doubleIndirectArr[double_indirect_count],indirectArr);
                        for(int indirect = 0; indirect < 2048; indirect++) {
                            if(indirectArr[indirect] != 0) {
                                //found a block in use, so free
                                block_store_release(fs->BlockStore_whole,indirectArr[indirect]);
                            }
                        }
                    }
                }
            }
            //finished freeing all blocks associated with file. Now we just free the file itself.
            for(int j = 0; j < folder_number_entries; j++)
            {
                //printf("(parent_data + j) -> filename = %s\n", (parent_data + j) -> filename);
                if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + (count-1))) == 0 )
                {
                    uint32_t currentVacantFile = (1 << j);
                    currentVacantFile ^= UINT32_MAX;
                    parent_inode->vacantFile &= currentVacantFile;
                    //we are going to clear the parent data file as well to be safe...
                    memset((parent_data+j)->filename,0,127);
                    (parent_data+j)->inodeNumber = 0;
                    block_store_write(fs->BlockStore_whole,parent_inode->directPointer[0],parent_data);
                    //write back parent inode to indicate updates to its vacant file
                    block_store_inode_write(fs->BlockStore_inode,parent_inode->inodeNumber,parent_inode);
                    //block should now be empty, so we can free it.
                    block_store_sub_release(fs->BlockStore_inode,child_inode_ID);
                    //now we can finish & return
                    free(parent_inode);
                    free(child_inode);
                    // before any return, we need to free tokens, otherwise memory leakage
                    for (size_t i = 0; i < count; i++)
                    {
                        free(*(tokens + i));
                    }
                    free(tokens);
                    free(parent_data);
                    return 0;
                }
            }
        }
    }
    else {
        //file path does not exist... return error.
        free(parent_inode);
        // before any return, we need to free tokens, otherwise memory leakage
        for (size_t i = 0; i < count; i++)
        {
            free(*(tokens + i));
        }
        free(tokens);
        free(parent_data);
        return -1;
    }
    return 0;
}
void free_str_array(char** path_elems, int number_of_path_elems) {
    for(int abort = 0; abort < number_of_path_elems;abort++) {
        free(path_elems[abort]);
        path_elems[abort] = NULL;
    }
    free(path_elems);
    path_elems = NULL;
}
int get_inode_at_path_and_parent(FS_t* fs, const char* path, inode_t* child_inode, inode_t* parent_inode_to_return, char* filename_returned) {
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
    //we now know that inode exists & we have inode number, so we can read the inode
    block_store_inode_read(fs->BlockStore_inode,file_inode_number,child_inode);
    block_store_inode_read(fs->BlockStore_inode,parent_inode->inodeNumber,parent_inode_to_return);
    strncpy(filename_returned,path_elems[number_of_path_elems-1],strlen(path_elems[number_of_path_elems-1]));
    free_str_array(path_elems,number_of_path_elems);
    free(parent_inode);
    free(parent_data);
    parent_inode = NULL;
    parent_data = NULL;
    return 0;
}
int get_parent_inode(FS_t* fs, const char* path, inode_t* parent_inode_to_return, char* filename_returned) {
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
    //find child file doing similar as above, sequentially searching through parent dir until we find given file
    int parent_counter = 0;
    for(parent_counter = 0; parent_counter < 31; parent_counter++) {
        if(strcmp((parent_data + parent_counter)->filename,path_elems[number_of_path_elems-1])==0 && ((parent_inode->vacantFile>>parent_counter) & 1)==1) {
            //if we find the file already exists, then this is a problem, so we return an error.
            free_str_array(path_elems,number_of_path_elems);
            free(parent_inode);
            free(parent_data);
            parent_inode = NULL;
            parent_data = NULL;
            return -1;
        }
    }
    //we now know that inode exists & we have inode number, so we can read the inode
    block_store_inode_read(fs->BlockStore_inode,parent_inode->inodeNumber,parent_inode_to_return);
    strncpy(filename_returned,path_elems[number_of_path_elems-1],strlen(path_elems[number_of_path_elems-1]));
    free_str_array(path_elems,number_of_path_elems);
    free(parent_inode);
    free(parent_data);
    parent_inode = NULL;
    parent_data = NULL;
    return 0;
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
    if(fs == NULL|| src == NULL || dst == NULL ) {
        return -1;
    }
    //call helper function to get child & parent inodes for src
    inode_t* src_child_inode = calloc(1,sizeof(inode_t));
    inode_t* src_parent_inode = calloc(1, sizeof(inode_t));
    char* filename = calloc(127,sizeof(char));
    int returnvalue = 0;
    returnvalue = get_inode_at_path_and_parent(fs,src,src_child_inode,src_parent_inode,filename);
    if(returnvalue == -1) {
        //error along the way, so free what was allocated & return -1.
        free(src_parent_inode);
        free(src_child_inode);
        free(filename);
        return -1;
    }
    //now we have the source & parent of src, so let's try and get the parent of the destination.
    inode_t* dst_parent_inode = calloc(1, sizeof(inode_t));
    char* dest_filename = calloc(127,sizeof(char));
    returnvalue = get_parent_inode(fs,dst,dst_parent_inode,dest_filename);
    if(returnvalue == -1) {
        //error along the way, so free what was allocated & return -1.
        free(src_parent_inode);
        free(src_child_inode);
        free(dst_parent_inode);
        free(filename);
        free(dest_filename);
        return -1;
    }
    //check case for moving into itself, which happens when dst_parent inode is same as src_child_inode
    if(src_child_inode->inodeNumber == dst_parent_inode->inodeNumber) {
        free(src_parent_inode);
        free(src_child_inode);
        free(dst_parent_inode);
        free(filename);
        free(dest_filename);
        return -1;
    }
    //we now should have valid inodes, so we should be able to proceed.
    //let's scan along & find some space in the dst parent directory file
    int looking_for_space;
    for(looking_for_space = 0; looking_for_space < 31; looking_for_space++) {
        if((dst_parent_inode->vacantFile >> looking_for_space) == 0) {
            break;
        }
    }
    //if we get to the end & still no open space was found, then the directory is full, so we return an error.
    if(looking_for_space == 31) {
        free(src_parent_inode);
        free(src_child_inode);
        free(dst_parent_inode);
        free(filename);
        free(dest_filename);
        return -1;
    }
    //sweet. Now all we should need to do is remove the child inode from the src parent directory file & add it to where we just found was open
    directoryFile_t* src_parent_directory = calloc(1,BLOCK_SIZE_BYTES);
    block_store_read(fs->BlockStore_whole,src_parent_inode->directPointer[0],src_parent_directory);
    directoryFile_t* dst_parent_directory = calloc(1,BLOCK_SIZE_BYTES);
    block_store_read(fs->BlockStore_whole,dst_parent_inode->directPointer[0],dst_parent_directory);
    for(int j = 0; j < folder_number_entries; j++)
    {
        //printf("(parent_data + j) -> filename = %s\n", (parent_data + j) -> filename);
        if( ((src_parent_inode->vacantFile >> j) & 1) == 1 && strcmp((src_parent_directory + j) -> filename, filename)==0) {
            //found entry in vacant file, flip that bit in parent vacancy to indicate it is now available to be used. Now we just need to free the child block
            uint32_t currentVacantFile = (1 << j);
            currentVacantFile ^= UINT32_MAX;
            src_parent_inode->vacantFile &= currentVacantFile;
            //we are going to clear the parent data file as well to be safe...
            memset((src_parent_directory+j)->filename,0,127);
            (src_parent_directory+j)->inodeNumber = 0;
            block_store_write(fs->BlockStore_whole,src_parent_inode->directPointer[0],src_parent_directory);
            //write back parent inode to indicate updates to its vacant file
            block_store_inode_write(fs->BlockStore_inode,src_parent_inode->inodeNumber,src_parent_inode);
            break;
        }
    }
    //removed from src parent, let's add it to dst.
    (dst_parent_directory+looking_for_space)->inodeNumber = src_child_inode->inodeNumber;
    //use new name for file
    strcpy((dst_parent_directory+looking_for_space)->filename,dest_filename);
    dst_parent_inode->vacantFile |= (1 << looking_for_space);
    //everything should now be up to date. Let's just write everything back now and free.
    block_store_write(fs->BlockStore_whole,dst_parent_inode->directPointer[0],dst_parent_directory);
    block_store_inode_write(fs->BlockStore_inode,dst_parent_inode->inodeNumber,dst_parent_inode);
    free(src_parent_inode);
    free(src_child_inode);
    free(dst_parent_inode);
    free(filename);
    free(dest_filename);
    free(src_parent_directory);
    free(dst_parent_directory);
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
    if(fs == NULL|| src == NULL || dst == NULL ) {
        return -1;
    }
    //call helper function to get child & parent inodes for src
    inode_t* src_child_inode = calloc(1,sizeof(inode_t));
    inode_t* src_parent_inode = calloc(1, sizeof(inode_t));
    char* filename = calloc(127,sizeof(char));
    int returnvalue = 0;
    returnvalue = get_inode_at_path_and_parent(fs,src,src_child_inode,src_parent_inode,filename);
    if(returnvalue == -1) {
        //error along the way, so free what was allocated & return -1.
        free(src_parent_inode);
        free(src_child_inode);
        free(filename);
        return -1;
    }
    //now we have the source & parent of src, so let's try and get the parent of the destination.
    inode_t* dst_parent_inode = calloc(1, sizeof(inode_t));
    char* dest_filename = calloc(127,sizeof(char));
    returnvalue = get_parent_inode(fs,dst,dst_parent_inode,dest_filename);
    if(returnvalue == -1) {
        //error along the way, so free what was allocated & return -1.
        free(src_parent_inode);
        free(src_child_inode);
        free(dst_parent_inode);
        free(filename);
        free(dest_filename);
        return -1;
    }
    //look for space to link to 
    int looking_for_space;
    for(looking_for_space = 0; looking_for_space < 31; looking_for_space++) {
        if((dst_parent_inode->vacantFile >> looking_for_space) == 0) {
            break;
        }
    }
    //if we get to the end & still no open space was found, then the directory is full, so we return an error.
    if(looking_for_space == 31) {
        free(src_parent_inode);
        free(src_child_inode);
        free(dst_parent_inode);
        free(filename);
        free(dest_filename);
        return -1;
    }
    //we have open space in the directory, so let's add the old inode to the parent directory at dst
    directoryFile_t* dst_parent_directory = calloc(1,BLOCK_SIZE_BYTES);
    block_store_read(fs->BlockStore_whole,dst_parent_inode->directPointer[0],dst_parent_directory);

    src_child_inode->linkCount++;
    //update parent now
    dst_parent_inode->vacantFile |= (1 << looking_for_space);
    if(src_child_inode->inodeNumber == dst_parent_inode->inodeNumber) {
        //if linking to itself, we need to update the child's vacant file to match.
        //child_inode->vacantFile = dst_parent_inode->vacantFile;
    }
    strcpy((dst_parent_directory+looking_for_space)->filename,dest_filename);
    (dst_parent_directory+looking_for_space)->inodeNumber = src_child_inode->inodeNumber;
    //write updates back
    block_store_write(fs->BlockStore_whole,dst_parent_inode->directPointer[0],dst_parent_directory);
    block_store_inode_write(fs->BlockStore_inode,dst_parent_inode->inodeNumber,dst_parent_inode);
    free(src_parent_inode);
    free(src_child_inode);
    free(dst_parent_inode);
    free(filename);
    free(dest_filename);
    free(dst_parent_directory);
    return 0;
}

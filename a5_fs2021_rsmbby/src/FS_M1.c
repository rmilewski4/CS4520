#include "dyn_array.h"
#include "bitmap.h"
#include "block_store.h"
#include "FS_M1.h"

#define BLOCK_STORE_NUM_BLOCKS 65536    // 2^16 blocks.
#define BLOCK_STORE_AVAIL_BLOCKS 65534  // Last 2 blocks consumed by the FBM
#define BLOCK_SIZE_BITS 32768           // 2^12 BYTES per block *2^3 BITS per BYTES
#define BLOCK_SIZE_BYTES 4096           // 2^12 BYTES per block
#define BLOCK_STORE_NUM_BYTES (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES)  // 2^16 blocks of 2^12 bytes.

#define INODE_BMP_SIZE 256

// You might find this handy.  I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)

//get copy of current inode bitmap, stored in inode_bmp. Returns new inode_bmp copy or NULL on error.
//You as the user are expected to destroy the returned bitmap when done.
bitmap_t* fs_get_inode_bitmap(FS_t* fs) {
    //create variable to hold block that will be pulled
    uint8_t* inode_bmp_block = malloc(BLOCK_SIZE_BYTES);
    if(inode_bmp_block == NULL) {
        return NULL;
    }
    //inode_bmp occupies block 10, so read block 10 & store in previously created variable
    size_t bytes_read = block_store_read(fs->bs,10,inode_bmp_block);
    //making sure we read the whole block
    if(bytes_read != BLOCK_SIZE_BYTES){
        return NULL;
    }
    //pull in the bitmap from the given data from the block
    bitmap_t* bmp = bitmap_import(INODE_BMP_SIZE,inode_bmp_block);
    free(inode_bmp_block);
    return bmp;

}
//writes the inode_bmp to the block store
void fs_write_inode_bitmap(FS_t* fs, bitmap_t* inode_bmp) {
    //create variable to hold whole block
    uint8_t* inode_bmp_block = malloc(BLOCK_SIZE_BYTES);
    if(inode_bmp_block == NULL) {
        return;
    }
    //read entire bmp block
    size_t bytes_read = block_store_read(fs->bs,10,inode_bmp_block);
    //making sure we read the whole block
    if(bytes_read != BLOCK_SIZE_BYTES){
        return;
    }
    //pull out the bitmap data using bmp_export
    const uint8_t* bmp_data = bitmap_export(inode_bmp);
    //copy the bitmap data into the block that was pulled out of the block store
    memcpy(inode_bmp_block,bmp_data,bitmap_get_bytes(inode_bmp));
    //write updated block back to the block store & free temp variable
    block_store_write(fs->bs,10,inode_bmp_block);
    free(inode_bmp_block);
}
//sets up inode bitmap.
void fs_create_inode_bitmap(FS_t* fs) {
    //only 256 inodes so only need 256 bits
    bitmap_t* inode_bmp = bitmap_create(256);
    //write bitmap to the block store
    fs_write_inode_bitmap(fs,inode_bmp);
    //destroy temp bitmap, can now be referenced using fs_get_inode_bitmap
    bitmap_destroy(inode_bmp);
}

//This function will write the given inode to the block store.
/// \return Number of bytes written to the block store, or -1 on error
size_t fs_write_inode(FS_t* fs, inode_t* inode_to_write) {
    //get inode num
    uint8_t inode_num = inode_to_write->inodeNum;
    //each block holds 64 inodes, so we need to figure out which block the given inode to be written
    //will be placed in.
    uint8_t block_num = (inode_num / 64) + 6;
    //pull out given block that is going to be written to
    uint8_t* block_data = malloc(BLOCK_SIZE_BYTES);
    size_t data_read = block_store_read(fs->bs,block_num,block_data);
    //making sure entire block was pulled out
    if(data_read != BLOCK_SIZE_BYTES) {
        return -1;
    }
    //copy given inode to the spot in inode table
    memcpy(&block_data[(inode_num % 64)*64],inode_to_write,64);
    //write back to block store & free temp variable
    size_t bytes_written = block_store_write(fs->bs,block_num,block_data);
    free(block_data);
    return bytes_written;
}
//get copy of current inode at given inode number. Return NULL on error, or the inode copy.
//You as the user are responsible for freeing the inode copy given.
inode_t* fs_get_inode(FS_t* fs, uint8_t inode_num) {
    //since each block holds 64 inodes, dividing the inode number by 64 allows us to get that block. The base inode table starts at block 6.
    uint8_t block_num = (inode_num / 64) + 6;
    uint8_t* block_data = malloc(BLOCK_SIZE_BYTES);
    //get block that holds the inode we need.
    size_t data_read = block_store_read(fs->bs,block_num,block_data);
    if(data_read != BLOCK_SIZE_BYTES) {
        return NULL;
    }
    //knowing that each inode is 64 bytes, we can index exactly into where we need to get to.
    //use modular division since the block we are looking into only has 64 elements.
    inode_t* inode_returned = malloc(64);
    memcpy(inode_returned,&block_data[(inode_num % 64)*64],64);
    return inode_returned;
}

FS_t *fs_format(const char *path)
{
    //checking for errors
    if(path == NULL){
        return NULL;
    }
    //create fs variable
    FS_t* fs = malloc(sizeof(FS_t));
    if(fs == NULL) {
        return NULL;
    }
    //use fs create to actually setup the block store
    fs->bs = block_store_create(path);
    //error checking the block store creation
    if(fs->bs == NULL) {
        free(fs);
        return NULL;
    }
    bool success;
    //allocate space in bs for inode table + inode bmp in blocks 6-10
    //inode table in blocks 6-9 (each block holds 64 inodes)
    //inode bmp in block 10
    for(int i = 6; i <=10;i++) {
        success = block_store_request(fs->bs, i);
        if (success == false) {
            block_store_destroy(fs->bs);
            free(fs);
            return NULL;
        }
    }
    //create inode bitmap in block 10 with helper function
    fs_create_inode_bitmap(fs);
    //get copy of bitmap as we need to setup our root inode.
    bitmap_t* inode_bmp = fs_get_inode_bitmap(fs);
    //get where the inode will be set (should be 0 since the bitmap was init. to all 0s)
    size_t root_inode_num = bitmap_ffz(inode_bmp);
    //set the bitmap to indicate the block is now occupied by an inode.
    bitmap_set(inode_bmp, root_inode_num);
    //write the update bitmap back to the block
    fs_write_inode_bitmap(fs,inode_bmp);
    //destroy the created bitmap copy
    bitmap_destroy(inode_bmp);
    //all inodes are 64 bytes long, extra space if there is any will be wasted.
    inode_t* root_inode = calloc(1,64);
    //setup root inode metadata
    root_inode->inodeNum = root_inode_num;
    root_inode->numberOfHardLinks = 1;
    root_inode->fileSize = 0; // no data in the directory yet.
    root_inode->inodeType = FS_DIRECTORY;
    root_inode->lastAccessTime = time(NULL);
    //write root inode to table
    fs_write_inode(fs,root_inode);
    //free root_inode var since it was just written to block store
    free(root_inode);
    //return fs ready to be used
    return fs;
}

FS_t *fs_mount(const char *path)
{
    //checking for errors
    if(path == NULL){
        return NULL;
    }
    //create fs variable & error check
    FS_t* fs = malloc(sizeof(FS_t));
    if(fs == NULL) {
        return NULL;
    }
    //open up block store on the given path and store in fs
    fs->bs = block_store_open(path);
    //error check opening the block store
    if(fs->bs == NULL) {
        free(fs);
        return NULL;
    }
    //return opened block store
    return fs;
}

int fs_unmount(FS_t *fs)
{
    //error checking fs
    if(fs == NULL) {
        return -1;
    }
    //destroy block store inside of the fs
    block_store_destroy(fs->bs);
    //free created fs variable & return success.
    free(fs);
    return 0;
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
    UNUSED(fs);
    UNUSED(path);
    UNUSED(type);

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
    UNUSED(fs);
    UNUSED(path);
    return 0;
}

int fs_close(FS_t *fs, int fd)
{
    //PSEUDOCODE:
    /*
    first, error check all parameters to ensure all are valid
    if fd is valid, then we can delete the file descriptor from the fd table, and update any associated data to indicate that it was removed
    return success
    */
    UNUSED(fs);
    UNUSED(fd);
    return 0;
}

off_t fs_seek(FS_t *fs, int fd, off_t offset, seek_t whence)
{
    UNUSED(fs);
    UNUSED(fd);
    UNUSED(offset);
    UNUSED(whence);
    return 0;
}

ssize_t fs_read(FS_t *fs, int fd, void *dst, size_t nbyte)
{
    UNUSED(fs);
    UNUSED(fd);
    UNUSED(dst);
    UNUSED(nbyte);
    return 0;
}

ssize_t fs_write(FS_t *fs, int fd, const void *src, size_t nbyte)
{
    UNUSED(fs);
    UNUSED(fd);
    UNUSED(src);
    UNUSED(nbyte);
    return 0;
}

int fs_remove(FS_t *fs, const char *path)
{
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
    UNUSED(fs);
    UNUSED(path);
    return NULL;
}

int fs_move(FS_t *fs, const char *src, const char *dst)
{
    UNUSED(fs);
    UNUSED(src);
    UNUSED(dst);
    return 0;
}

int fs_link(FS_t *fs, const char *src, const char *dst)
{
    UNUSED(fs);
    UNUSED(src);
    UNUSED(dst);
    return 0;
}

#include <stdio.h>
#include <stdint.h>
#include "bitmap.h"
#include "block_store.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <errno.h>

// include more if you need
#define BLOCK_STORE_NUM_BLOCKS 2048
#define BLOCK_SIZE_BYTES 64         // 2^6 BYTES per block
#define BITMAP_SIZE_BITS BLOCK_STORE_NUM_BLOCKS
#define BITMAP_SIZE_BYTES (BITMAP_SIZE_BITS / 8)
#define BLOCK_STORE_NUM_BYTES (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES)
#define BITMAP_START_BLOCK 1022
#define BITMAP_NUM_BLOCKS (BITMAP_SIZE_BYTES/BLOCK_SIZE_BYTES)
// You might find this handy.  I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)

struct block_store {
    //using char for the data blocks because it's only 1 byte elements, and can still perform pointer arithmetic if necessary.
    char* data_blocks;
    bitmap_t* fbm;
};

block_store_t *block_store_create()
{
    //Create blockstore and error check
    block_store_t* bs = malloc(sizeof(block_store_t));
    if(bs == NULL) {
        return NULL;
    }
    //Allocate space for all block store based on the byte size and number of blocks, error check
    bs->data_blocks = calloc(BLOCK_STORE_NUM_BLOCKS,BLOCK_SIZE_BYTES);
    if(bs->data_blocks == NULL) {
        return NULL;
    }
    //Create bitmap and store at location 1022 in the data blocks, however, we maintain a pointer to that bitmap for easy access.
    bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS, &(bs->data_blocks[BLOCK_SIZE_BYTES*BITMAP_START_BLOCK]));
    //blocks 1022-1025 are occupied by the bitmap, so we will set these to 1s.
    for(size_t block = BITMAP_START_BLOCK; block < BITMAP_START_BLOCK+(BITMAP_NUM_BLOCKS);block++) {
        bitmap_set(bs->fbm, block);
    }
    return bs;
}

void block_store_destroy(block_store_t *const bs)
{
    //error checking
    if(bs == NULL) {
        return;
    }
    //Destroy the bitmap
    bitmap_destroy(bs->fbm);
    //Free the data blocks and block store
    free(bs->data_blocks);
    free(bs);
}
size_t block_store_allocate(block_store_t *const bs)
{
    //error checking
    if(bs == NULL) {
        return SIZE_MAX;
    }
    //use helper function to find first zero block
    size_t firstzero = bitmap_ffz(bs->fbm);
    //if block is full, indicate error
    if(firstzero == SIZE_MAX) {
        return SIZE_MAX;
    }
    //set given bit of bitmap to indicate it has been allocated
    bitmap_set(bs->fbm,firstzero);
    return firstzero;
}

bool block_store_request(block_store_t *const bs, const size_t block_id)
{
    //error checking
    if(bs == NULL  || block_id > BLOCK_STORE_NUM_BLOCKS) {
        return false;
    }
    //check and see if block is occupied already, if so, indicate error
    if(bitmap_test(bs->fbm,block_id)==1) {
        return false;
    }
    //otherwise set the bit and return true
    bitmap_set(bs->fbm,block_id);
    return true;
}

void block_store_release(block_store_t *const bs, const size_t block_id)
{
    //error checking
    if(bs == NULL  || block_id > BLOCK_STORE_NUM_BLOCKS) {
        return;
    }
    //using helper function to reset the bit of bitmap back to 0 to indicate it can be used again
    bitmap_reset(bs->fbm,block_id);
}

size_t block_store_get_used_blocks(const block_store_t *const bs)
{
    //error checking
    if(bs == NULL) {
        return SIZE_MAX;
    }
    //using helper function to get the total number of set bits in bitmap
    return bitmap_total_set(bs->fbm);
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
    if(bs == NULL) {
        return SIZE_MAX;
    }
    //we can get the number of free blocks by counting the number of bits in the bitmap subtracted by the total # of set bits
    return bitmap_get_bits(bs->fbm) - bitmap_total_set(bs->fbm);
}

size_t block_store_get_total_blocks()
{
    //Just use builtin constant to get total number of blocks in bitmap
    return BLOCK_STORE_NUM_BLOCKS;
}

size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
    //error checking
    if(bs == NULL || buffer == NULL || block_id > BLOCK_STORE_NUM_BLOCKS) {
        return 0;
    }
    //If block is empty, we can't read from it
    if(bitmap_test(bs->fbm,block_id)==0) {
        return 0;
    }
    //get the address of the starting block by indexing into data block
    char* startingblock = &(bs->data_blocks[block_id*BLOCK_SIZE_BYTES]);
    //copy the data from the block to the buffer
    memcpy(buffer,startingblock,BLOCK_SIZE_BYTES);
    return BLOCK_SIZE_BYTES;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
    //error checking
    if(bs == NULL || buffer == NULL || block_id > BLOCK_STORE_NUM_BLOCKS) {
        return 0;
    }
    //get the address of the starting block by indexing into data block
    char* writeblock = &(bs->data_blocks[block_id*BLOCK_SIZE_BYTES]);
    //copy data from buffer to block that is going to be written to
    memcpy(writeblock,buffer,BLOCK_SIZE_BYTES);
    return BLOCK_SIZE_BYTES;
}

block_store_t *block_store_deserialize(const char *const filename)
{
    //error check file name
    if(filename == NULL ) {
        return NULL;
    }
    //open file for reading, check for errors
    int fd = open(filename, O_RDONLY | O_CREAT, 0666);
    if(fd == -1) {
        return 0;
    }
    //create bs device & error check
    block_store_t* bs = block_store_create();
    if(bs == NULL) {
        return 0;
    }
    //read to the data block from the file
    ssize_t bytesread = read(fd,bs->data_blocks,BLOCK_STORE_NUM_BYTES);
    if(bytesread == -1) {
        return 0;
    }
    //close file and return bs
    if(close(fd) == -1) {
        return 0;
    }   
    return bs;
}

size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
    //error check
    if(bs == NULL || filename == NULL) {
        return 0;
    }
    //open file for writing
    int fd = open(filename, O_WRONLY | O_CREAT, 0666);
    if(fd == -1) {
        printf("error opening fd\n");
        return 0;
    }
    //write the data to the file descriptor from the data block
    size_t byteswritten = write(fd,bs->data_blocks,BLOCK_STORE_NUM_BYTES);
    //close the file and return # of bytes written
    if(close(fd) == -1) {
        return 0;
    }
    return byteswritten;
}

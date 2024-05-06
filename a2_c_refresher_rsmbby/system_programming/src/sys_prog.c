#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../include/sys_prog.h"

// LOOK INTO OPEN, READ, WRITE, CLOSE, FSTAT/STAT, LSEEK
// GOOGLE FOR ENDIANESS HELP

bool bulk_read(const char *input_filename, void *dst, const size_t offset, const size_t dst_size)
{
    //checking to make sure we have a valid filename, destination and size and that they will not cause issues for read operations
    if(input_filename == NULL || dst == NULL || dst_size <= 0) {
        return false;
    }
    //Using the unix open call for read only, and returning to a file descriptor
    int fd = open(input_filename,O_RDONLY);
    //if the open has an error, it will return -1 and so we will return false.
    if(fd == -1) {
        return false;
    }
    //We advance into the given file from the start by the offset given
    int ecseek = lseek(fd,offset,SEEK_SET);
    // if our seek returns an error, we will close our file and return false
    if(ecseek == -1) {
        close(fd);
        return false;
    }
    //finally reading the data into dst up until the size of dst
    int bytesread = read(fd, dst, dst_size);
    //If we read 0 bytes (this means that we have a bad buffer and that we went past the end of the file) or -1 indicating an error, we return false
    if(bytesread == -1 || bytesread == 0) {
        close(fd);
        return false;
    }
    //otherwise we return the file and return true.
    close(fd);
    return true;
}

bool bulk_write(const void *src, const char *output_filename, const size_t offset, const size_t src_size)
{
    //checking to make sure we have a valid filename, destination and size and that they will not cause issues for read operations
    if(src == NULL || output_filename == NULL || src_size <= 0) {
        return false;
    }  
    int fd = open(output_filename, O_WRONLY);
    //if the open has an error, it will return -1 and so we will return false.
    if(fd == -1) {
        return false;
    }
    //We advance into the given file from the start by the offset given
    int ecseek = lseek(fd,offset,SEEK_SET);
    // if our seek returns an error, we will close our file and return false
    if(ecseek == -1) {
        close(fd);
        return false;
    }
    int byteswritten = write(fd,src,src_size);
    if(byteswritten == -1 ) {
        close(fd);
        return false;
    }
    //close and return true when done
    close(fd);
    return true;
}


bool file_stat(const char *query_filename, struct stat *metadata)
{
    //verify that the filename and metadata exists that is being passed in
    if(query_filename == NULL || metadata == NULL) {
        return false;
    }
    //run the stat system call with the given parameters
    int statec = stat(query_filename, metadata);
    //if stat returns -1, this indicates error so return false
    if(statec == -1) {
        return false;
    }
    //otherwise return true
    return true;
}

bool endianess_converter(uint32_t *src_data, uint32_t *dst_data, const size_t src_count)
{
    if(src_data == NULL || dst_data == NULL || src_count <= 0) {
        return false;
    }
    uint8_t leftmost, leftmiddle, rightmiddle, rightmost;
    for(int i = 0; i < src_count; i++) {
        //This will return the left most byte by anding the source data with an FF in the rightmost 8 bits of the number. 
        //This will then be converted to the leftmost 8 bits after the bitshift
        leftmost = (src_data[i] & 0x000000FF) >> 0;
        //Similarly, we return the leftmiddle and rightmiddle bits and shift them accordingly to the end of the number
        leftmiddle = (src_data[i] & 0x0000FF00) >> 8;
        rightmiddle = (src_data[i] & 0x00FF0000) >> 16;
        //We get the 8 most bits on the left which are to become the right most bits
        rightmost = (src_data[i] & 0xFF000000) >> 24;
        //Since this currently holds the leftmost bits in the first spot, we need to shift these 24 bits to the left, the next will be 16, then 8 and so on
        leftmost = leftmost << 24;
        leftmiddle = leftmiddle << 16;
        rightmiddle = rightmiddle << 8;
        leftmost = leftmost << 0;
        //Finally, we bitwise OR these elements together to get the original number.
        dst_data[i] = (leftmost | leftmiddle | rightmiddle | rightmost);
    }
    return true;
}


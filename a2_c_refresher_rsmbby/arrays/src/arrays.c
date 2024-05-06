#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../include/arrays.h"

// LOOK INTO MEMCPY, MEMCMP, FREAD, and FWRITE

bool array_copy(const void *src, void *dst, const size_t elem_size, const size_t elem_count)
{
    //checking to make sure none of the values we use for memcpy are invalid, such as either element being NULL or the size part being 0
    if(src == NULL || dst == NULL || elem_size==0 || elem_count == 0) {
        return false;
    }
    //If the memory addresses are not null then we will use memcpy to copy the source into the destination
    if(src != NULL && dst != NULL){
       memcpy(dst,src,elem_size*elem_count);
    }
    //Check and see using memcmp if the operation was successful.
    if (memcmp(src,dst,elem_size*elem_count)==0) {
        return true;
    }
    else {
        return false;
    }
}

bool array_is_equal(const void *data_one, void *data_two, const size_t elem_size, const size_t elem_count)
{
    //Similar to above function, checking to see if any invalid data exists
    if(data_one == NULL || data_two == NULL || elem_size==0 || elem_count == 0) {
        return false;
    }
    //Using memcmp to check if the arrays are equal. Memcmp will return 0 if they are the same.
    if (memcmp(data_one,data_two,elem_size*elem_count)==0) {
        return true;
    }
    else {
        return false;
    }
}

ssize_t array_locate(const void *data, const void *target, const size_t elem_size, const size_t elem_count)
{
    //check to see if invalid data is in the array
    if(data == NULL || target == NULL || elem_size == 0) {
        return -1;
    }
    //Loop through for each element in the array
    for(int i = 0; i < elem_count;i++) {
        //use pointer arethmetic to compare each individual element to the target and return the index if found.
        if(memcmp(data+(elem_size*i),target,elem_size)==0) {
            return i;
        }
    }
    return -1;
}

bool array_serialize(const void *src_data, const char *dst_file, const size_t elem_size, const size_t elem_count)
{
    //checking for invalid possibliities, if the source data doesn't exist or the file name is NULL, either size element is 0 or if a newline is found in the file name.
    if(src_data == NULL || dst_file == NULL || elem_size == 0 || elem_count == 0 || strchr(dst_file,'\n') != NULL) {
        return false;
    }
    //create and open file pointer to write in binary.
    FILE* FP;
    FP = fopen(dst_file,"wb");
    //if the file couldn't be opened return false
    if(FP == NULL) {
        return false;
    }
    //write each element and close the file, and return true.
    fwrite(src_data,elem_size,elem_count,FP);
    fclose(FP);
    return true;
}

bool array_deserialize(const char *src_file, void *dst_data, const size_t elem_size, const size_t elem_count)
{
    //checking for invalid possibliities, if the source data doesn't exist or the file name is NULL, either size element is 0 or if a newline is found in the file name.
    if(src_file == NULL || dst_data == NULL || elem_size == 0 || elem_count == 0 || strchr(src_file,'\n') != NULL) {
        return false;
    }
    //create and open file pointer to read in binary.
    FILE* FP;
    FP = fopen(src_file,"rb");
    //if the file couldn't be opened return false
    if(FP == NULL) {
        return false;
    }
    //read each element into the destination array, close the file, and return true;
    fread(dst_data,elem_size,elem_count,FP);
    fclose(FP);
    return true;
}


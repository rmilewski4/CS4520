#include "../include/allocation.h"
#include <stdlib.h>
#include <stdio.h>

void* allocate_array(size_t member_size, size_t nmember,bool clear)
{
    //pointer that will be returned at the end with the allocated space
    void* ptr = NULL;
    //chekcing to ensure the member byte size is valid and not 0 or negative
    if((int) nmember <= 0 || (int) member_size <= 0) {
        return NULL;
    }
    //if we are using calloc
    if(clear == true) {
        //Set the pointer created earlier to the calloc call with the given parameters
        ptr = calloc(nmember, member_size);
    }
    else {
        //Otherwise we are using malloc, and the size in bytes can be obtained by multiplying the member size by the number of members
        ptr = malloc(nmember * member_size);
    }
    //checking for errors, if pointer is null, we can return NULL to indicate error (this statement isn't really necessary but done for clarity)
    if(ptr == NULL) {
        return NULL;
    }
    //Finally return the allocated pointer.
    return ptr;
}

void* reallocate_array(void* ptr, size_t size)
{
    //Checking for errors right away, returning NULL indicating error if the size is invalid, or given an invalid pointer
    if((int) size < 0 || ptr == NULL) {
        return NULL;
    }
    //Call realloc, using the given parameters, and updating the given pointer, and finally returning the updated pointer
    ptr = realloc(ptr, size);
    return ptr;
}

void deallocate_array(void** ptr)
{
    //Derefrencing the given pointer and freeing it. Since free does not do anything when called on a NULL pointer, we don't need to error check
    free(*ptr);
    //Set the pointer to NULL to indicate it was freed
    *ptr = NULL;
}

char* read_line_to_buffer(char* filename)
{
    //Allocate space for the buffer
    char* buffer = malloc(sizeof(char) * 1096);
    //create filepointer and open for reading
    FILE* fp;
    fp = fopen(filename,"r");
    //catch possible error with fileopening
    if(fp == NULL) {
        return NULL;
    }
    //Use fgets to read a line of the file into the buffer
    fgets(buffer,sizeof(buffer),fp);
    //close the file and return the buffer
    fclose(fp);
    return buffer;

}

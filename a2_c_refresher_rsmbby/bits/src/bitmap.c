#include "../include/bitmap.h"

// data is an array of uint8_t and needs to be allocated in bitmap_create
//      and used in the remaining bitmap functions. You will use data for any bit operations and bit logic
// bit_count the number of requested bits, set in bitmap_create from n_bits
// byte_count the total number of bytes the data contains, set in bitmap_create


bitmap_t *bitmap_create(size_t n_bits)
{
    size_t byte_count = 0;
    //Checking if we have a valid bit size, if not we return NULL to indicate error
    if(n_bits <= 0) {
        return NULL;
    }
    //We do modular division to see if it's possible for the number of bits to evenly divide into 8
    if(n_bits % 8 == 0) {
        //if so, we can just set our byte count to that number actually divided
        byte_count = n_bits / 8;
    }
    else {
        //Otherwise we're at an uneven bit count, so we will need to add an extra byte to our bit map to fill in this extra
        //space, so we do the division and add one to compensate
        byte_count = (n_bits / 8) + 1;
    }
    //using calloc to initalize all memory to 0
    bitmap_t * bmp = calloc(1,sizeof(bitmap_t));
    //check for errors with calloc
    if(bmp == NULL) {
        return NULL;
    }
    //set elements appropriately
    bmp->bit_count = n_bits;
    bmp->byte_count = byte_count;
    //create actual data array and set it into bitmap
    uint8_t * data = calloc(byte_count, sizeof(uint8_t));
    if(data == NULL) {
        return false;
    }
    bmp->data = data;
    return bmp;
}

bool bitmap_set(bitmap_t *const bitmap, const size_t bit)
{
    //check for errors with bitmap or invalid bit size
    if(bitmap == NULL || bit > bitmap->bit_count || bit < 0) {
        return false;
    }
    //Finding the index and position of the manipulated bit using remainder and integer division. We divide by 8 because each element in our array is 8 bits
    int index = bit/8;
    int position = bit % 8;
    //Access at the given index and bitwise OR the array element with a 1 shifted to the calculated position
    bitmap->data[index] |= 1 << (position);
    return true;
}

bool bitmap_reset(bitmap_t *const bitmap, const size_t bit)
{
    //check for errors with bitmap or invalid bit size
    if(bitmap == NULL || bit > bitmap->bit_count || bit < 0) {
        return false;
    }
    //Finding the index and position of the manipulated bit using remainder and integer division. We divide by 8 because each element in our array is 8 bits
    int index = bit/8;
    int position = bit % 8;
    //Access at the given index and bitwise AND the array element with a 1 shifted to the calculated position and then NOTted
    bitmap->data[index] &= ~(1 << (position));
    return true;
}

bool bitmap_test(const bitmap_t *const bitmap, const size_t bit)
{
    //check for errors with bitmap or invalid bit size
    if(bitmap == NULL || bit > bitmap->bit_count || bit < 0) {
        return false;
    }
    //Finding the index and position of the manipulated bit using remainder and integer division. We divide by 8 because each element in our array is 8 bits
    int index = bit/8;
    int position = bit % 8;
    //Access at the given index and bitwise AND the array element with a 1 shifted to the calculated position which will return the singular bit.
    //If this is not equal to 0 then the bit is a 1, so we return 1, otherwise we can return 0
    if((bitmap->data[index] & (1 << position)) != 0) {
        return 1;
    }
    return 0;
}

size_t bitmap_ffs(const bitmap_t *const bitmap)
{
    //check for valid bitmap
    if(bitmap == NULL) {
        return SIZE_MAX;
    }
    //looping through at each bit position
    for(int numbits = 0;numbits<bitmap->bit_count; numbits++) {
        //Figuring out what index and position this is, similar to above functions
        int index = numbits / 8;
        int position = numbits % 8;
        //Just like bitmap_test function, checking to see if the queried bit is a 1 or not, if so, we return the index we are at
        if((bitmap->data[index] & (1 << position)) != 0) {
            return numbits;
        }
    }
    //If not found, return max size
    return SIZE_MAX;
}

size_t bitmap_ffz(const bitmap_t *const bitmap)
{
    //checking for valid bitmap
    if(bitmap == NULL) {
        return SIZE_MAX;
    }
    //looping though each bit
    for(int numbits = 0;numbits<bitmap->bit_count; numbits++) {
        int index = numbits / 8;
        int position = numbits % 8;
        //Just like find first zero function, except we are comparing the query to 0 and not 1. If we find a 1 we return the index we are at.
        if((bitmap->data[index] & (1 << position)) == 0) {
            return numbits;
        }
    }
    return SIZE_MAX;
}

bool bitmap_destroy(bitmap_t *bitmap)
{
    //checking for valid bitmap
    if(bitmap == NULL) {
        return false;
    }
    free(bitmap->data);
    //freeing the object back to memory then returning true
    free(bitmap);
    return true;
}

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "../include/sstring.h"

bool string_valid(const char *str, const size_t length)
{
    //first check and see if we have a valid string that isn't null and has a valid length of greater than 0
    if(str == NULL || length <= 0) {
        return false;
    }
    //compare the null terminator character ASCII value to the last character of the string, which will be the null terminator character if it is a valid string and can return true
    if(str[length-1] == '\0') {
        return true;
    }
    //otherwise return false
    return false;
}

char *string_duplicate(const char *str, const size_t length)
{
    //first check and see if we have a valid string that isn't null and has a valid length of greater than 0
    if(str == NULL || length <= 0) {
        return false;
    }
    //malloc space for the string, which is the size of char Times the length.
    char* destStr = malloc(length * sizeof(char));
    //copy the old string into the new string using memcpy
    memcpy(destStr,str,length*sizeof(char));
    //return the new string.
    return destStr;
}

bool string_equal(const char *str_a, const char *str_b, const size_t length)
{
    //first check and see if we have a valid string that isn't null and has a valid length of greater than 0
    if(str_a == NULL || str_b == NULL || length <= 0) {
        return false;
    }
    //compare the two strings and checks if they are equal using memcmp since strings are just character arrays.
    if(memcmp(str_a,str_b,sizeof(char)*length) == 0) {
        return true;
    }
    return false;
}

int string_length(const char *str, const size_t length)
{
    //first check and see if we have a valid string that isn't null and has a valid length of greater than 0
    if(str == NULL || length <= 0) {
        return -1;
    }
    //creater counter and loop variable starting at the beginning of the string
    int counter = 0;
    char currentchar = str[0];
    //we will loop until we hit the "end" of the string or null terminator
    while(currentchar != '\0') {
        //increment the counter
        counter++;
        //since the string is one element greater, we can get the next element of the string by just using the counter as our index to continue down the string.
        currentchar = str[counter];
    }
    return counter;
}

int string_tokenize(const char *str, const char *delims, const size_t str_length, char **tokens, const size_t max_token_length, const size_t requested_tokens)
{
    //first check and see if we have a valid string that isn't null and a valid delimter, and has a valid length of greater than 0
    if(str == NULL || delims == NULL || str_length <= 0 || tokens == NULL || requested_tokens == 0 || max_token_length == 0) {
        return false;
    }
    //Create new string since we won't be able to modify the parameter string due to it being const
    char* copyStr = malloc(sizeof(char)*str_length);
    //Actually copy the string over using the std lib function
    strcpy(copyStr,str);
    //Use strtok to get the first token using the delimeter
    char* tempToken = strtok(copyStr,delims);
    int counter = 0;
    //We know that we have gotten all tokens when the temptoken return NULL so we will loop until we reach NULL
    while(tempToken != NULL) {
        //checking if our space to store the tokens has been allocated properly, returning -1 if not.
        if(tokens[counter] == NULL) {
            free(copyStr);
            return -1;
        }
        else {
            //otherwise we copy the recieved token into the token array
            strcpy(tokens[counter],tempToken);
        }
        //recieve the next token in the string
        tempToken = strtok(NULL,delims);
        counter++;
    }
    //finally return the count of all parsed tokens done and free the memory we created fot the temporary string
    free(copyStr);
    return counter;
}

bool string_to_int(const char *str, int *converted_value)
{
    //first check and see if we have a valid string that isn't null and has a valid length of greater than 0
    if(str == NULL || converted_value == NULL) {
        return false;
    }
    //We will use strtol instead of atoi because atoi has undefined behavior when it overflows. Using a larger datatype will first allow us to check if the given number is within the bounds of an integer
    long longconverted = strtol(str, NULL, 10);
    //if the number is not within the bounds of an integer then return false.
    if(longconverted > INT_MAX || longconverted < INT_MIN) {
        return false;
    }
    //Otherwise cast the long back to an integer and return true;
    *converted_value = (int)longconverted;
    return true;
}


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "../include/error_handling.h"

int create_blank_records(Record_t **records, const size_t num_records)
{
	//checking for bad parameters, if either are invalid, we return the proper error code
	if(num_records == 0 || *records != NULL) {
		return -1;
	}
	if((int) num_records < 0) {
		return -2;
	}
	*records = (Record_t*) malloc(sizeof(Record_t) * num_records);
	//checking for proper allocation and returning the proper error code
	if(*records == NULL) {
		return -2;
	}
	memset(*records,0,sizeof(Record_t) * num_records);
	return 0;	
}

int read_records(const char *input_filename, Record_t *records, const size_t num_records) {
	//checking for bad parameters
	if(input_filename == NULL || records == NULL || num_records == 0) {
		return -1;
	}
	int fd = open(input_filename, O_RDONLY);
	//checking for bad open
	if(fd == -1) {
		return -2;
	}
	ssize_t data_read = 0;
	for (size_t i = 0; i < num_records; ++i) {
		data_read = read(fd,&records[i], sizeof(Record_t));	
		//checking for bad read
		if(data_read <= 0) {
			return -3;
		}
	}
	return 0;
}

int create_record(Record_t **new_record, const char* name, int age)
{
	//checking for bad parameters
	if(*new_record != NULL || name == NULL || name[0] == '\n' || strlen(name) >= 50 || age < 1 || age > 200) {
		return -1;
	}
	*new_record = (Record_t*) malloc(sizeof(Record_t));
	//checking for bad allocation
	if(*new_record == NULL) {
		return -2;
	}
	memcpy((*new_record)->name,name,sizeof(char) * strlen(name));
	(*new_record)->name[MAX_NAME_LEN - 1] = 0;	
	(*new_record)->age = age;
	return 0;

}

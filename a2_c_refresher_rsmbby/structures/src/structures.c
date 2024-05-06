#include "../include/structures.h"
#include <stdio.h>

int compare_structs(sample_t* a, sample_t* b)
{
	//checking each individual element from each struct and comparing them
	if(a->a == b->a && a->b == b->b && a->c == b->c) {
		return 1;
	}
	return 0;
}

void print_alignments()
{
	printf("Alignment of int is %zu bytes\n",__alignof__(int));
	printf("Alignment of double is %zu bytes\n",__alignof__(double));
	printf("Alignment of float is %zu bytes\n",__alignof__(float));
	printf("Alignment of char is %zu bytes\n",__alignof__(char));
	printf("Alignment of long long is %zu bytes\n",__alignof__(long long));
	printf("Alignment of short is %zu bytes\n",__alignof__(short));
	printf("Alignment of structs are %zu bytes\n",__alignof__(fruit_t));
}
//USING type = 1 for apple, and type = 2 for orange
int sort_fruit(const fruit_t* a,int* apples,int* oranges, const size_t size)
{
	//traversing through the entire array
	for(int i = 0; i < (int) size; i++) {
		//For apples, if we find type to be one, we increment the apple counter
		if(a[i].type == 1){
			(*apples)++;
		}
		else {
			//Otherwise, it must be an orange, so we increment the orange counter
			(*oranges)++;
		}
	}
	return size;
}

int initialize_array(fruit_t* a, int apples, int oranges)
{
	//Looping through the entire array, which is the two fruit counts added together
	for(int i  = 0; i < apples+oranges; i++) {
		if(i < apples) {
			//we start by adding the apples, so we set the first elements from 0 - the # of apples to be of type apple
			a[i].type = 1;
		}
		else {
			//otherwise we set them to be oranges
			a[i].type = 2;
		}
	}
	return 0;
}

int initialize_orange(orange_t* a)
{
	//initalize the orange_t struct by allocating space for it, and returning 0 on success.
	a = malloc(sizeof(orange_t));
	if(a == NULL) {
		return -1;
	}
	//initalizing the elements of the struct
	a->type = 2;
	a->weight = 0;
	a->peeled = 0;
	return 0;

}

int initialize_apple(apple_t* a)
{
	//Similar to above function but for apple
	a = malloc(sizeof(apple_t));
	if(a == NULL) {
		return -1;
	}
	a->type = 1;
	a->weight = 0;
	a->worms = 0;
	return 0;
}

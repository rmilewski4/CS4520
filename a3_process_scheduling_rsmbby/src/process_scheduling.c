#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dyn_array.h"
#include "process_scheduling.h"
#include "timsort.h"

// You might find this handy.  I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)

// private function
void virtual_cpu(ProcessControlBlock_t *process_control_block) 
{
    // decrement the burst time of the pcb
    --process_control_block->remaining_burst_time;
}

//function to compare arrival times, used for dyn_array_sort function
int compare_arrival(const void *a, const void *b) {
    const ProcessControlBlock_t * arr1 = a;
    const ProcessControlBlock_t * arr2 = b;
    return arr1->arrival - arr2->arrival;
}
//function that actually handles processing each fcfs process. Since processes are pushed back onto the ready_queue, we first scan to get to the end of the list of processes with the smallest arrival time, then pull from the end of that list until elements with that arrival time are done.
void process_fcfs(dyn_array_t* ready_queue, ProcessControlBlock_t* element_in_use, unsigned long* total_run_time, float* turnaround_time_sum, float* waiting_time_sum, size_t indexToRemove) {
    //We loop until we have processed all elements with the given arrival time. Since dyn_array_at returns NULL when going out of bounds on the array, we know that when we exceed the bounds of the array we are done.
    while(element_in_use != NULL) {
        element_in_use->started = true;
        //Saving how long the process had to wait before being executed
        unsigned long waiting_time = *total_run_time - element_in_use->arrival;
        // we execute it until it is complete.
        while (element_in_use->remaining_burst_time > 0) {
            //Simulate CPU Cycle, increment runtime
            virtual_cpu(element_in_use);
            (*total_run_time)++;
        }
        //Calculating turnaround time with total runtime - arrival time
        unsigned long turnaround_time = *total_run_time - element_in_use->arrival;
        //Saving sum of total calculations, needed for average later
        *turnaround_time_sum += turnaround_time;
        *waiting_time_sum += waiting_time;
        //Actually remove the given element that we just processed from the ready queue.
        dyn_array_erase(ready_queue,indexToRemove);
        //Grab the next element to process, which was one before the index we just processed. This is assuming that there is another element to process that arrived at the same time.
        element_in_use = dyn_array_at(ready_queue, --indexToRemove);
    }
}
//Implementing FCFS process scheduling algorithm
bool first_come_first_serve(dyn_array_t *ready_queue, ScheduleResult_t *result)
{
    //Error checking
    if(ready_queue == NULL || result == NULL) {
        return false;
    }
    //creating variables needed for calculations later
    unsigned long total_run_time = 0;
    int total_processes = dyn_array_size(ready_queue);
    float waiting_time_sum = 0;
    float turnaround_time_sum = 0;
    //Sort the array based on arrival time so we can actually process based on which arrived first
    //Since the given dyn_array function is not guaranteed to be stable based on qsort. I added a stable sort function to the library which is based on Timsort https://en.wikipedia.org/wiki/Timsort
    //I got the implementation of this from https://github.com/patperry/timsort/tree/master
    dyn_array_stable_sort(ready_queue,compare_arrival);
    //Since the array is sorted, the element at the front of the array has the smallest arrival time.
    uint32_t currentArrival =((ProcessControlBlock_t*) dyn_array_front(ready_queue))->arrival;
    //Loop until all elements have been processed
    while(dyn_array_empty(ready_queue) != true) {
        //Loop through the array to find all processes that have the same arrival time and process those by popping off the back of that list
        for(size_t i = 0; i < dyn_array_size(ready_queue);i++) {
            ProcessControlBlock_t * current = dyn_array_at(ready_queue,i);
            //If we find an element with a different arrival time than the original arrival time, we can now process the list with the original arrival time.
            if(current->arrival != currentArrival) {
                //that list starts at index - 1 and goes down from there (if necessary)
                size_t indexToRemove = i-1;
                //Get the first element to be processed and call the process function
                ProcessControlBlock_t* element_in_use = dyn_array_at(ready_queue,indexToRemove);
                process_fcfs(ready_queue,element_in_use,&total_run_time,&turnaround_time_sum,&waiting_time_sum,indexToRemove);
                //Update the arrival that now needs to be processed and break out of the for loop to repeat the process.
                currentArrival = current->arrival;
                break;
            }
            //If we don't find an element with a different arrival time, but we're at the end of the array, then we also need to remove from the list
            else if((i+1) == dyn_array_size(ready_queue)) {
                //So we remove from the very end of the array and go down from there following an identical process as above.
                size_t indexToRemove = i ;
                ProcessControlBlock_t* element_in_use = dyn_array_at(ready_queue,indexToRemove);
                process_fcfs(ready_queue,element_in_use,&total_run_time,&turnaround_time_sum,&waiting_time_sum,indexToRemove);
                break;
            }
        }
    }
    //Calculating given return elements
    result->total_run_time = total_run_time;
    result->average_waiting_time = waiting_time_sum / total_processes;
    result->average_turnaround_time = turnaround_time_sum / total_processes;
    return true;
}
int compare_burst(const void *a, const void *b) {
    const ProcessControlBlock_t * arr1 = a;
    const ProcessControlBlock_t * arr2 = b;
    return arr1->remaining_burst_time - arr2->remaining_burst_time;
}
//This function will add elements with a valid arrival time to the active queue
void populate_active_queue(dyn_array_t* ready_queue, dyn_array_t* active_queue, unsigned long current_time) {
    for(size_t i = 0; i < dyn_array_size(ready_queue);i++) {
        ProcessControlBlock_t* activePCB = dyn_array_at(ready_queue, i);
        //If the given PCB has an arrival time less than the program's active tracked time, move it to the active queue
        if(activePCB->arrival <= current_time) {
            //Add to active, remove from ready
            dyn_array_push_back(active_queue,activePCB);
            dyn_array_erase(ready_queue, i);
            //If dealing with multiple elements that need to be added at once, removing one will cause issues with deleting from ready_queue
            //As the indicies will change when removing, so we just call the function recursivlely to get any other valid elements. We then break & complete the function when done
            populate_active_queue(ready_queue,active_queue,current_time);
            break;
        }
    }
}
//Function that will actually be processing the individual elements
void process_sjf(dyn_array_t* active_queue, ProcessControlBlock_t* element_in_use, unsigned long* total_run_time, float* turnaround_time_sum, float* waiting_time_sum, size_t indexToRemove) {
    element_in_use->started = true;
    //Saving how long the process had to wait before being executed
    unsigned long waiting_time = *total_run_time - element_in_use->arrival;
    // we execute it until it is complete.
    while (element_in_use->remaining_burst_time > 0) {
        //Simulate CPU Cycle, increment runtime
        virtual_cpu(element_in_use);
        (*total_run_time)++;
    }
    //Calculating turnaround time with total runtime - arrival time
    unsigned long turnaround_time = *total_run_time - element_in_use->arrival;
    //Saving sum of total calculations, needed for average later
    *turnaround_time_sum += turnaround_time;
    *waiting_time_sum += waiting_time;
    //Actually remove the given element that we just processed from the ready queue.
    dyn_array_erase(active_queue,indexToRemove);
    //Grab the next element to process, which was one before the index we just processed. This is assuming that there is another element to process that arrived at the same time.
    element_in_use = dyn_array_at(active_queue, --indexToRemove);
}
bool shortest_job_first(dyn_array_t *ready_queue, ScheduleResult_t *result) 
{
    //error checking
    if(ready_queue == NULL || result == NULL) {
        return false;
    }
    //If ready_queue is empty, then we are done immediately and return
    if(dyn_array_empty(ready_queue)==true) {
        result->average_turnaround_time = 0;
        result->total_run_time = 0;
        result->average_waiting_time = 0;
        return true;
    }
    //creating variables needed for calculations later
    unsigned long total_run_time = 0;
    int total_processes = dyn_array_size(ready_queue);
    float waiting_time_sum = 0;
    float turnaround_time_sum = 0;
    //Sort the array based on arrival time so we can actually process based on which arrived first. We can't process one that arrives in the future..
    //Since the given dyn_array function is not guaranteed to be stable based on qsort. I added a stable sort function to the library which is based on Timsort https://en.wikipedia.org/wiki/Timsort
    //I got the implementation of this from https://github.com/patperry/timsort/tree/master
    dyn_array_stable_sort(ready_queue,compare_arrival);
    //Create active queue which will only hold elments that are valid (i.e. their arrival hasn't happened yet)
    //To start this will only get elements with arrival 0
    dyn_array_t* active_queue = dyn_array_create(0,sizeof(ProcessControlBlock_t),NULL);
    //Populate active queue by taking elements out of ready queue
    populate_active_queue(ready_queue,active_queue,total_run_time);
    //If the active array is empty after first populating it, this means there are no processes with an arrival of 0, so we increment the runtime and retry to add until we get a process
    while(dyn_array_empty(active_queue) == true) {
        total_run_time++;
        populate_active_queue(ready_queue,active_queue,total_run_time);
    }
    while(dyn_array_empty(active_queue) != true) {
        //Sort based on arrival times. We know that we will pull from the front of the array since we want shortest processes first
        dyn_array_stable_sort(active_queue, compare_burst);
        uint32_t currentBurst =((ProcessControlBlock_t*) dyn_array_front(active_queue))->remaining_burst_time;
        //Since objects with the same burst time have their order preserved, we need to extract the element with the last different burst time and remove down from there.
        //This follows very similar logic from FCFS, except we are going off of burst time instead of arrival time.
        for(size_t i = 0; i < dyn_array_size(active_queue);i++) {
            ProcessControlBlock_t *current = dyn_array_at(active_queue, i);
            //If we find an element with a different burst time than the original burst time, we can now process the list with the original burst time.
            if (current->remaining_burst_time != currentBurst) {
                //that list starts at index - 1 and goes down from there (if necessary)
                size_t indexToRemove = i - 1;
                //Get the first element to be processed and call the process function
                ProcessControlBlock_t *element_in_use = dyn_array_at(active_queue, indexToRemove);
                process_sjf(active_queue, element_in_use, &total_run_time, &turnaround_time_sum, &waiting_time_sum,
                            indexToRemove);
                //Since time has passed, we try and see if we can add to our active queue.
                populate_active_queue(ready_queue, active_queue, total_run_time);
                break;
            }
                //If we don't find an element with a different burst time, then we're at the end of the array, and we need to remove from the back of the list
            else if ((i + 1) == dyn_array_size(active_queue)) {
                //So we remove from the very end of the array and go down from there following an identical process as above.
                size_t indexToRemove = i;
                ProcessControlBlock_t *element_in_use = dyn_array_at(active_queue, indexToRemove);
                process_sjf(active_queue, element_in_use, &total_run_time, &turnaround_time_sum, &waiting_time_sum,
                            indexToRemove);
                //Since time has passed, we try and see if we can add to our active queue.
                populate_active_queue(ready_queue, active_queue, total_run_time);
                break;
            }
        }
    }
    result->total_run_time = total_run_time;
    result->average_waiting_time = waiting_time_sum / total_processes;
    result->average_turnaround_time = turnaround_time_sum / total_processes;
    dyn_array_destroy(active_queue);
    return true;   
}
int compare_priority(const void *a, const void *b) {
    const ProcessControlBlock_t * arr1 = a;
    const ProcessControlBlock_t * arr2 = b;
    return arr1->priority - arr2->priority;
}
//Function that will actually be processing the individual elements
void process_priority(dyn_array_t* active_queue, ProcessControlBlock_t* element_in_use, unsigned long* total_run_time, float* turnaround_time_sum, float* waiting_time_sum, size_t indexToRemove) {
    element_in_use->started = true;
    //Saving how long the process had to wait before being executed
    unsigned long waiting_time = *total_run_time - element_in_use->arrival;
    // we execute it until it is complete.
    while (element_in_use->remaining_burst_time > 0) {
        //Simulate CPU Cycle, increment runtime
        virtual_cpu(element_in_use);
        (*total_run_time)++;
    }
    //Calculating turnaround time with total runtime - arrival time
    unsigned long turnaround_time = *total_run_time - element_in_use->arrival;
    //Saving sum of total calculations, needed for average later
    *turnaround_time_sum += turnaround_time;
    *waiting_time_sum += waiting_time;
    //Actually remove the given element that we just processed from the ready queue.
    dyn_array_erase(active_queue,indexToRemove);
    //Grab the next element to process, which was one before the index we just processed. This is assuming that there is another element to process that arrived at the same time.
    element_in_use = dyn_array_at(active_queue, --indexToRemove);
}
bool priority(dyn_array_t *ready_queue, ScheduleResult_t *result) 
{
    //error checking
    if(ready_queue == NULL || result == NULL) {
        return false;
    }
    //If ready_queue is empty, then we are done immediately and return
    if(dyn_array_empty(ready_queue)==true) {
        result->average_turnaround_time = 0;
        result->total_run_time = 0;
        result->average_waiting_time = 0;
        return true;
    }
    //creating variables needed for calculations later
    unsigned long total_run_time = 0;
    int total_processes = dyn_array_size(ready_queue);
    float waiting_time_sum = 0;
    float turnaround_time_sum = 0;
    //Sort the array based on arrival time so we can actually process based on which arrived first. We can't process one that arrives in the future..
    //Since the given dyn_array function is not guaranteed to be stable based on qsort. I added a stable sort function to the library which is based on Timsort https://en.wikipedia.org/wiki/Timsort
    //I got the implementation of this from https://github.com/patperry/timsort/tree/master
    dyn_array_stable_sort(ready_queue,compare_arrival);
    //Create active queue which will only hold elments that are valid (i.e. their arrival hasn't happened yet)
    //To start this will only get elements with arrival 0
    dyn_array_t* active_queue = dyn_array_create(0,sizeof(ProcessControlBlock_t),NULL);
    //Populate active queue by taking elements out of ready queue
    populate_active_queue(ready_queue,active_queue,total_run_time);
    //If the active array is empty after first populating it, this means there are no processes with an arrival of 0, so we increment the runtime and retry to add until we get a process
    while(dyn_array_empty(active_queue) == true) {
        total_run_time++;
        populate_active_queue(ready_queue,active_queue,total_run_time);
    }
    while(dyn_array_empty(active_queue) != true) {
        //Sort based on priority. We know that we will pull from the front of the array since we want highest priority first
        dyn_array_stable_sort(active_queue, compare_priority);
        uint32_t currentPriority =((ProcessControlBlock_t*) dyn_array_front(active_queue))->priority;
        //Since objects with the same burst time have their order preserved, we need to extract the element with the last different burst time and remove down from there.
        //This follows very similar logic from FCFS, except we are going off of burst time instead of arrival time.
        for(size_t i = 0; i < dyn_array_size(active_queue);i++) {
            ProcessControlBlock_t *current = dyn_array_at(active_queue, i);
            //If we find an element with a different burst time than the original burst time, we can now process the list with the original burst time.
            if (current->priority != currentPriority) {
                //that list starts at index - 1 and goes down from there (if necessary)
                size_t indexToRemove = i - 1;
                //Get the first element to be processed and call the process function
                ProcessControlBlock_t *element_in_use = dyn_array_at(active_queue, indexToRemove);
                process_priority(active_queue, element_in_use, &total_run_time, &turnaround_time_sum, &waiting_time_sum,
                            indexToRemove);
                //Since time has passed, we try and see if we can add to our active queue.
                populate_active_queue(ready_queue, active_queue, total_run_time);
                break;
            }
                //If we don't find an element with a different burst time, then we're at the end of the array, and we need to remove from the back of the list
            else if ((i + 1) == dyn_array_size(active_queue)) {
                //So we remove from the very end of the array and go down from there following an identical process as above.
                size_t indexToRemove = i;
                ProcessControlBlock_t *element_in_use = dyn_array_at(active_queue, indexToRemove);
                process_priority(active_queue, element_in_use, &total_run_time, &turnaround_time_sum, &waiting_time_sum,
                            indexToRemove);
                //Since time has passed, we try and see if we can add to our active queue.
                populate_active_queue(ready_queue, active_queue, total_run_time);
                break;
            }
        }
    }
    result->total_run_time = total_run_time;
    result->average_waiting_time = waiting_time_sum / total_processes;
    result->average_turnaround_time = turnaround_time_sum / total_processes;
    dyn_array_destroy(active_queue);
    return true;     
}
//This function will add elements with a valid arrival time to the active queue for an in progress round robin. With RR, we pull off the back of the queue, and add to the front, so the only difference
//between this function and populate_active_queue is that we push to the front instead of the back
void populate_active_queue_rr(dyn_array_t* ready_queue, dyn_array_t* active_queue, unsigned long current_time) {
    for(size_t i = 0; i < dyn_array_size(ready_queue);i++) {
        ProcessControlBlock_t* activePCB = dyn_array_at(ready_queue, i);
        //If the given PCB has an arrival time less than the program's active tracked time, move it to the active queue
        if(activePCB->arrival <= current_time) {
            //Add to active, remove from ready
            //Push to front instead of back as normal
            dyn_array_push_front(active_queue,activePCB);
            dyn_array_erase(ready_queue, i);
            //If dealing with multiple elements that need to be added at once, removing one will cause issues with deleting from ready_queue
            //As the indicies will change when removing, so we just call the function recursivlely to get any other valid elements. We then break & complete the function when done
            populate_active_queue_rr(ready_queue,active_queue,current_time);
            break;
        }
    }
}
//helper function to process each time quantum
void process_roundrobin(dyn_array_t* active_queue, ProcessControlBlock_t* element_in_use, unsigned long* total_run_time, float* turnaround_time_sum, float* waiting_time_sum, size_t quantum, dyn_array_t* ready_queue) {
    element_in_use->started = true;
    size_t quantumCount = 0;
    //Saving how long the process had to wait before being executed. Since the process could have been previously started, we need to subtract from the time the process was last executed.
    //This is being stored in priority because if I add any elements to ProcessControlBlock, my code compiles, but the compiler gives warnings that the new variable is not initalized in any of the test functions.
    //For this reason, and because we don't use priority for RR, I just store the data in the variable
    long waiting_time = *total_run_time - element_in_use->priority;
    // we execute it until it is complete, or time quantum runs out.
    while (element_in_use->remaining_burst_time > 0) {
        if(quantum == quantumCount) {
            //read to end of active queue.
            //Update the time that the process ran to.
            element_in_use->priority = *total_run_time;
            //Add any new elements that have since arrived to the back of the queue
            populate_active_queue_rr(ready_queue,active_queue,*total_run_time);
            //Push element onto the back of the line of elements to process.
            dyn_array_push_front(active_queue,element_in_use);
            //Add any waiting that was done during this time.
            *waiting_time_sum += waiting_time;
            return;
        }
        //Simulate CPU Cycle, increment runtime
        virtual_cpu(element_in_use);
        (*total_run_time)++;
        quantumCount++;
    }
    //Calculating turnaround time with total runtime - arrival time
    unsigned long turnaround_time = *total_run_time - element_in_use->arrival;
    //Saving sum of total calculations, needed for average later
    *turnaround_time_sum += turnaround_time;
    //Need to subtract arrival time from waiting time.
    waiting_time -= element_in_use->arrival;
    *waiting_time_sum += waiting_time;
}

bool round_robin(dyn_array_t *ready_queue, ScheduleResult_t *result, size_t quantum) 
{
    //error checking
    if(ready_queue == NULL || result == NULL) {
        return false;
    }
    //If ready_queue is empty, then we are done immediately and return
    if(dyn_array_empty(ready_queue)==true) {
        result->average_turnaround_time = 0;
        result->total_run_time = 0;
        result->average_waiting_time = 0;
        return true;
    }
    //creating variables needed for calculations later
    unsigned long total_run_time = 0;
    int total_processes = dyn_array_size(ready_queue);
    float waiting_time_sum = 0;
    float turnaround_time_sum = 0;
    for(int i = 0; i < total_processes; i++) {
        ProcessControlBlock_t *element_in_use = dyn_array_at(ready_queue, i);
        //Saving how long the process had to wait before being executed. Since the process could have been previously started, we need to subtract from the time the process was last executed.
        //This is being stored in priority because if I add any elements to ProcessControlBlock, my code compiles, but the compiler gives warnings that the new variable is not initalized in any of the test functions.
        //For this reason, and because we don't use priority for RR, I just store the data in the variable.
        //We set it equal to 0 to get rid of any data that was originally stored there.
        element_in_use->priority = 0;
    }
    //Sort the array based on arrival time so we can actually process based on which arrived first. We can't process one that arrives in the future..
    //Since the given dyn_array function is not guaranteed to be stable based on qsort. I added a stable sort function to the library which is based on Timsort https://en.wikipedia.org/wiki/Timsort
    //I got the implementation of this from https://github.com/patperry/timsort/tree/master
    dyn_array_stable_sort(ready_queue,compare_arrival);
    //Create active queue which will only hold elments that are valid (i.e. their arrival hasn't happened yet)
    //To start this will only get elements with arrival 0
    dyn_array_t* active_queue = dyn_array_create(0,sizeof(ProcessControlBlock_t),NULL);
    //Populate active queue by taking elements out of ready queue
    populate_active_queue(ready_queue,active_queue,total_run_time);
    //If the active array is empty after first populating it, this means there are no processes with an arrival of 0, so we increment the runtime and retry to add until we get a process
    while(dyn_array_empty(active_queue) == true) {
        total_run_time++;
        populate_active_queue(ready_queue,active_queue,total_run_time);
    }
while(dyn_array_empty(active_queue) != true) {
        ProcessControlBlock_t active;
        //Pull from the back, which is the next element to be processed in order
        dyn_array_extract_back(active_queue,&active);
        process_roundrobin(active_queue,&active,&total_run_time,&turnaround_time_sum,&waiting_time_sum,quantum,ready_queue);
        //Add any new elements to the queue.
        populate_active_queue_rr(ready_queue,active_queue,total_run_time);
    }
    result->total_run_time = total_run_time;
    result->average_waiting_time = waiting_time_sum / total_processes;
    result->average_turnaround_time = turnaround_time_sum / total_processes;
    dyn_array_destroy(active_queue);
    return true;     
}

dyn_array_t *load_process_control_blocks(const char *input_file) 
{
    //Error checking
    if(input_file==NULL) {
        return NULL;
    }
    //Opening file and checking for errors with it
    int fd = open(input_file,O_RDONLY);
    if(fd == -1) {
        return NULL;
    }
    uint32_t pcbsize;
    //read first 4 bytes of file to pull out size.
    ssize_t numread = read(fd,&pcbsize,4);
    //Checknig for errors with reading from the file
    if(numread == -1 || numread == 0) {
        close(fd);
        return NULL;
    }
    //Actually creating the array and making sure it got created sucessfully
    dyn_array_t* arr = dyn_array_create(pcbsize,sizeof(ProcessControlBlock_t),NULL);
    if(arr == NULL) {
        close(fd);
        return NULL;
    }
    //Looping through the size of the array
    for(unsigned long i = 0; i<pcbsize; i++) {
        uint32_t buffer;
        //Read into a 4 byte buffer to save each element.
        numread = read(fd,&buffer, 4);
        //If we read less than 4 bytes, then something went wrong, so destroy the array and return NULL indicating error
        if(numread < 4) {
            dyn_array_destroy(arr);
            close(fd);
            return NULL;
        }
        //Creating a temp processcontrol block to save to and saving the burst time that was just read to it
        ProcessControlBlock_t temp;
        temp.remaining_burst_time = buffer;
        //read in the next element, priority, into the buffer and check for errors again
        numread = read(fd,&buffer, 4);
        if(numread < 4) {
            dyn_array_destroy(arr);
            close(fd);
            return NULL;
        }
        //save to temp
        temp.priority = buffer;
        //Get last variable from file into buffer and save to temp
        numread = read(fd,&buffer, 4);
        if(numread < 4) {
            dyn_array_destroy(arr);
            close(fd);
            return NULL;
        }
        temp.arrival = buffer;
        //set the other variable in ProcessControlBlock to false since no process has started yet
        temp.started = false;
        //Finally push the element onto the back of the array.
        dyn_array_push_back(arr,&temp);
    }
    close(fd);
    //return the created array when done
    return arr;
}
void process_srtf(dyn_array_t* active_queue, ProcessControlBlock_t* element_in_use, unsigned long* total_run_time, float* turnaround_time_sum, float* waiting_time_sum, dyn_array_t* ready_queue) {
    element_in_use->started = true;
    //Saving how long the process had to wait before being executed. Since the process could have been previously started, we need to subtract from the time the process was last executed.
    //This is being stored in priority because if I add any elements to ProcessControlBlock, my code compiles, but the compiler gives warnings that the new variable is not initalized in any of the test functions.
    //For this reason, and because we don't use priority for SRTF, I just store the data in the variable
    long waiting_time = *total_run_time - element_in_use->priority;
    // we execute it until it is complete, or we find element with smaller remaining burst time.
    while (element_in_use->remaining_burst_time > 0) {
        //Simulate CPU Cycle, increment runtime
        virtual_cpu(element_in_use);
        (*total_run_time)++;
        //Add any new elements to the active queue since time has passed
        populate_active_queue(ready_queue,active_queue,*total_run_time);
        //Resort based on burst, smallest  burst will be at front
        dyn_array_stable_sort(active_queue,compare_burst);
        ProcessControlBlock_t* PotentialNext = dyn_array_front(active_queue);
        //If the queue isn't empty and the potential next has a smaller burst, we stop processing the current element to process that one instead.
        if(dyn_array_size(active_queue) != 0 && PotentialNext->remaining_burst_time < element_in_use->remaining_burst_time) {
            //Update the time that the process ran to.
            element_in_use->priority = *total_run_time;
            //Push original element onto the back of the line of elements to process.
            dyn_array_push_back(active_queue,element_in_use);
            //Add any waiting that was done during this time.
            *waiting_time_sum += waiting_time;
            return;
        }
    }
    //Calculating turnaround time with total runtime - arrival time
    unsigned long turnaround_time = *total_run_time - element_in_use->arrival;
    //Saving sum of total calculations, needed for average later
    *turnaround_time_sum += turnaround_time;
    //Need to subtract arrival time from waiting time.
    waiting_time -= element_in_use->arrival;
    *waiting_time_sum += waiting_time;
}

bool shortest_remaining_time_first(dyn_array_t *ready_queue, ScheduleResult_t *result) 
{
    //error checking
    if(ready_queue == NULL || result == NULL) {
        return false;
    }
    //If ready_queue is empty, then we are done immediately and return
    if(dyn_array_empty(ready_queue)==true) {
        result->average_turnaround_time = 0;
        result->total_run_time = 0;
        result->average_waiting_time = 0;
        return true;
    }
    //creating variables needed for calculations later
    unsigned long total_run_time = 0;
    int total_processes = dyn_array_size(ready_queue);
    float waiting_time_sum = 0;
    float turnaround_time_sum = 0;
    for(int i = 0; i < total_processes; i++) {
        ProcessControlBlock_t *element_in_use = dyn_array_at(ready_queue, i);
        //Saving how long the process had to wait before being executed. Since the process could have been previously started, we need to subtract from the time the process was last executed.
        //This is being stored in priority because if I add any elements to ProcessControlBlock, my code compiles, but the compiler gives warnings that the new variable is not initalized in any of the test functions.
        //For this reason, and because we don't use priority for SRTF, I just store the data in the variable.
        //We set it equal to 0 to get rid of any data that was originally stored there.
        element_in_use->priority = 0;
    }
    //Sort the array based on arrival time so we can actually process based on which arrived first. We can't process one that arrives in the future..
    //Since the given dyn_array function is not guaranteed to be stable based on qsort. I added a stable sort function to the library which is based on Timsort https://en.wikipedia.org/wiki/Timsort
    //I got the implementation of this from https://github.com/patperry/timsort/tree/master
    dyn_array_stable_sort(ready_queue,compare_arrival);
    //Create active queue which will only hold elments that are valid (i.e. their arrival hasn't happened yet)
    //To start this will only get elements with arrival 0
    dyn_array_t* active_queue = dyn_array_create(0,sizeof(ProcessControlBlock_t),NULL);
    //Populate active queue by taking elements out of ready queue
    populate_active_queue(ready_queue,active_queue,total_run_time);
    //If the active array is empty after first populating it, this means there are no processes with an arrival of 0, so we increment the runtime and retry to add until we get a process
    while(dyn_array_empty(active_queue) == true) {
        total_run_time++;
        populate_active_queue(ready_queue,active_queue,total_run_time);
    }
while(dyn_array_empty(active_queue) != true) {
        //Sort based on burst time. Pull from the front, which has smallest burst time fulfulling SRTF
        dyn_array_stable_sort(active_queue, compare_burst);
        ProcessControlBlock_t active;
        //Pull from the front, which is the next element to be processed
        dyn_array_extract_front(active_queue,&active);
        process_srtf(active_queue,&active,&total_run_time,&turnaround_time_sum,&waiting_time_sum,ready_queue);
        //Add any new elements to the queue.
        populate_active_queue(ready_queue,active_queue,total_run_time);
    }
    result->total_run_time = total_run_time;
    result->average_waiting_time = waiting_time_sum / total_processes;
    result->average_turnaround_time = turnaround_time_sum / total_processes;
    dyn_array_destroy(active_queue);
    return true;    
}

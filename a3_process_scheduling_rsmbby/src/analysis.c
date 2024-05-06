#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "dyn_array.h"
#include "process_scheduling.h"

#define FCFS    "FCFS"
#define P       "P"
#define RR      "RR"
#define SJF     "SJF"
#define SRTF    "SRTF"

// Add and comment your analysis code in this function.
int main(int argc, char **argv) 
{
    if (argc < 3) 
    {
        printf("%s <pcb file> <schedule algorithm> [quantum]\n", argv[0]);
        return EXIT_FAILURE;
    }
    //Pulling out data from the command line.
    char* filename = argv[1];
    char* algorithm = argv[2];
    char* timequantum = NULL;
    //Using time library to calculate the timing of each function
    clock_t t;
    size_t quantum = 0;
    //Load ready queue using load pcb function
    dyn_array_t* pcbs = load_process_control_blocks(filename);
    if(pcbs == NULL) {
        printf("Could not open the file, or it was formatted incorrectly!\n");
        return EXIT_FAILURE;
    }
    bool res;
    //Allocate space for result
    ScheduleResult_t* sr = calloc(1,sizeof(ScheduleResult_t));
    //If algorithm is round robin...
    if(strncmp(RR,algorithm,2)==0) {
        //Pull out time quantum
        timequantum = argv[3];
        if(timequantum == NULL) {
            printf("%s <pcb file> <schedule algorithm> [quantum]\n", argv[0]);
            free(sr);
            return EXIT_FAILURE;
        }
        //convert to long
        quantum = strtol(timequantum,NULL,10);
        if(quantum == 0) {
            printf("Invalid time quantum!\n");
            free(sr);
            return EXIT_FAILURE;
        }
        t = clock();
        //Run round robin with given paramters and calculate timing.
        res = round_robin(pcbs,sr,quantum);
        t = clock() - t;
    }
    //First come first serve
    else if(strncmp(FCFS,algorithm,4)==0) {
        //calculating using FCFS function with given parameters
        t = clock();
        res = first_come_first_serve(pcbs,sr);
        t = clock() - t;
    }
    else if(strncmp(P,algorithm,1)==0) {
        //Calculating using Priority function with given parameters
        t = clock();
        res = priority(pcbs,sr);
        t = clock() - t;
    }
    else if(strncmp(SJF,algorithm,3)==0) {
        //Calculating using SJF function with given parameters
        t = clock();
        res = shortest_job_first(pcbs,sr);
        t = clock() - t;
    }
    else if(strncmp(SRTF,algorithm,4)==0) {
        //Calculating using SRTF function with given parameters
        t = clock();
        res = shortest_remaining_time_first(pcbs,sr);
        t = clock() - t;
    }
    else {
        //If we couldn't find based on the given input, set the result to false to indicate error
        res = false;
    }
    if(res == false) {
        printf("An error occured while processing!\n");
        free(sr);
        return EXIT_FAILURE;
    }
    //Actually calculate time taken
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
    //This is if algorithm was not round robin, to print all relevant data
    if(timequantum == NULL){
        printf("\n%s Processing:\n\nAverage Turnaround Time: %.2f\nAverage Wait Time: %.2f\nTotal Run Time: %ld\nTime to process (calculated in seconds): %.6f\n",algorithm,sr->average_turnaround_time,
        sr->average_waiting_time,sr->total_run_time,time_taken);
    }
    else {
        //Otherwise, print round robin data (with quantum)
        printf("\n%s Processing with quantum of %ld:\n\nAverage Turnaround Time: %.2f\nAverage Wait Time: %.2f\nTotal Run Time: %ld\nTime to process (calculated in seconds): %.6f\n",algorithm,quantum,sr->average_turnaround_time,
        sr->average_waiting_time,sr->total_run_time,time_taken);
    }
    //Open readme file for appending
    FILE* fp = fopen("../readme.md","a+");
    //If opened sucessfully, write same information that was printed, to the file
    if(fp != NULL) {
        if(timequantum == NULL){
            fprintf(fp,"\n\n**%s Processing:**\n\nAverage Turnaround Time: %.2f\n\nAverage Wait Time: %.2f\n\nTotal Run Time: %ld\n\nTime to process (calculated in seconds): %.6f\n\n",algorithm,sr->average_turnaround_time,
            sr->average_waiting_time,sr->total_run_time,time_taken);
        }
        else {
            fprintf(fp,"\n\n**%s Processing with quantum of %ld:**\n\n\nAverage Turnaround Time: %.2f\n\nAverage Wait Time: %.2f\n\nTotal Run Time: %ld\n\nTime to process (calculated in seconds): %.6f\n\n",algorithm,quantum,sr->average_turnaround_time,
            sr->average_waiting_time,sr->total_run_time,time_taken);
        }
        fclose(fp);
    }
    //Free allocated pointer when done.
    free(sr);
    dyn_array_destroy(pcbs);
    return EXIT_SUCCESS;
}

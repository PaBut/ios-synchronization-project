#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>
#include <time.h>

// #define MAX

int main(int argc, char** argv){
    int customer_count = 0;
    int worker_count = 0;
    int max_waiting_time = 0;
    int max_break_time = 0;
    int max_closing_time = 0;
    
    if(argc != 6){
        fprintf(stderr, "Error: Input must be in this format: NZ NU TZ TU F\n");
        return 1;
    }

    if((customer_count = atoi(argv[1])) == 0){
        fprintf(stderr, "Error: Customer count must be an int value\n");
        return 1;
    }
    if((worker_count = atoi(argv[2])) == 0){
        fprintf(stderr, "Error: Worker count must be an int value\n");
        return 1;
    }
    if((max_waiting_time = atoi(argv[3])) == 0 || 
    max_waiting_time < 0 || max_waiting_time > 10000 ){
        fprintf(stderr, "Error: Maximal customer waiting time must be an int value in range [0;10000]\n");
        return 1;
    }
    if((max_break_time = atoi(argv[4])) == 0 || 
    max_break_time < 0 || max_break_time > 100 ){
        fprintf(stderr, "Error: Maximal worker break time must be an int value in range [0;100]\n");
        return 1;
    }

    if((max_closing_time = atoi(argv[5])) == 0 || 
    max_closing_time < 0 || max_closing_time > 10000 ){
        fprintf(stderr, "Error: Maximal closing time must be an int value in range [0;10000]\n");
        return 1;
    }

    


    return 0;
}
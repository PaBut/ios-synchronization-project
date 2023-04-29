#define _XOPEN_SOURCE 500 // For usleep
#define _GNU_SOURCE       // For MAP_ANONYMOUS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <stdbool.h>

// Shared memory variables
int* msg_id = NULL;
int* total_customer_count = NULL;
int** queue_customer_count = NULL;
int* workers_ready = NULL;
bool* closed = NULL;

// Command line arguments
int NZ = 0; // Customer count
int NU = 0; // Worker count
int TZ = 0; // Maximal waiting time to enter the office
int TU = 0; // Maximal worker break time
int F = 0;  // Maximal working time of the office, after which no customer can enter the shop

// File for writing the output
FILE* file = NULL;

// Semaphores
sem_t** customer_queue;
sem_t* customer_ready;
sem_t* mutex;
sem_t* print_semaphore;

// Function for printing logs in the file
void print(const char* message, ...){
    va_list args;
    va_start(args, message);
    sem_wait(print_semaphore);
    fprintf(file ,"%d: ", (*msg_id)++);
    vfprintf(file, message, args);
    fflush(file);
    sem_post(print_semaphore);
    va_end(args);
}

void initialize(){
    // Initialization of shared memory
    if((msg_id = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (total_customer_count = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (workers_ready = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (queue_customer_count = mmap(NULL, sizeof(int*), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (closed = mmap(NULL, sizeof(bool), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if((queue_customer_count[i] = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED){
            perror("mmap");
            exit(EXIT_FAILURE);
        }
        *queue_customer_count[i] = 0;
    }

    *msg_id = 1;
    *total_customer_count = 0;
    *workers_ready = 0;
    *closed = false;

    // Opening the file for output
    if((file = fopen("proj2.out", "w")) == NULL){
        fprintf(stderr, "Error: File opening failed\n");
        exit(EXIT_FAILURE);
    }


    // Semaphore initialization
    if((customer_queue = mmap(NULL, sizeof(sem_t*), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED ||
    (customer_ready = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED ||
    (mutex = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED ||
    (print_semaphore = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if((customer_queue[i] = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED){
            perror("mmap");
            exit(EXIT_FAILURE);
        }
    }

    if(sem_init(customer_ready, 1, 0) == -1 ||
    sem_init(mutex, 1, 1) == -1 ||
    sem_init(print_semaphore, 1, 1) == -1){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if(sem_init(customer_queue[i], 1, 0) == -1){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }
    }
}

// Function for deinitialization of shared memory, destroying semaphores and closing log file 
void deinitialize(){
    fclose(file);

    if(munmap(msg_id, sizeof(int)) == -1 ||
    munmap(total_customer_count, sizeof(int)) ||
    munmap(workers_ready, sizeof(int)) ||
    munmap(closed, sizeof(bool))){
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if(sem_destroy(customer_queue[i]) == -1){
            perror("sem_destroy");
            exit(EXIT_FAILURE);
        }
        if(munmap(queue_customer_count[i], sizeof(int)) == -1){
            perror("munmap");
            exit(EXIT_FAILURE);
        }
    }

    if(munmap(queue_customer_count, sizeof(int*)) == -1){
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    if(sem_destroy(customer_ready) ||
    sem_destroy(mutex) ||
    sem_destroy(print_semaphore)){
        perror("sem_destroy");
        exit(EXIT_FAILURE);
    }
}

void customer_process(int id){
    srand(time(NULL) * getpid());
    print("Z %d: started\n", id);
    usleep((rand() % (TZ + 1)) * 1000);                 // Waiting for <0;TZ> ms to enter the office
    int chosen_service = 0;
    sem_wait(mutex);
    if(*closed){                                        // If post office is closed, customer will go home
        print("Z %d: going home\n", id);
        sem_post(mutex);
        return;
    }else{                                              // If post office is open, customer will choose activity <1;3> 
        chosen_service = rand() % 3;
        print("Z %d: entering office for a service %d\n", id, chosen_service + 1);
        (*total_customer_count)++;                      // Appending customer to the specific queue
        (*queue_customer_count)[chosen_service]++;
        sem_post(mutex);
    }

    sem_wait(customer_queue[chosen_service]);           // Waiting to be called by worker

    sem_post(customer_ready);                           // Customer ready to served
    
    print("Z %d: called by office worker\n", id); 
    usleep((rand() % 11) * 1000);                       // Being served by the worker for <0;10> ms   
    print("Z %d: going home\n", id);

}

void worker_process(int id){
    srand(time(NULL) * getpid());
    print("U %d: started\n", id);
    while(true){
        sem_wait(mutex);
        if(*workers_ready + 1 <= *total_customer_count){ // Going to serve a customer only if ready workers isn't enough to serve all customers  
            (*workers_ready)++;
            int chosen_queue;
            do{                                         // Worker chooses random non empty queue
                chosen_queue = rand() % 3;
            }while((*queue_customer_count)[chosen_queue] <= 0);

            sem_post(customer_queue[chosen_queue]);     // Sends signal for calling the customer from chosen queue
            (*queue_customer_count)[chosen_queue]--;    // Pops the customer from the queue 
            (*total_customer_count)--;
            (*workers_ready)--;                         // Decrementing count of workers ready to serve customers in queue
            sem_post(mutex);

            sem_wait(customer_ready);                   // Waiting for customer to be ready to start the serving process simultaneously with the customer  

            print("U %d: serving a service of type %d\n", id, chosen_queue + 1);

            usleep((rand() % 11) * 1000);               // Serving the customer for <0;10> ms
            print("U %d: service finished\n", id);
        }else{
            sem_post(mutex);
        }

        sem_wait(mutex);
        if(*total_customer_count == 0)                  // If there's no waiting customers, taking a break
        {
            if(*closed){                                // If in addition to it the office is closed, then going home
                sem_post(mutex);
                break;
            }
            print("U %d: taking break\n", id);
            sem_post(mutex);
            usleep((rand() % (TU + 1)) * 1000);         // Having a break for <0;TU> ms
            print("U %d: break finished\n", id);
        }else{
            sem_post(mutex);
        }

        
    }
    print("U %d: going home\n", id);
}

bool check_if_int(char* number){
    for(int i = 0; number[i] != '\0'; i++){
        if(!(number[i] >= '0' && number[i] <= '9')){
            return false;
        }
    }
    return true;
}

// Function for getting the command line arguments and checking if they are valid
bool get_args(int argc, char** argv){
    if(argc != 6){
        fprintf(stderr, "Error: Input must be in this format: NZ NU TZ TU F\n");
        return false;
    }

    for(int i = 1; i < 6; i++){
        if(!check_if_int(argv[i])){
            fprintf(stderr, "Error: Each input argument must be an int\n");
            return false;
        }
    }

    NZ = atoi(argv[1]);
    NU = atoi(argv[2]);
    TZ = atoi(argv[3]);
    TU = atoi(argv[4]);
    F = atoi(argv[5]);

    if(NU == 0){
        fprintf(stderr, "Workers count can't be 0\n");
        return false;
    }
    if(TZ < 0 || TZ > 10000 ){
        fprintf(stderr, "Error: Maximal customer waiting time must be in range [0;10000]\n");
        return false;
    }
    if(TU < 0 || TU > 100 ){
        fprintf(stderr, "Error: Maximal worker break time must be in range [0;100]\n");
        return false;
    }

    if(F < 0 || F > 10000 ){
        fprintf(stderr, "Error: Maximal closing time must be in range [0;10000]\n");
        return false;
    }

    return true;

}


int main(int argc, char** argv){
    if(!get_args(argc, argv)){
        return EXIT_FAILURE;
    }
    
    initialize();

    // Customer process generator
    for(int i = 1; i <= NZ; i++){
        int id = fork();
        if(id == 0){
            customer_process(i);
            fclose(file);
            exit(EXIT_SUCCESS);
        }else if(id == -1){
            perror("fork");
            deinitialize();
            return EXIT_FAILURE;
        }
    }

    // Worker process generator
    for(int i = 1; i <= NU; i++){
        int id = fork();
        if(id == 0){
            worker_process(i);
            fclose(file);
            exit(EXIT_SUCCESS);
        }else if(id == -1){
            perror("fork");
            deinitialize();
            return EXIT_FAILURE;
        }
    }

    usleep((rand() % (F / 2 + 1) + (F / 2)) * 1000);  // Waiting for <F/2;F> ms to close the office
    sem_wait(mutex);
    *closed = true;
    print("closing\n");
    sem_post(mutex);
    
    while(wait(NULL) > 0);                            // Waiting for child processes to be ended
    
    deinitialize();
    return EXIT_SUCCESS;
}
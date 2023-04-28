#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

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

int* msg_id = NULL;
int* customer_in_queue = NULL;
int** customer_queue_count = NULL;
int* workers_ready = NULL;
bool* closed = NULL;
int customer_count = 0;
int worker_count = 0;
int max_waiting_time = 0;
int max_break_time = 0;
int max_closing_time = 0;

FILE* file = NULL;

sem_t** customer_semaphores;
sem_t* worker_semaphore;
sem_t* mutex;
sem_t* print_semaphore;

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

void semaphores_init(){
    if((customer_semaphores = mmap(NULL, sizeof(sem_t*), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED ||
    (worker_semaphore = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED ||
    (mutex = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED ||
    (print_semaphore = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if((customer_semaphores[i] = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED){
            perror("mmap");
            exit(EXIT_FAILURE);
        }
    }
    
    if((msg_id = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (customer_in_queue = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (workers_ready = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (customer_queue_count = mmap(NULL, sizeof(int*), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED ||
    (closed = mmap(NULL, sizeof(bool), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, 0, 0)) == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if((customer_queue_count[i] = mmap(NULL, sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0 )) == MAP_FAILED){
            perror("mmap");
            exit(EXIT_FAILURE);
        }
        *customer_queue_count[i] = 0;
    }

    *msg_id = 1;
    *customer_in_queue = 0;
    *workers_ready = 0;
    *closed = false;
    file = fopen("proj2.out", "w");

    if(sem_init(worker_semaphore, 1, 0) == -1 ||
    sem_init(mutex, 1, 1) == -1 ||
    sem_init(print_semaphore, 1, 1) == -1){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; i++){
        if(sem_init(customer_semaphores[i], 1, 0) == -1){
            perror("sem_init");
            exit(EXIT_FAILURE);
        }
    }
}

void semaphores_free(){
    fclose(file);
    munmap(msg_id, sizeof(int));
    munmap(customer_in_queue, sizeof(int));
    munmap(workers_ready, sizeof(int));
    munmap(closed, sizeof(bool));

    for(int i = 0; i < 3; i++){
        sem_destroy(customer_semaphores[i]);
        munmap(customer_queue_count[i], sizeof(int));
    }
    munmap(customer_queue_count, sizeof(int*));
    //munmap(customer_semaphores, sizeof(sem_t*));
    sem_destroy(worker_semaphore);
    sem_destroy(mutex);
    sem_destroy(print_semaphore);
}

void customer_process(int id){
    srand(time(NULL) * getpid());
    print("Z %d: started\n", id);
    usleep((rand() % (max_waiting_time + 1)) * 1000);
    int tmp_service_id = 0;
    sem_wait(mutex);
    if(*closed){
        print("Z %d: going home\n", id);
        sem_post(mutex);
        return;
    }else{
        tmp_service_id = rand() % 3;
        print("Z %d: entering office for a service %d\n", id, tmp_service_id + 1);
        (*customer_in_queue)++;
        (*customer_queue_count)[tmp_service_id]++;
        sem_post(mutex);
    }

    sem_wait(customer_semaphores[tmp_service_id]);
    
    // sem_wait(mutex);
    // (*customer_queue_count)[tmp_service_id]--;
    // (*customer_in_queue)--;
    // sem_post(mutex);

    sem_post(worker_semaphore);
    
    print("Z %d: called by office worker\n", id); 
    usleep((rand() % 10) * 1000);    
    print("Z %d: going home\n", id);

}

// bool worker_exit(){
//     sem_wait(mutex);
//     int tmp_customer_in_queue = *customer_in_queue;
//     bool closed_tmp = *closed;
//     sem_post(mutex);
//     return tmp_customer_in_queue != 0 || !closed_tmp;
// }

void worker_process(int id){
    srand(time(NULL) * getpid());
    print("U %d: started\n", id);
    while(true){
        sem_wait(mutex);
        if(*workers_ready + 1 <= *customer_in_queue){
            (*workers_ready)++;
            int chosen_queue;
            do{
                chosen_queue = rand() % 3;
            }while((*customer_queue_count)[chosen_queue] <= 0);
            sem_post(customer_semaphores[chosen_queue]);
            //print("%d customers in queue\n", (*customer_queue_count)[chosen_queue]);
            (*customer_queue_count)[chosen_queue]--;
            (*customer_in_queue)--;
            (*workers_ready)--;
            sem_post(mutex);

            // sem_wait(mutex);
            // (*customer_queue_count)[chosen_queue]--;
            // (*customer_in_queue)--;
            // (*workers_ready)--;
            // sem_post(mutex);


            sem_wait(worker_semaphore);
            sem_wait(mutex);
            print("U %d: serving a service of type %d\n", id, chosen_queue + 1);
            sem_post(mutex);

            usleep((rand() % 10) * 1000);
            print("U %d: service finished\n", id);
        }else{
            sem_post(mutex);
        }

        sem_wait(mutex);
        if(*customer_in_queue == 0)
        {
            if(*closed){
                sem_post(mutex);
                break;
            }
            print("U %d: taking break\n", id);
            sem_post(mutex);
            usleep((rand() % (max_break_time + 1)) * 1000);
            print("U %d: break finished\n", id);
            // continue;
        }
        else{
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

bool get_args(int argc, char** argv){
    if(argc != 6){
        fprintf(stderr, "Error: Input must be in this format: NZ NU TZ TU F\n");
        return false;
    }

    for(int i = 1; i < 6; i++){
        if(!check_if_int(argv[i])){
            fprintf(stderr, "Error: Input must be in this format: NZ NU TZ TU F\n");
            return false;
        }
    }


    customer_count = atoi(argv[1]);
    worker_count = atoi(argv[2]);
    max_waiting_time = atoi(argv[3]);
    max_break_time = atoi(argv[4]);
    max_closing_time = atoi(argv[5]);
    if(worker_count == 0){
        fprintf(stderr, "Workers count can't be 0\n");
        return false;
    }
    if(max_waiting_time < 0 || max_waiting_time > 10000 ){
        fprintf(stderr, "Error: Maximal customer waiting time must be in range [0;10000]\n");
        return false;
    }
    if(max_break_time < 0 || max_break_time > 100 ){
        fprintf(stderr, "Error: Maximal worker break time must be in range [0;100]\n");
        return false;
    }

    if(max_closing_time < 0 || max_closing_time > 10000 ){
        fprintf(stderr, "Error: Maximal closing time must be in range [0;10000]\n");
        return false;
    }

    return true;

}


int main(int argc, char** argv){
    if(!get_args(argc, argv)){
        return EXIT_FAILURE;
    }
    semaphores_init();


    for(int i = 1; i <= worker_count; i++){
        int id = fork();
        if(id == 0){
            worker_process(i);
            fclose(file);
            exit(0);
        }
    }


    for(int i = 1; i <= customer_count; i++){
        int id = fork();
        if(id == 0){
            customer_process(i);
            fclose(file);
            exit(0);
        }
    }

    usleep((rand() % (max_closing_time / 2 + 1) + (max_closing_time / 2)) * 1000);
    sem_wait(mutex);
    *closed = true;
    sem_post(mutex);
    print("closing\n");
    while(wait(NULL) > 0);
    semaphores_free();

    return 0;
}
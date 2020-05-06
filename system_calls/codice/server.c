/// @file server.c
/// @brief Contiene l'implementazione del SERVER.

#define _DEFAULT_SOURCE

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>

#define D1 0
#define D2 1
#define D3 2
#define D4 3
#define D5 4


/*Declaration of methods*/
void sigTermHandler(int sig);

/*Global variables*/
int shmidBoard;

struct Board {
    int matrix[BOARD_ROWS][BOARD_COLS];
} * board;

int shmidAckList;

Acknowledgment * ackList;

/*Main*/
int main(int argc, char * argv[]) {
    
    if(argc != 3){
        printf("Usage: %s msg_queue_key positions_file", argv[0]);
        exit(1);
    }

    //Blocking all the signals but SIGTERM
    //and setting a new signal handler
    sigset_t newSet;

    if(sigfillset(&newSet) == -1)
        errExit("sigfillset failed");
    
    if(sigdelset(&newSet, SIGTERM) == -1)
        errExit("sigdelset failed");
    
    if(sigprocmask(SIG_SETMASK, &newSet, NULL) == -1)
        errExit("sigprocmask failed");
    
    if(signal(SIGTERM, sigTermHandler) == SIG_ERR)
        errExit("signal failed");

    //Checking if the file exists and opening it
    int fd = open(argv[2], O_RDONLY);
    if(fd == -1)
        errExit("open failed");
    

    //Managing shared memories
    //Board
    size_t memSize = sizeof(struct Board);

    shmidBoard = alloc_shared_memory(IPC_PRIVATE, memSize);

    board = (struct Board *)get_shared_memory(shmidBoard, 0);

    //Acknoledge List
    memSize = sizeof(Acknowledgment) * ACK_LIST_SIZE;

    shmidAckList = alloc_shared_memory(IPC_PRIVATE, memSize);

    ackList = (Acknowledgment *)get_shared_memory(shmidAckList, 0);

    /*...
      ...*/    
}

//non completa
void sigTermHandler(int sig){
    free_shared_memory(board);
    remove_shared_memory(shmidBoard);
}
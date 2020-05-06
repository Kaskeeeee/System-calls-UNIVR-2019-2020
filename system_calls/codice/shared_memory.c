/// @file shared_memory.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#include "err_exit.h"
#include "shared_memory.h"

int alloc_shared_memory(key_t shmKey, size_t size){
    //getting shared memory
    int shmid = shmget(shmKey, size, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(shmid == -1)
        errExit("shmget failed");
    return shmid;
}


void *get_shared_memory(int shmid, int shmflg){
    //attaching shared memory
    void *ptr = shmat(shmid, NULL, shmflg);
    if(ptr == (void *)-1)
        errExit("shmat failed");
    return ptr;
}

void free_shared_memory(void *ptr){
    //detaching shared memory segments
    if(shmdt(ptr) == -1)
        errExit("shmdt failed");
}

void remove_shared_memory(int shmid){
    //removing shared memory
    if(shmctl(shmid, IPC_RMID, NULL) == -1)
        errExit("shmctl (IPC_RMID) failed");
}


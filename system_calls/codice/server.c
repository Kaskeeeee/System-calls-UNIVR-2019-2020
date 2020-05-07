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
#include <sys/sem.h>
#include <unistd.h>

#define ACK_LIST_SEM 0;


/*Declaration of methods*/
void sigTermHandlerServer(int sig);
void updatePosition(int pos_file_fd, int device_no);
void print_positions(int step);
void sigTermHandlerDevice(int sig);

/*Global variables*/
int shmidBoard;
struct Board {
    int matrix[BOARD_ROWS][BOARD_COLS];
} * board;

int shmidAckList;
Acknowledgment * ackList;
int board_semid;
int acklist_semid
int devices_positions[N_DEVICES][2];
pid_t children_pids[1 + N_DEVICES];

int serverFIFO;
int serverFIFO_extra;
char pathToMyFIFO[25];

/*Main*/
int main(int argc, char * argv[]) {
    
    if(argc != 3){
        printf("Usage: %s msg_queue_key positions_file", argv[0]);
        exit(1);
    }

    int msgKey = atoi(argv[1]);
    if (msgKey <= 0) {
        printf("The message queue key must be greater than zero!\n");
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
    
    if(signal(SIGTERM, sigTermHandlerServer) == SIG_ERR)
        errExit("signal failed");

    printf("<Server> Server ready!\n");

    //Checking if the file exists and opening it
    printf("<Server> Opening the file: %s...\n", argv[2]);
    
    int fd = open(argv[2], O_RDONLY);
    if(fd == -1)
        errExit("open failed");
    
    printf("<Server> Done!\n");

    //Managing shared memories
    //Board
    size_t memSize = sizeof(struct Board);
    shmidBoard = alloc_shared_memory(IPC_PRIVATE, memSize);
    board = (struct Board *)get_shared_memory(shmidBoard, 0);

    //Acknoledge List
    memSize = sizeof(Acknowledgment) * ACK_LIST_SIZE;
    shmidAckList = alloc_shared_memory(IPC_PRIVATE, memSize);
    ackList = (Acknowledgment *)get_shared_memory(shmidAckList, 0);

    //Setting semaphores
    //Board semaphore set
    board_semid = semget(IPC_PRIVATE, N_DEVICES, IPC_CREAT | S_IRUSR | S_IWUSR);

    unsigned short initialValues[] = {0, 0, 0, 0, 0};
    union semun arg;
    arg.array = initialValues;

    if(semctl(board_semid, 0, SETALL, arg) == -1)
        errExit("semctl failed");

    //Acknowledge list semaphore set
    acklist_semid = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    if(semctl(acklist_semid, 0, SETVAL, 0) == -1)
        errExit("semctl failed");
    
    //Generating children processes
    children_pids[0] = fork();

    if(children_pids[0] < 0)
        errExit("Could not fork");
    else if(children_pids[0] == 0){
        /*-----------------------------
                ACK_MANAGER CODE
         -----------------------------*/
    }

    for(int i = 0; i < N_DEVICES; i++){
        children_pids[i + 1] = fork();
        if(children_pids[i + 1] < 0)
            errExit("Could not fork");
        else if(children_pids[i + 1] == 0){
            /*--------------------------
                    DEVICES CODE
             --------------------------*/

            if(signal(SIGTERM, sigTermHandlerDevice) == SIG_ERR)
                errExit("signal failed");
            
            sprintf(pathToMyFIFO, "%s%d", baseDeviceFIFO, getpid());
            if(mkfifo(pathToMyFIFO, S_IRUSR | S_IWUSR) == -1)
                errExit("fifo not created");

            serverFIFO = open(pathToMyFIFO, O_RDONLY);
            serverFIFO_extra = open(pathToMyFIFO, O_WRONLY);

            int device_no = i;

            semOp(board_semid, device_no, -1);
            updatePosition(fd, device_no);
            if(device_no != N_DEVICES - 1)
                semOp(board_semid, device_no + 1, 1);

            //Message msgsList[10] = {NULL};

            while(1){
                
            }

        }
    }
    

    /*-----------------------------
             SERVER CODE
       ---------------------------*/ 
    int step = 0;
    while(1){
        semOp(board_semid, 0, 1);
        sleep(2);
        print_positions(step);
        step++;
    } 
}

//non completa
void sigTermHandlerServer(int sig){
    for(int i = 1; i <= N_DEVICES; i++)
        kill(children_pids[i], SIGTERM);
    free_shared_memory(board);
    remove_shared_memory(shmidBoard);
    exit(0);
}

//Signal handler for devices:
//every device closes its own fifo
void sigTermHandlerDevice(int sig){
    // Close the FIFO
    if (serverFIFO != 0 && close(serverFIFO) == -1)
        errExit("close failed");

    if (serverFIFO_extra != 0 && close(serverFIFO_extra) == -1)
        errExit("close failed");

    // Remove the FIFO
    if (unlink(pathToMyFIFO) != 0)
        errExit("unlink failed");
    
    _exit(0);
}

//the updatePosition method reads from file the next
//position of a device and updates it in the board matrix
//and in the devices_positions matrix
void updatePosition(int pos_file_fd, int device_no){
    char buffer[4];
    int bR = read(pos_file_fd, buffer, 3);
    
    if(bR != -1){
        buffer[bR] = '\0';
        lseek(pos_file_fd, 1, SEEK_CUR);
    }
    else errExit("read failed");

    int x = (int)(buffer[0] - '0');
    int y = (int)(buffer[2] - '0');

    if(board->matrix[x][y] != 0){
        printf("Error, cell is not free!\n")
        kill(getppid(), SIGTERM);
    }
    
    board->matrix[x][y] = getpid();
    devices_positions[device_no][0] = x;
    devices_positions[device_no][1] = y;
}

//the print_positions method is used to 
//print informations about devices' positions
void print_positions(int step){
    printf("# Step %d: device positions #############", step)
    for(int i = 0; i < N_DEVICES; i++)
        printf("%d  %d  %d  msgs: %s", children_pids[i+1], devices_positions[i][0], devices_positions[i][1], /*lista*/);
    printf("#########################################");
}
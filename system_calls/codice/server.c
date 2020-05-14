/// @file server.c
/// @brief Contiene l'implementazione del SERVER.

#define _DEFAULT_SOURCE

#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include <signal.h>
#include <sys/wait.h>


//#define ACK_LIST_SEM 0;


/*Declaration of methods*/
void sigTermHandlerServer(int sig);
void sigTermHandlerAckManager(int sig);
void sigTermHandlerDevice(int sig);


/*Global variables*/
int shmidBoard;                         //shared memory board id
struct Board * board;                   //board pointer
int msqid;                              //message queue id
int shmidAckList;                       //acklist id
Acknowledgment * ackList;               //acklist pointer
int sem_idx_board;                      //semaphore board id
int sem_idx_ack;                        //semaphore acklist id
int sem_idx_access;                     //semaphore access id
pid_t children_pids[1 + N_DEVICES];     //array to keep children pids

int serverFIFO;                         //serverFIFO id
char pathToMyFIFO[25];                  //pathname to fifo

/*Main*/
int main(int argc, char * argv[]) {
    
    if(argc != 3){
        printf("Usage: %s msg_queue_key positions_file\n", argv[0]);
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

    printf("<Server> Server ready! [PID: %d]\n", getpid());

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
    sem_idx_board = semget(IPC_PRIVATE, N_DEVICES, IPC_CREAT | S_IRUSR | S_IWUSR);

    unsigned short initialValues[] = {0, 0, 0, 0, 0};
    union semun arg;
    arg.array = initialValues;

    if(semctl(sem_idx_board, 0, SETALL, arg) == -1)
        errExit("semctl failed");

    //Acknowledge list semaphore set
    sem_idx_ack = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    arg.val = 1;
    if(semctl(sem_idx_ack, 0, SETVAL, arg) == -1)
        errExit("semctl failed");
    
    //Board access semaphore
    sem_idx_access = semget(IPC_PRIVATE, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    arg.val = N_DEVICES;
    if(semctl(sem_idx_access, 0, SETVAL, arg) == -1)
        errExit("semctl failed");

    //Generating children processes
    children_pids[0] = fork();

    if(children_pids[0] < 0)
        errExit("Could not fork");
    else if(children_pids[0] == 0){
        /*-----------------------------
                ACK_MANAGER CODE
         -----------------------------*/
        
        //assign a specific sigHandler to SIGTERM
        if(signal(SIGTERM, sigTermHandlerAckManager) == SIG_ERR)
            errExit("signal failed");

        //Creating a message queue
        msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);
        if(msqid == -1)
            errExit("msgget failed");
        
        //the list keeps divided the different
        //message ids
        
        while(1){
            semOp(sem_idx_ack, 0, -1);
            
            check_list(ackList, msqid);
            
            semOp(sem_idx_ack, 0, 1);
            sleep(5);
        }

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

            //Opening FIFO in non blocking mode
            serverFIFO = open(pathToMyFIFO, O_RDONLY | O_NONBLOCK);
            if(serverFIFO == -1)
                errExit("fifo open failed");

            const int device_no = i;
            int prevX = -1, prevY = -1;

            semOp(sem_idx_board, device_no, -1);

            updatePosition(fd, board, &prevX, &prevY);
            if(device_no != N_DEVICES - 1)
                semOp(sem_idx_board, device_no + 1, 1);

            //when all devices join the board
            //this semaphore will unlock the server
            //so it can print positions
            semOp(sem_idx_access, 0, -1);

            //List of device messages
            Message msgList[MSG_ARRAY_SIZE] = {0};

            //core of the device
            while(1){
                semOp(sem_idx_board, device_no, -1);

                //Send the messages 
                semOp(sem_idx_ack, 0, -1);
                send_messages(ackList, board, prevX, prevY, msgList);
                semOp(sem_idx_ack, 0, 1);

                //order the list of messages
                //pushing to the tail of the array
                //the "non-messages" (message_id = 0)
                insertion_sort_msg(msgList);

                //Get the messages and update acknowledgement list
                semOp(sem_idx_ack, 0, -1);
                receive_update(msgList, ackList, serverFIFO);
                semOp(sem_idx_ack, 0, 1);

                //update position
                updatePosition(fd, board, &prevX, &prevY);
                if(device_no < N_DEVICES - 1)
                    semOp(sem_idx_board, device_no + 1, 1);

                //unlock the server
                semOp(sem_idx_access, 0, -1);
            }

        }
    }
    

    /*-----------------------------
             SERVER CODE
     -----------------------------*/
 
    int step = 0;
    while(1){
        //Unlocks the first device
        semOp(sem_idx_board, 0, 1);
        
        //Waits for all the devices to updatePosition()
        //and prints the positions
        semOp(sem_idx_access, 0, 0);
        semOp(sem_idx_ack, 0, -1);

        print_positions(step, board, children_pids, ackList);
        
        semOp(sem_idx_ack, 0, 1);

        //Set again the semaphore to N_DEVICES
        semOp(sem_idx_access, 0, N_DEVICES);
        sleep(2);

        step++;
    } 
}

//Signal handler that closes all the IPCs
//created by the server
void sigTermHandlerServer(int sig){
    for(int i = 0; i <= N_DEVICES; i++)
        kill(children_pids[i], SIGTERM);
    
    //wait for all the children to exit
    while(wait(NULL) != -1);
    if(errno != ECHILD)
        errExit("Unexpected error");

    //remove semaphores
    if(semctl(sem_idx_board, 0, IPC_RMID, NULL) == -1 ||
        semctl(sem_idx_ack, 0, IPC_RMID, NULL) == -1 ||
        semctl(sem_idx_access, 0, IPC_RMID, NULL) == -1)
        errExit("could not remove semaphores");

    //remove board
    free_shared_memory(board);
    remove_shared_memory(shmidBoard);

    //remove acklist
    free_shared_memory(ackList);
    remove_shared_memory(shmidAckList);

    exit(0);
}

//Signal handler for ack manager:
//it closes the message queue befor exiting
void sigTermHandlerAckManager(int sig){
    if(msgctl(msqid, IPC_RMID, NULL) == -1)
        errExit("msgctl IPC_RMID failed");
    exit(0);
}

//Signal handler for devices:
//every device closes its own fifo
void sigTermHandlerDevice(int sig){
    // Close the FIFO
    if (serverFIFO != 0 && close(serverFIFO) == -1)
        errExit("close failed");

    // Remove the FIFO
    if (unlink(pathToMyFIFO) != 0)
        errExit("unlink failed");
    
    exit(0);
}


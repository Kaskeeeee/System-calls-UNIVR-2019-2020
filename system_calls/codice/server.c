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
#include <sys/msg.h>
#include <unistd.h>

//#define ACK_LIST_SEM 0;


/*Declaration of methods*/
void sigTermHandlerServer(int sig);
void updatePosition(int pos_file_fd, int device_no);
void print_positions(int step);
void sigTermHandlerDevice(int sig);
void initialize(Message * dev_msg_list, int size);

/*Global variables*/
int shmidBoard;                         //shared memory board id
struct Board * board;                   //board pointer

int shmidAckList;                       //acklist id
Acknowledgment * ackList;               //acklist pointer
int sem_idx_board;                      //semaphore board id
int sem_idx_ack;                        //semaphore acklist id
int sem_idx_access;
pid_t children_pids[1 + N_DEVICES];     //array used to keep all the pids of generated processes
int prevX = -1, prevY = -1;             //variables to keep track of last position in order
                                        //to clear it when the devices moves away

int serverFIFO;                         //serverFIFO id
char pathToMyFIFO[25];                  //pathname to fifo

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
    arg.val = 0;
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
        
        //Creating a message queue
        int msqid = msgget(msgKey, IPC_CREAT | S_IRUSR | S_IWUSR);
        if(msqid == -1)
            errExit("msgget failed");
        
        //the list keeps divided the different
        //message ids
        
        while(1){
            semOp(sem_idx_ack, 0, -1);
            //for loop to check if there are 5 acks of a message id
            //and to free memory
            for(int i = 0; i < ACK_LIST_SIZE; i++){
                int count = 1;
                //save the message id
                int m_id = ackList[i].message_id;
                //check if it's a real message id
                if(m_id != 0){
                    //check if there are N_DEVICES (5) acknowledgements
                    for(int j = i + 1; j < i + N_DEVICES; j++){
                        if(ackList[j].message_id == m_id)
                            count++;
                    }
                    if(count == N_DEVICES){
                        //create the message for the message queue;
                        struct ackMessage message_to_client;
                        //mtype will be the pid of the client since
                        //ackList[i] identify the first ack that has
                        //client's pid in the field pid_sender
                        message_to_client.mtype = ackList[i].pid_sender;
                        int j;
                        for(j = i; j < i + N_DEVICES; j++){
                            message_to_client.acks[j] = ackList[j];
                            //mark the position as free
                            ackList[j].message_id = 0;
                        }
                        //tell the last device to delete last message
                 //TODO       kill(ackList[j-1].pid_receiver, SIGUSR1);
                        //send message
                        if(msgsnd(msqid, &message_to_client, sizeof(struct ackMessage), 0) == -1)
                            errExit("msgsnd failed");
                    }
                }
                
            }
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

            int device_no = i;

            semOp(sem_idx_board, device_no, -1);
            updatePosition(fd, device_no);
            if(device_no != N_DEVICES - 1)
                semOp(sem_idx_board, device_no + 1, 1);

            //when all devices join the board
            //this semaphore will unlock the server
            //so it can print positions
            semOp(sem_idx_access, 0, -1);

            //List of device messages
            Message msgList[20];
            //init the list
            initialize(msgList, 20);
            while(1){
                //Invio messaggi
                send_messages(board, prevX, prevY, msgList, 20);

                //Get the messages and update acknowledgement list
                receive_update(msgList, 20, ackList, sem_idx_ack, serverFIFO);
            }

        }
    }
    

    /*-----------------------------
             SERVER CODE
       ---------------------------*/
        
    int step = 0;
    while(1){
        //Unlocks the first device
        semOp(sem_idx_board, 0, 1);
        
        //Waits for all the devices to updatePosition()
        //and prints the positions
        semOp(sem_idx_access, 0, 0);
        print_positions(step);

        //Set again the semaphore to N_DEVICES
        semOp(sem_idx_access, 0, N_DEVICES);

        sleep(2);
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

    // Remove the FIFO
    if (unlink(pathToMyFIFO) != 0)
        errExit("unlink failed");
    
    _exit(0);
}

//the updatePosition method reads from file the next
//position of a device and updates it in the board matrix
//and in the devices_positions matrix
void updatePosition(int pos_file_fd, int device_no){
    //read the first 3 characters of the file
    char buffer[4];
    int bR = read(pos_file_fd, buffer, 3);
    
    //buffer now should contain a string
    //with the format: "x,y"

    if(bR != -1){
        buffer[bR] = '\0';
        //lseek skips the next '|' or '\n'
        lseek(pos_file_fd, 1, SEEK_CUR);
    }
    else errExit("read failed");

    //if you haven't reached EOF
    //then update position
    if(bR != 0){
        int x = (int)(buffer[0] - '0');
        int y = (int)(buffer[2] - '0');

        if(board->matrix[x][y] != 0){
            printf("Error, cell is not free!\n");
            kill(getppid(), SIGTERM);
        }

        if(prevX != -1 && prevY != -1)
            board->matrix[prevX][prevY] = 0;
        board->matrix[x][y] = getpid();
        prevX = x;
        prevY = y;
    }
}

//the print_positions method is used to 
//print informations about devices' positions
void print_positions(int step){
    int x, y;
    printf("# Step %d: device positions #############", step);
    for(int i = 0; i < N_DEVICES; i++){
        pid_t devpid = children_pids[i+1];
        getPosition(devpid, board, &x, &y);
        printf("%d  %d  %d  msgs: ", devpid, x, y);
        print_device_msgs(devpid);
        printf("\n");
    }
    printf("#########################################");
}

void initialize(Message * dev_msg_list, int size){
    for(int i = 0; i < size; i++)
        dev_msg_list[i].message_id = -1;
}

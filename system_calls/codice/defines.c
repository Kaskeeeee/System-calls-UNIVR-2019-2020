/// @file defines.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche del progetto.

#include "defines.h"

char * msg_id_history_file = "/tmp/msg_id_history_file";

/*------------------------------------------------------------
                
                SERVER METHODS

------------------------------------------------------------*/

void print_positions(int step, struct Board * board, int * children_pids, Acknowledgment * ackList){
    int x, y;
    pid_t devpid;
    printf("\n# Step %2d: device positions ##############\n", step);
    for(int i = 1; i <= N_DEVICES; i++){
        devpid = children_pids[i];
        getPosition(devpid, board, &x, &y);
        printf("DEVICE[%d]: %d  (%d, %d)  msgs: ", i, devpid, x, y);
        print_device_msgs(devpid, ackList);
        printf("\n");
    }
    printf("######################################### (Server: %d)\n", getpid());
}

void getPosition(pid_t pid, struct Board * board, int * x, int * y){
    for(int i = 0; i < BOARD_ROWS; i++)
        for(int j = 0; j < BOARD_COLS; j++)
            if(board->matrix[i][j] == pid){
                *x = i;
                *y = j;
                return;
            }
}

void print_device_msgs(pid_t pid, Acknowledgment * ackList){
    for(int i = 0; i < ACK_LIST_SIZE - 1; i++){
        //if you found the pid you were looking for
        //then check if the next position is free
        //if so that means the device has not yet sent the msg
        if(ackList[i].pid_receiver == pid && ackList[i+1].message_id == 0)
            printf("[ %d ]", ackList[i].message_id);
    }
}


/*------------------------------------------------------------
                
                DEVICES METHODS

------------------------------------------------------------*/

void receive_update(Message * msgList, Acknowledgment * ackList, int serverFIFO){
    //find the first free position of the array
    int i;
    for(i = 0; i < MSG_ARRAY_SIZE && msgList[i].message_id != 0; i++);
    //if there is a free position then read from fifo
    if(i != MSG_ARRAY_SIZE){
        //while there is something to read and the last position
        //of the array is not reached, read from fifo
        while(read(serverFIFO, &msgList[i], sizeof(Message)) > 0 && i < MSG_ARRAY_SIZE){
            int index_first_free_area = -1;
            //Ack
            Acknowledgment ack = {msgList[i].pid_sender, msgList[i].pid_receiver,
                                        msgList[i].message_id, time(NULL)};
            //at the end of this for loop i either found:
            //  - the index of the first free area of the list if this is the first ack with that message_id
            //  - the index of the first occurence of message_id in the list
            //  - the list is full
            int j;
            for(j = 0; j < ACK_LIST_SIZE && ackList[j].message_id != msgList[i].message_id; j += N_DEVICES){
                if(index_first_free_area == -1 && ackList[j].message_id == 0)
                    index_first_free_area = j;
            }
            
            if(j != ACK_LIST_SIZE){
                //save index of first occurence
                int first_occurence = j;

                //find the correct index 
                //to store the message
                while(ackList[j].message_id == msgList[i].message_id)
                    j++;
                ackList[j] = ack;

                //if there are N_DEVICES acknowledgements
                //then delete the message from the array
                if((j - first_occurence + 1) == N_DEVICES)
                    msgList[i].message_id = 0;
            }            
            else if(index_first_free_area != -1)
                ackList[index_first_free_area] = ack;
            //if the list is full then the message is rejected
            else{
                printf("Error, the list is full!\n");
                kill(getppid(), SIGTERM);
            }

            i++;
        }
    }
}

void send_messages(Acknowledgment * ackList, struct Board * board, int x, int y, Message * msgList){
    //search in the matrix for a device
    //to whom send the messages
    for(int i = 0; i < BOARD_ROWS; i++){
        for(int j = 0; j < BOARD_COLS; j++){
            if(board->matrix[i][j] != 0 && (i != x || j != y)){
                //save the found pid as pid_receiver
                pid_t pid_receiver = board->matrix[i][j];

                //open its fifo
                char path2deviceFIFO[25];
                sprintf(path2deviceFIFO, "%s%d", baseDeviceFIFO, pid_receiver);
                int fd = open(path2deviceFIFO, O_WRONLY);
                
                //should send at least 1 message
                int nr_messages = 0;

                //for every message, check if
                //pid_sender is in range and it
                //hasn't already received that message
                for(int k = 0; k < MSG_ARRAY_SIZE && msgList[k].message_id != 0; k++){
                    if(in_range(i,j,x,y,msgList[k].max_distance) &&
                        !received_yet(ackList, msgList[k].message_id, pid_receiver)){
                        
                        
                        //now I am the sender
                        //and pid_receiver is the receiver
                        msgList[k].pid_sender = getpid();
                        msgList[k].pid_receiver = pid_receiver;

                        if(write(fd, &msgList[k], sizeof(Message)) == -1)
                            errExit("write failed");
                        
                        //mark message as sent    
                        msgList[k].message_id = 0;
                        nr_messages++;
                    }
                }
                if(close(fd) == -1)
                    errExit("close fifo failed");
                //if i sent at least one message
                //return because we can only send to 
                //one device at a time
                if(nr_messages)
                    return;
            }
        }
    }
}

int in_range(int i, int j, int x, int y, double max_dist){
    int diff_ix = i - x;
    int diff_jy = j - y;
    return (max_dist * max_dist) >= ((diff_ix * diff_ix) + (diff_jy * diff_jy));
}

int received_yet(Acknowledgment * ackList, int message_id, pid_t dest){
    for(int i = 0; i < ACK_LIST_SIZE; i++)
        if(ackList[i].message_id == message_id && ackList[i].pid_receiver == dest)
            return 1;
    return 0;
}

void insertion_sort_msg(Message * msgList){
    //implementation of insertion sort:
    //sorting on message id's
    for(int j = 1; j < MSG_ARRAY_SIZE; j++){
        Message pivot = msgList[j];
        int i = j - 1;
        while(i >= 0 && msgList[i].message_id < pivot.message_id){
            msgList[i+1] = msgList[i];
            i -= 1;
        }
        msgList[i+1] = pivot;
    }
}

void updatePosition(int pos_file_fd, struct Board * board, int * prevX, int * prevY){
    //read the first 3 characters of the file
    char buffer[4];
    int bR = read(pos_file_fd, buffer, 3);
    
    if(bR == 0){
        //restart
        lseek(pos_file_fd, 0, SEEK_SET);
        bR = read(pos_file_fd, buffer, 3);
    }

    //buffer now should contain a string
    //with the format: "x,y"
    if(bR != -1){
        buffer[bR] = '\0';
        //lseek skips the next '|' or '\n'
        lseek(pos_file_fd, 1, SEEK_CUR);
    }else errExit("read failed");
    

    //if you haven't reached EOF
    //then update position
    if(bR != 0){
        int x = (int)(buffer[0] - '0');
        int y = (int)(buffer[2] - '0');

        if(board->matrix[x][y] != 0 && board->matrix[x][y] != getpid()){
            printf("Error, cell is not free! I am [%d] and I collided with [%d]\n", getpid(), board->matrix[x][y]);
            kill(getppid(), SIGTERM);
        }

        if(*prevX != -1 && *prevY != -1)
            board->matrix[*prevX][*prevY] = 0;
        board->matrix[x][y] = getpid();
        *prevX = x;
        *prevY = y;
    }
}


/*------------------------------------------------------------
                
                ACK MANAGER METHODS

------------------------------------------------------------*/


void check_list(Acknowledgment * ackList, int msqid){
    //for loop to check if there are 5 acks of a message id
    //and to free memory
    for(int i = 0; i < ACK_LIST_SIZE; i += N_DEVICES){
        //save the message id
        int m_id = ackList[i].message_id;
        //check if it's a real message id
        if(m_id != 0){
            int count = 1;

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
                
                for(int j = i; j < i + N_DEVICES; j++){
                    message_to_client.acks[j-i] = ackList[j];
                    //mark the position as free
                    ackList[j].message_id = 0;
                    ackList[j].pid_receiver = -1;
                    ackList[j].pid_sender = -1;
                }
                //send message
                size_t mSize = sizeof(struct ackMessage) - sizeof(long);
                if(msgsnd(msqid, &message_to_client, mSize, 0) == -1)
                    errExit("msgsnd failed");
            }
        }         
    }
}




/*------------------------------------------------------------
                
                CLIENT METHODS

------------------------------------------------------------*/

int readInt(char * s){
    char *endptr = NULL;
    errno = 0;
    long int res = strtol(s, &endptr, 10);
    //the conversion has been done correctly if:
    //  errno has been unchanged
    //  endptr contains a value that is different from the original string
    if (errno != 0 || *endptr != '\n')
        errExit("<Client> Conversion to integer failed");
    return res;
}


double readDouble(char * s){
    char *endptr = NULL;
    errno = 0;
    double res = strtod(s, &endptr);

    if(errno != 0 || *endptr != '\n')
      errExit("<Client> Conversion to integer failed");

    return res;
}


void printToOutput(struct ackMessage ack_list, char * message_text){
    printf("Message %d: %s\nAcknoledgement List:\n", ack_list.acks[0].message_id, message_text);
    char buffer[20];
    for(int i = 0; i < N_DEVICES; i++){
        get_tstamp(ack_list.acks[i].timestamp, buffer, 20);
        printf("%d, %d, %s\n", ack_list.acks[i].pid_sender, ack_list.acks[i].pid_receiver, buffer);
    }
}


void get_tstamp(time_t timer, char * buffer, size_t buffer_size){
    struct tm * tm_info = localtime(&timer);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

int m_id_available(int history_fd, int chosen_id){
    //read file from beginning
    lseek(history_fd, 0, SEEK_SET);

    int current_id = 0;
    while(read(history_fd, &current_id, sizeof(int))){
        if(current_id == chosen_id){
            printf("<Client> Message id is already taken!\n");
            return 0;
        }
    }
    if(write(history_fd, &chosen_id, sizeof(int)) == -1)
        errExit("write failed");
    return 1;    
}

void free_message_id(int history_fd){
    int free_id = -1;
    lseek(history_fd, -sizeof(int), SEEK_CUR);
    if(write(history_fd, &free_id, sizeof(int)) == -1)
        errExit("write failed");
}
/// @file defines.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche del progetto.

#include "defines.h"

/*------------------------------------------------------------
                
                SERVER FUNCTIONS

------------------------------------------------------------*/

void getPosition(pid_t pid, struct Board * board, int * x, int * y){
    for(int i = 0; i < BOARD_ROWS; i++)
        for(int j = 0; j < BOARD_COLS; j++)
            if(board->matrix[i][j] == pid){
                *x = i;
                *y = j;
                return;
            }
}

void receive_update(Message * msgList, int size, Acknowledgment * ackList, int serverFIFO){
    //find the first free position of the array
    int i;
    for(i = 0; i < size && msgList[i].message_id != 0; i++);
    //if there is a free position then read from fifo
    if(i != size){
        //while there is something to read and the last position
        //of the array is not reached, read from fifo
        while(read(serverFIFO, &msgList[i], sizeof(Message)) > 0 && i < size){
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
                if(index_first_free_area == -1 && ackList[j].message_id == 0);
                    index_first_free_area = j;
            }

            if(j != ACK_LIST_SIZE){
                while(ackList[j].message_id == msgList[i].message_id)
                    j++;
                ackList[j] = ack;
            }            
            else if(index_first_free_area != -1)
                ackList[index_first_free_area] = ack;
            //if the list is full then the message is rejected
            else
                errExit("message not received");

            i++;
        }
    }
}

int in_range(int i, int j, int x, int y, double max_dist){
    int diff_ix = i - x;
    int diff_jy = j - y;
    if((max_dist * max_dist) > (diff_ix * diff_ix) + (diff_jy * diff_jy))
        return 1;
    return 0;
}

void send_messages(Acknowledgment * ackList, struct Board * board, int x, int y, Message * msgList, int size){
    //search in the matrix for a device
    //to whom send the messages
    for(int i = 0; i < BOARD_ROWS; i++){
        for(int j = 0; j < BOARD_COLS; j++){
            if(board->matrix[i][j] != 0 && i != x && j != y){
                char path2deviceFIFO[25];
                sprintf(path2deviceFIFO, "%s%d", baseDeviceFIFO, board->matrix[i][j]);
                int fd = open(path2deviceFIFO, O_WRONLY);
                for(int k = 0; k < size && msgList[k].message_id != 0; k++){
                    if(in_range(i,j,x,y,msgList[k].max_distance) &&
                        !received_yet(ackList, msgList[k].message_id, board->matrix[i][j])){
                        if(write(fd, &msgList[k], sizeof(Message)) == -1)
                            errExit("write failed");
                        msgList[k].message_id = 0;
                        close(fd);
                        //return because we can send messages to a single device
                        return;
                    }
                }
            }
        }
    }
}

int received_yet(Acknowledgment * ackList, int message_id, pid_t dest){
    for(int i = 0; i < ACK_LIST_SIZE; i += N_DEVICES)
        if(ackList[i].message_id == message_id){
            for(int j = i; j < i + N_DEVICES; j++)
                if(ackList[j].pid_receiver == dest)
                    return 1;
            return 0;
        }
    return 0;
}

void print_positions(int step, struct Board * board, int * children_pids, Acknowledgment * ackList){
    int x, y;
    printf("# Step %d: device positions #############", step);
    for(int i = 0; i < N_DEVICES; i++){
        pid_t devpid = children_pids[i+1];
        getPosition(devpid, board, &x, &y);
        printf("%d  %d  %d  msgs: ", devpid, x, y);
        print_device_msgs(devpid, ackList);
        printf("\n");
    }
    printf("#########################################");
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

void initialize(Message * dev_msg_list, int size){
    for(int i = 0; i < size; i++)
        dev_msg_list[i].message_id = -1;
}

void check_list(Acknowledgment * ackList, int msqid){
    //for loop to check if there are 5 acks of a message id
    //and to free memory
    for(int i = 0; i < ACK_LIST_SIZE; i += N_DEVICES){
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
                //TODO 
                //send message
                if(msgsnd(msqid, &message_to_client, sizeof(struct ackMessage), 0) == -1)
                    errExit("msgsnd failed");
            }
        }         
    }
}


/*------------------------------------------------------------
                
                CLIENT FUNCTIONS

------------------------------------------------------------*/

int readInt(char * s){
    char *endptr = NULL;
    errno = 0;
    long int res = strtol(s, &endptr, 10);
    //the conversion has been done correctly if:
    //  errno has been unchanged
    //  endptr contains a value that is different from the original string
    //  res is positive
    if (errno != 0 || *endptr != '\n' || res <= 0) {
        printf("Invalid input argument\n");
        exit(1);
    }
    return res;
}


double readDouble(char * s){
    char *endptr = NULL;
    errno = 0;
    double res = strtod(s, &endptr);

    if(errno != 0 || *endptr != '\n' || res <= 1.0){
        printf("Invalid input argument\n");
        exit(1);
    }
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
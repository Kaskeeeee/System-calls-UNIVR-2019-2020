/// @file defines.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche del progetto.

#include "defines.h"
#include "semaphore.h"
#include <sys/sem.h>

void getPosition(pid_t pid, struct Board * board, int * x, int * y){
    for(int i = 0; i < BOARD_ROWS; i++)
        for(int j = 0; j < BOARD_COLS; j++)
            if(board->matrix[i][j] == pid){
                *x = i;
                *y = j;
                return;
            }
}

void receive_update(Message * msgList, int size, Acknowledgment * ackList, int sem_idx_ack, int serverFIFO){
    //find the first free position of the array
    int i;
    for(i = 0; i < size && msgList[i].message_id != 0; i++);
    //if there is a free position then read from fifo
    if(i != size){
        //while there is something to read and the last position
        //of the array is not reached, read from fifo
        while(read(serverFIFO, &msgList[i], sizeof(Message)) > 0 && i < size){
            //get the lock on acknowledgement list
            semOp(sem_idx_ack, 0, -1);

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
            semOp(sem_idx_ack, 0, 1);
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

void send_messages(struct Board * board, int x, int y, Message * msgList, int size){
    for(int i = 0; i < BOARD_ROWS; i++){
        for(int j = 0; j < BOARD_COLS; j++){
            if(board->matrix[i][j] != 0 && i != x && j != y){
                char path2deviceFIFO[25];
                sprintf(path2deviceFIFO, "%s%d", baseDeviceFIFO, board->matrix[i][j]);
                int fd = open(path2deviceFIFO, O_WRONLY);
                for(int k = 0; k < size && msgList[k].message_id != 0; k++){
                    if(in_range(i,j,x,y,msgList[k].max_distance)){
                        if(write(fd, &msgList[k], sizeof(Message)) == -1)
                            errExit("write failed");
                        msgList[k].message_id = 0;
                    }
                }
                close(fd);
                //return because we can send messages to a single device
                return;
            }
        }
    }
}



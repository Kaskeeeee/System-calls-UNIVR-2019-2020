/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include "fifo.h"
#include <fcntl.h>
#define N_DEVICES 5
#define ACK_LIST_SIZE 100
#define BOARD_COLS 10
#define BOARD_ROWS 10

typedef struct {
    pid_t pid_sender;
    pid_t pid_receiver;
    int message_id;
    time_t timestamp;
} Acknowledgment;

typedef struct {
    pid_t pid_sender;
    pid_t pid_receiver;
    int message_id;
    char message[256];
    int max_distance;
} Message;

struct ackMessage {
    long mtype;
    Acknowledgment acks[N_DEVICES];
};

struct Board {
    int matrix[BOARD_ROWS][BOARD_COLS];
};

void getPosition(pid_t pid, struct Board * board, int * x, int * y);

//the receive_update method receive all the messages in the fifo
//and organize the acknowledgement list
void receive_update(Message * msgList, int size, Acknowledgment * ackList, int sem_idx_ack, int serverFIFO);

//the in_range method is a support function that checks if a (i,j) device
//is reachable from the (x,y) identified device.
int in_range(int i, int j, int x, int y, double max_dist);

//the send_messages method checks if there are devices in range to
//send all the messages stored in the array of the device;
void send_messages(struct Board * board, int x, int y, Message * msgList, int size);
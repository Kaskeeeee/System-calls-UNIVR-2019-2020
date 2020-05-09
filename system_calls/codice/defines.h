/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once
#include <time.h>
#include <stdlib.h>
#include <sys/types.h> 
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

void receive_update(Message * msgList, int size, Acknowledgment * ackList, int sem_idx_ack, int serverFIFO);
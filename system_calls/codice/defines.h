/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once
#include <time.h>
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
/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <string.h>
#include <stdio.h>
#include <sys/msg.h>
#include "err_exit.h"
#include "fifo.h"

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
void receive_update(Message * msgList, int size, Acknowledgment * ackList, int serverFIFO);

//the in_range method is a support function that checks if a (i,j) device
//is reachable from the (x,y) identified device.
int in_range(int i, int j, int x, int y, double max_dist);

//the send_messages method checks if there are devices in range to
//send all the messages stored in the array of the device;
void send_messages(Acknowledgment * ackList, struct Board * board, int x, int y, Message * msgList, int size);

//the print_positions method is used to 
//print informations about devices' positions
void print_positions(int step, struct Board * board, int * children_pids, Acknowledgment * ackList);

//initialize as empty the message list of a device
void initialize(Message * dev_msg_list, int size);

//the check_list method is used by the ack_manager to check
//if a set of acks is complete, if so it gets removed and a
//message is sent to the client who first sent the message
void check_list(Acknowledgment * ackList, int msqid);

//the received_yet method return 1 if a certain device (dest)
//has already received message with id message_id, 0 otherwise
int received_yet(Acknowledgment * ackList, int message_id, pid_t dest);

//print_device_msgs checks the ack list to print
//the messages that a device has not yet sent 
void print_device_msgs(pid_t pid, Acknowledgment * ackList);

//the readInt method converts and returns 
//the value of a string into an integer
int readInt(char * s);

//the readDouble method converts and returns 
//the value of a string into a double
double readDouble(char * s);

//the printToOutput method writes the acklist
//to the output file
void printToOutput(struct ackMessage ack_list, char * message_text);

//the get_tstamp method returns a
//string with format yy-mm-dd hh:mm:ss
//but make sure you pass a buffer big enough
void get_tstamp(time_t t, char * buffer, size_t buffer_size);
/// @file fifo.h
/// @brief Contiene la definizioni di variabili e
///         funzioni specifiche per la gestione delle FIFO.

//#pragma once

#include "defines.h"

char * baseDeviceFIFO;

//the send_to_dev_in_range method checks if there are devices
//in range in order to send a message. If any device is in range
//its FIFO is open to write the message.
void send_to_dev_in_range(int matrix[BOARD_ROWS][BOARD_COLS], double max_dist, int myX, int myY, Message msg);

//the in_range method is a support function that checks if a (i,j) device
//is reachable from the (x,y) identified device.
int in_range(int i, int j, int x, int y, double max_dist);
/// @file fifo.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione delle FIFO.

#include "err_exit.h"
#include "fifo.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

char * baseDeviceFIFO = "/tmp/dev_fifo.";

/*void send_to_dev_in_range(int matrix[BOARD_ROWS][BOARD_COLS], double max_dist, int myX, int myY, Message msg[10]){
    char pathToDev[25];
    msg.pid_sender = getpid();

    for(int i = 0; i < BOARD_ROWS; i++)
        for(int j = 0; j < BOARD_COLS; j++){
            if(matrix[i][j] != 0 && i != myX && j != myY)
                if(in_range(i, j, myX, myY, max_dist)){
                    sprintf(pathToDev, "%s%d", baseDeviceFIFO, matrix[i][j]);
                    msg.pid_receiver = matrix[i][j];
                    int fd = open(pathToDev, O_WRONLY);
                    if(write(fd, &msg, sizeof(msg)) != sizeof(msg));
                        errExit("write failed");
                    close(fd);
                    return;
                }
        }
}*/


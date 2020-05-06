/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include "fifo.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <fcntl.h>

#define OUT "output_file_"
/*Declaration of methods*/
int readInt(char * s);


/*Main*/
int main(int argc, char * argv[]) {
    //check if the key is passed as argument
    if(argc != 2){
        printf("Usage: %s msg_queue_key\n", argv[0]);
        exit(1);
    }

    key_t msqKey = atoi(argv[1]);

    Message msg;
    char buffer[20];

    //Input of necessary data
    printf("<Client> Hello, let's send a message!\n");
    msg.pid_sender = getpid();

    printf("<Client> Insert the receiver's pid: ");
    fgets(buffer, sizeof(buffer), stdin);
    msg.pid_receiver = readInt(buffer);

    printf("<Client> Insert the message id: ");
    fgets(buffer, sizeof(buffer), stdin);
    msg.message_id = readInt(buffer);

    printf("<Client> Insert your message: ");
    fgets(msg.message, sizeof(msg.message), stdin);

    printf("<Client> Insert the max distance: ");
    fgets(buffer, sizeof(buffer), stdin);
    msg.max_distance = readInt(buffer);

    //Obtaining FIFO pathname, opening it and sending the message
    char actualDeviceFIFO[25];
    sprintf(actualDeviceFIFO, "%s%d", baseDeviceFIFO, msg.pid_receiver);

    int fifo_fd = open(actualDeviceFIFO, O_WRONLY);
    if(fifo_fd == -1)
        errExit("open failed");
    
    if(write(fifo_fd, &msg, sizeof(msg)) != sizeof(msg))
        errExit("write failed");

    if(close(fifo_fd) == -1)
        errExit("close failed");
    
    if(unlink(actualDeviceFIFO) == -1)
        errExit("unlink failed");

    printf("<Client> Message sent successfully!\n");
    printf("<Client> Waiting for a response...\n");

    //Opening the messagge queue with the given key and
    //waiting a response from the server
    int msqid = msgget(msqKey, IPC_CREAT | S_IRUSR);

    struct ackMessage acks;
    size_t mSize = sizeof(struct ackMessage) - sizeof(long);

    if(msgrcv(msqid, &acks, mSize, getpid(), 0) == -1)
        errExit("msgrcv failed");
    
    //Creating and filling output file
    
    sprintf(buffer, "%s%d", OUT, msg.message_id);
    int output_fd = open(buffer, O_CREAT | O_EXCL | O_WRONLY, S_IRWXU | S_IRWXG);

    for(int i = 0; i < N_DEVICES; i++){
        if(write(output_fd, &(acks.acks[i]), sizeof(Acknowledgment)) != sizeof(Acknowledgment))
            errExit("write failed");
    }

    if(close(output_fd) == -1)
        errExit("close failed");

    printf("<Client> Acknowledge list printed in the %s file!\n", buffer);
}

/*Implementation of methods*/

//the readInt method converts and returns 
//the value of a string into an integer
int readInt(char * s){
    char *endptr = NULL;
    errno = 0;
    long int res = strtol(s, &endptr, 10);
    //the conversion has been done correctly if:
    //  errno has been unchanged
    //  endptr contains a value that is different from the original string
    //  res is a positive number
    if (errno != 0 || *endptr != '\n' || res <= 0) {
        printf("Invalid input argument\n");
        exit(1);
    }
    return res;
}
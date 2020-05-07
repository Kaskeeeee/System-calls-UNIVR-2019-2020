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
double readDouble(char * s);
void printToOutput(int output_fd, struct ackMessage acks, char * message_text);
char * get_tstamp(time_t t);

/*Main*/
int main(int argc, char * argv[]) {
    //check if the key is passed as argument
    if(argc != 2){
        printf("Usage: %s msg_queue_key\n", argv[0]);
        exit(1);
    }

    int msgKey = atoi(argv[1]);
    if (msgKey <= 0) {
        printf("The message queue key must be greater than zero!\n");
        exit(1);
    }

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
    msg.max_distance = readDouble(buffer);

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

    printf("<Client> Message sent successfully!\n");
    printf("<Client> Waiting for a response...\n");

    //Opening the messagge queue with the given key and
    //waiting a response from the server
    int msqid = msgget(msgKey, IPC_CREAT | S_IRUSR);

    struct ackMessage acks;
    size_t mSize = sizeof(struct ackMessage) - sizeof(long);

    if(msgrcv(msqid, &acks, mSize, getpid(), 0) == -1)
        errExit("msgrcv failed");
    
    //Creating and filling output file
    //Closing stdout in order to write on new opened file with printf
    close(STDOUT_FILENO);

    sprintf(buffer, "%s%d", OUT, msg.message_id);
    int output_fd = open(buffer, O_CREAT | O_EXCL | O_WRONLY, S_IRWXU | S_IRWXG);

    printToOutput(output_fd, acks, msg.message);

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
    //  res is positive
    if (errno != 0 || *endptr != '\n' || res <= 0) {
        printf("Invalid input argument\n");
        exit(1);
    }
    return res;
}

//the readDouble method converts and returns 
//the value of a string into a double
double readDouble(char * s){
    char *endptr = NULL;
    errno = 0;
    double res = strtod(s, &endptr);

    if(errno != 0 || *endptr != '\n' || res <= 1.0){
        printf("Invalid input argument\n");
        exit(1);
    }
}

//the printToOutput method writes the acklist
//to the output file
void printToOutput(int output_fd, struct ackMessage acks, char * message_text){
    printf("Message %d: %s\nAcknoledgement List:\n", acks.acks[0].message_id, message_text);
    for(int i = 0; i < N_DEVICES; i++){
        printf("%d, %d, %s\n", acks.acks[i].pid_sender, acks.acks[i].pid_receiver, get_tstamp(acks.acks[i].timestamp));
    }
}

//the get_tstamp method returns a
//string with format hh:mm:ss
char * get_tstamp(time_t timer){
    struct tm * tm = localtime(&timer);
    char * buffer[20];

    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year, tm->tm_mon, tm->tm_mday,\
         tm->tm_hour, tm->tm_min, tm->tm_sec);
    return buffer;
}
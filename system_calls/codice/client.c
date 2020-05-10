/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include <unistd.h>
#include <sys/stat.h>

#define OUT "output_file_"

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

    struct ackMessage ack_list;
    size_t mSize = sizeof(struct ackMessage) - sizeof(long);

    if(msgrcv(msqid, &ack_list, mSize, getpid(), 0) == -1)
        errExit("msgrcv failed");
    
    //Creating and filling output file
    //Closing stdout in order to write on new opened file with printf
    close(STDOUT_FILENO);

    sprintf(buffer, "%s%d", OUT, msg.message_id);
    int output_fd = open(buffer, O_CREAT | O_EXCL | O_WRONLY, S_IRWXU | S_IRWXG);
    if(output_fd == -1)
        errExit("open failed");

    printToOutput(ack_list, msg.message);

    if(close(output_fd) == -1)
        errExit("close failed");

    printf("<Client> Acknowledge list printed in the %s file!\n", buffer);
}



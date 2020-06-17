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
    char buffer[25];
    size_t len;
    char actualDeviceFIFO[25];
    
    //open history file to check if the message id is taken
    int history_fd = open(msg_id_history_file, O_RDWR);
    if(history_fd == -1){
        perror("<Client> Could not open history file!");
        printf("<Client> Server might be down, terminating session...\n");
        exit(1);
    }

    //Opening the messagge queue with the given key
    int msqid = msgget(msgKey, S_IRUSR);
    if(msqid == -1)
        errExit("<Client> Message queues does not exists");

    //Input of necessary data
    printf("<Client> Hello, let's send a message!\n");
    msg.pid_sender = getpid();

    printf("<Client> Insert the receiver's pid: ");
    fgets(buffer, sizeof(buffer), stdin);
    msg.pid_receiver = readInt(buffer);

    //Check already if pid_receiver is a valid pid for a device
    sprintf(actualDeviceFIFO, "%s%d", baseDeviceFIFO, msg.pid_receiver);
    int fifo_fd = open(actualDeviceFIFO, O_WRONLY);
    if(fifo_fd == -1){
        errExit("<Client> open failed");
    }

    do{
        printf("<Client> Insert the message id: ");
        fgets(buffer, sizeof(buffer), stdin);
        msg.message_id = readInt(buffer);

        //Check validity of message_id
        if(msg.message_id <= 0){
            printf("<Client> Invalid argument exception, message id must be greater than 0!\n");
            exit(1);
        }
    }while(!m_id_available(history_fd, msg.message_id));


    printf("<Client> Insert your message: ");
    fgets(msg.message, sizeof(msg.message), stdin);
    len = strlen(msg.message);
    msg.message[len - 1] = '\0';

    printf("<Client> Insert the max distance: ");
    fgets(buffer, sizeof(buffer), stdin);
    msg.max_distance = readDouble(buffer);

    //Check validity of max_distance
    if(msg.max_distance < 1.0){
        printf("<Client> Invalid argument exception, no device is reachable with that max distance!\n");
        free_message_id(history_fd);
        exit(1);
    }

    //Sending message to device FIFO  
    if(write(fifo_fd, &msg, sizeof(msg)) != sizeof(msg))
        errExit("write failed");

    if(close(fifo_fd) == -1)
        errExit("close failed");

    //Waiting response from server...
    printf("<Client> Message sent successfully!\n");
    printf("<Client> Waiting for a response...\n");

    struct ackMessage ack_list;
    size_t mSize = sizeof(struct ackMessage) - sizeof(long);

    if(msgrcv(msqid, &ack_list, mSize, getpid(), 0) == -1)
        errExit("msgrcv failed");
    
    //Creating and filling output file
    //Closing stdout in order to write on new opened file with printf
    int dup_out = dup(STDOUT_FILENO);
    close(STDOUT_FILENO);

    sprintf(buffer, "%s%d.txt", OUT, msg.message_id);
    int output_fd = open(buffer, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG);
    if(output_fd == -1)
        errExit("open failed");

    printToOutput(ack_list, msg.message);

    if(close(output_fd) == -1)
        errExit("close failed");

    char result[80];
    sprintf(result, "<Client> Acknowledge list printed in the %s file!\n", buffer);
    if(write(dup_out, result, strlen(result)) == -1)
        errExit("write failed");

    //set free the message id used
    free_message_id(history_fd);
    if(close(history_fd) == -1)
        errExit("close failed");
}



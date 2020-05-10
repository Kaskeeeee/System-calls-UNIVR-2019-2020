#define _DEFAULT_SOURCE
#include<stdio.h>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include <sys/wait.h>

int x = 10;

int main(){
    int fd = open("out.txt", O_RDONLY);
    char buffer[30];

    for(int i = 0; i < 3; i++){
        pid_t pid = fork();
        if(pid == 0){
            x--;
            int bR = read(fd, buffer, 7);
            //buffer[bR] = '\0';
            printf("%s\n", buffer);
            printf("%d\n", x);
            close(fd);
            exit(0);
        }
    }
    sleep(2);
    int bR = read(fd, buffer, 10);
    buffer[bR] = '\0';
    printf("%s\n", buffer);
    printf("%d\n", x);
    close(fd);
    while(wait(NULL) != -1);
}
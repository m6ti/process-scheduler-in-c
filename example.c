#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#define NUMBER_OF_PROCESSES 2
int main() {
    pid_t pid = 0;
    int i;
    for(i = 0; i < NUMBER_OF_PROCESSES; i++)
    {
        pid = fork();
        if(pid < 0) {
            printf("Could not create process\n");
            exit(1);
        } else if(pid == 0) {
            printf("Hello from the child process\n");
        }
    }
}
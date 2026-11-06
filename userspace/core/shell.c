#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 100

int main() {
    char input[MAX_COMMAND_LENGTH];
    
    while (1) {
        printf("Shell>\n");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
            exit(1);
        }
        
        // Remove the newline character from the input
        for (int i = 0; input[i] != '\0'; i++) {
            if (input[i] == '\n') {
                input[i] = '\0';
                break;
            }
        }
        
        if (strcmp(input, "exit") == 0) {
            printf("Exiting the shell...\n");
            break;
        }
        
        // Fork a child process
        pid_t child_pid = fork();
        
        if (child_pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (child_pid == 0) {
            // This is the child process
            char* args[MAX_COMMAND_LENGTH];
            char* token = strtok(input, " ");
            int i = 0;
            
            while (token != NULL) {
                args[i] = token;
                token = strtok(NULL, " ");
                i++;
            }
            
            args[i] = NULL;
            printf("Executing %s\n",args[0]); 
            // Execute the command using execvp
            if (execve(args[0], args,NULL) == -1) {
                perror("Execution error");
                exit(1);
            }
        } else {
            // This is the parent process
            int status;
            printf("parent waiting for pid %d\n",child_pid);
            waitpid(child_pid, &status, 0);
        }
    }
    
    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CNT 1000
int main() {
    int *array;
    //alloc on heap
    array = malloc(sizeof(int)*CNT);
    for(int i = 0; i<CNT; i++)
        array[i] = i;
    printf("Fork starting\n");
    int pid = fork();
    printf("fork returned %d\n",pid);
    if (pid != 0){
        for(int i = 0; i<CNT; i++){
            array[i] = i*2;
            printf("Parent array[%d]= %d\n",i,array[i]);
        }    
        waitpid(pid);
    }
    else {
        fork();
        fork();
        fork();
        fork();
    for(int i = 0; i<CNT; i++)
        printf("Child array[%d]= %d\n",i,array[i]);
    }
    free(array);
    return 0;
}

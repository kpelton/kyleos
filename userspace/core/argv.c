#include <stdio.h>

int main(int argc, char **argv) {
    
    printf("arguments %d\n",argc);
    for(int i=0; i<argc; i++ ) {
        printf("arguments %s\n",argv[i]);
    }
return 0;
}


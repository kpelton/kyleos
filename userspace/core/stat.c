#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    struct stat *st = malloc(sizeof(struct stat));
    FILE *file;
    file = fopen(argv[1],"r");
    if (file == NULL) {
        fprintf(stderr, "Could not open file: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    st->st_size =0; 
    printf("%lx\n",st);
    int retcall = fstat(fileno(file),st);
    //readdir(NULL);
    printf("STAT retvat:%d\n",retcall);
    printf("STAT Size:%d\n",st->st_size);
    printf("STAT ino:%d\n",st->st_ino);
    printf("STAT dev:%d\n",st->st_dev);
    fclose(file);
    return 0;
}

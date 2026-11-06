#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    FILE *file;
    int counter = 0;
    char path[1024];
        strncpy(path,"/2/data.txt",10);
    printf("opening file\n");
    // Open file for writing

    file = fopen(path,"w");
    if (file == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    // Write incrementing numbers every 1 second
    while (1) {
        fprintf(file, "%d\n", counter);
        fflush(file); // Flush the output buffer
        counter++;
    }

    // Close file
    fclose(file);

    return 0;
}

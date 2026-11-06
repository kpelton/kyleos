#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    FILE *file;
    int counter = 0;
    printf("opening file\n");
    // Open file for writing
    file = fopen("/2/test.txt","w");
    if (file == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    // Write incrementing numbers every 1 second
    while (1) {
        fprintf(file, "%d\n", counter);
        fflush(file); // Flush the output buffer
        counter++;
        sleep(1); // Wait for 1 second
    }

    // Close file
    fclose(file);

    return 0;
}

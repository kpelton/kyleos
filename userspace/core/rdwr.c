#include <stdio.h>
#include <stdlib.h>

#define FILENAME "/2/data.txt"
#define NUM_BYTES 500000

int main() {
    FILE *file;
    char *data = malloc(NUM_BYTES);
    char *read_data = malloc(NUM_BYTES);
    for (;;) {
    // Fill data array with ASCII characters
    for (int i = 0; i < NUM_BYTES; i++) {
        data[i] = (char)((i % 94) + 33); // ASCII printable characters start from 33 ('!') to 126 ('~')
    }

    // Writing data to file
    file = fopen(FILENAME, "w");
    if (file == NULL) {
        fprintf(stderr, "Error opening file for writing.\n");
        return 1;
    }
    fwrite(data, sizeof(char), NUM_BYTES, file);
    fclose(file);

    // Reading data from file
    file = fopen(FILENAME, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file for reading.\n");
        return 1;
    }
    fread(read_data, sizeof(char), NUM_BYTES, file);
    fclose(file);

    // Checking for correctness
    for (int i = 0; i < NUM_BYTES; i++) {
        if (data[i] != read_data[i]) {
            printf("Error: Data mismatch at position %d.\n", i);
            return 1;
        }
    }

    for (int i = 0; i < NUM_BYTES; i++)
        data[i] = 0;
    //printf("Data read successfully and matches the original data.\n");
    }
    return 0;
}

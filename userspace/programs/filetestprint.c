#include <stdio.h>

int main() {
    FILE *file;
    char *filename ="/bible.txt";
    char ch;
    while (1) {
    // Open the file in read mode
    file = fopen(filename, "r");

    if (file == NULL) {
        printf("Error opening the file.\n");
        return 1;
    }

    printf("File contents:\n");

    // Read and print each character until the end of file
    while ((ch = fgetc(file)) != EOF) {
        printf("%c", ch);
    }

    // Close the file
    fclose(file);
}
    return 0;
}

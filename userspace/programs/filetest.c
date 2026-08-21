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
    fclose(file);
    printf("File contents:\n");
    continue;
    // Read and print each character until the end of file
//    ch = fgetc(file);
    //while ((ch = fgetc(file)) != EOF);

    // Close the file
    fclose(file);
}
    return 0;
}

#include <stdio.h>

int main() {
    FILE *file = fopen("/2/console", "w");
    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    fprintf(file, "Hello World");
    fclose(file);
    return 0;
}

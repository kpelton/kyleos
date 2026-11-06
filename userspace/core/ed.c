#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FILE_CONTENT_LENGTH 8192

int main(int argc, char **argv) {
    char content[MAX_FILE_CONTENT_LENGTH];
    char line[512];
    char *filename;
    FILE *file;
    size_t used = 0;

    if (argc == 2) {
        filename = argv[1];
    } else if (argc == 3 && (strcmp(argv[1], "a") == 0 || strcmp(argv[1], "n") == 0)) {
        filename = argv[2];
    } else {
        printf("Usage: %s <file>\n", argv[0]);
        printf("Finish editing with a line containing only a dot.\n");
        exit(1);
    }
    content[0] = '\0';
    printf("Editing %s. Enter text; a single . saves.\n", filename);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (strcmp(line, ".\n") == 0 || strcmp(line, ".") == 0)
            break;
        if (used + strlen(line) >= sizeof(content)) {
            printf("File is limited to %d bytes.\n", MAX_FILE_CONTENT_LENGTH);
            return 1;
        }
        strcpy(content + used, line);
        used += strlen(line);
    }
    file = fopen(filename, "w");
    if (file == NULL) {
        perror(filename);
        return 1;
    }
    fputs(content, file);
    fclose(file);
    printf("Saved %d bytes.\n", (int)used);
    return 0;
}

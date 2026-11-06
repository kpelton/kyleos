#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 512

int main(int argc, char *argv[])
{
    FILE *input = stdin;
    char line[LINE_SIZE];
    const char *pattern;

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s pattern [file]\n", argv[0]);
        return 1;
    }
    pattern = argv[1];
    if (argc == 3) {
        input = fopen(argv[2], "r");
        if (input == NULL) {
            perror(argv[2]);
            return 1;
        }
    }
    while (fgets(line, sizeof(line), input) != NULL)
        if (strstr(line, pattern) != NULL)
            fputs(line, stdout);
    if (input != stdin)
        fclose(input);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINES 10
#define LINE_SIZE 512

int main(int argc, char **argv)
{
    FILE *f = stdin;
    char lines[LINES][LINE_SIZE];
    char line[LINE_SIZE];
    int count = 0;
    if (argc > 2) { fprintf(stderr, "Usage: tail [file]\n"); return 1; }
    if (argc == 2 && (f = fopen(argv[1], "r")) == NULL) { perror(argv[1]); return 1; }
    while (fgets(line, sizeof(line), f) != NULL) {
        strcpy(lines[count % LINES], line);
        count++;
    }
    if (f != stdin) fclose(f);
    int first = count > LINES ? count - LINES : 0;
    for (int i = first; i < count; i++) fputs(lines[i % LINES], stdout);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *f = stdin;
    int c, lines = 0;
    if (argc > 2) { fprintf(stderr, "Usage: head [file]\n"); return 1; }
    if (argc == 2 && (f = fopen(argv[1], "r")) == NULL) { perror(argv[1]); return 1; }
    while (lines < 10 && (c = fgetc(f)) != EOF) {
        putchar(c);
        if (c == '\n') lines++;
    }
    if (f != stdin) fclose(f);
    return 0;
}

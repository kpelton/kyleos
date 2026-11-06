#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *f = stdin;
    long bytes = 0, lines = 0, words = 0;
    int c, in_word = 0;
    if (argc > 2) { fprintf(stderr, "Usage: wc [file]\n"); return 1; }
    if (argc == 2 && (f = fopen(argv[1], "r")) == NULL) { perror(argv[1]); return 1; }
    while ((c = fgetc(f)) != EOF) {
        bytes++;
        if (c == '\n') lines++;
        if (c == ' ' || c == '\n' || c == '\t') in_word = 0;
        else if (!in_word) { words++; in_word = 1; }
    }
    if (f != stdin) fclose(f);
    printf("%d %d %d\n", (int)lines, (int)words, (int)bytes);
    return 0;
}

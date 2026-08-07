#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define EDIT_MAX 8192

static void raw_mode(int enabled)
{
    asm volatile("mov $27, %%rax; int $0x80" : : "D"(enabled) : "rax");
}

static int move_vertical(const char *text, int len, int pos, int direction)
{
    int start = pos;
    int column;
    int target_start;
    int target_end;

    while (start > 0 && text[start - 1] != '\n') start--;
    column = pos - start;
    if (direction < 0) {
        if (start == 0) return pos;
        target_end = start - 1;
        target_start = target_end;
        while (target_start > 0 && text[target_start - 1] != '\n') target_start--;
    } else {
        target_start = pos;
        while (target_start < len && text[target_start] != '\n') target_start++;
        if (target_start == len) return pos;
        target_start++;
        target_end = target_start;
        while (target_end < len && text[target_end] != '\n') target_end++;
    }
    return target_start + (column < target_end - target_start ? column : target_end - target_start);
}

static void redraw(const char *name, const char *text, int len, int pos, int dirty)
{
    int row = 1, col = 1, cursor_row = 1, cursor_col = 1;
    printf("\033[2J\033[Hkedit %s [%s]  Ctrl-S save  Ctrl-Q quit\r\n",
           name, dirty ? "modified" : "saved");
    for (int i = 0; i < len; i++) {
        if (i == pos) { cursor_row = row; cursor_col = col; }
        putchar(text[i]);
        if (text[i] == '\n') { row++; col = 1; } else col++;
    }
    if (pos == len) { cursor_row = row; cursor_col = col; }
    printf("\033[%d;%dH", cursor_row + 1, cursor_col);
    fflush(stdout);
}

static int save(const char *name, const char *text, int len)
{
    FILE *f = fopen(name, "w");
    if (f == NULL) return -1;
    if (len) fwrite(text, 1, len, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    char text[EDIT_MAX];
    int len = 0, pos = 0, dirty = 0;
    FILE *f;
    if (argc != 2) { fprintf(stderr, "Usage: kedit <file>\n"); return 1; }
    f = fopen(argv[1], "r");
    if (f != NULL) { len = fread(text, 1, EDIT_MAX - 1, f); fclose(f); }
    raw_mode(1);
    for (;;) {
        char ch;
        redraw(argv[1], text, len, pos, dirty);
        if (read(0, &ch, 1) != 1) continue;
        if (ch == 19) { if (save(argv[1], text, len) == 0) dirty = 0; continue; }
        if (ch == 17) break;
        if (ch == 27) {
            char seq[3];
            if (read(0, &seq[0], 1) != 1 || read(0, &seq[1], 1) != 1) continue;
            if (seq[0] == '[' && seq[1] == 'D' && pos > 0) pos--;
            if (seq[0] == '[' && seq[1] == 'C' && pos < len) pos++;
            if (seq[0] == '[' && seq[1] == 'A') pos = move_vertical(text, len, pos, -1);
            if (seq[0] == '[' && seq[1] == 'B') pos = move_vertical(text, len, pos, 1);
            if (seq[0] == '[' && seq[1] == '3') {
                if (read(0, &seq[2], 1) == 1 && seq[2] == '~' && pos < len) {
                    memmove(text + pos, text + pos + 1, len - pos - 1);
                    len--;
                    dirty = 1;
                }
            }
            continue;
        }
        if ((ch == 8 || ch == 127) && pos > 0) {
            memmove(text + pos - 1, text + pos, len - pos); pos--; len--; dirty = 1; continue;
        }
        if ((ch == '\r' || ch == '\n') && len < EDIT_MAX - 1) {
            memmove(text + pos + 1, text + pos, len - pos); text[pos++] = '\n'; len++; dirty = 1; continue;
        }
        if (ch >= 32 && ch < 127 && len < EDIT_MAX - 1) {
            memmove(text + pos + 1, text + pos, len - pos); text[pos++] = ch; len++; dirty = 1;
        }
    }
    raw_mode(0);
    printf("\033[2J\033[H");
    return 0;
}

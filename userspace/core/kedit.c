#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define EDIT_MAX 8192

static int key_poll(int *pressed, uint8_t *scancode, uint8_t *extended)
{
    long result;
    asm volatile("mov $24, %%rax; int $0x80; movq %%rax, %0" : "=g"(result));
    return (int)result;
}

static void tty_raw(int enabled)
{
    asm volatile("mov $27, %%rax; int $0x80" : : "D"(enabled) : "rax");
}

static char scan_to_char(uint8_t scan)
{
    static const char table[] = {
        0, 0, '1','2','3','4','5','6','7','8','9','0','-','=',0,0,
        'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
        'a','s','d','f','g','h','j','k','l',';',39,'`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,0,0,' '
    };
    return scan < sizeof(table) ? table[scan] : 0;
}

static void draw(const char *name, const char *text, int length, int cursor, int dirty)
{
    int row = 1;
    int col = 1;
    int crow = 1;
    int ccol = 1;
    printf("\033[2J\033[H");
    printf("kedit: %s  %s   Ctrl-S save  Ctrl-Q quit\r\n\033[K", name,
           dirty ? "modified" : "saved");
    for (int i = 0; i < length; i++) {
        if (i == cursor) { crow = row; ccol = col; }
        putchar(text[i]);
        if (text[i] == '\n') { row++; col = 1; }
        else col++;
    }
    if (cursor == length) { crow = row; ccol = col; }
    printf("\033[%d;%dH", crow + 1, ccol);
    fflush(stdout);
}

static int save_file(const char *name, const char *text, int length)
{
    FILE *file = fopen(name, "w");
    if (file == NULL) return -1;
    if (length > 0) fwrite(text, 1, length, file);
    fclose(file);
    return 0;
}

int main(int argc, char **argv)
{
    char text[EDIT_MAX];
    int length = 0, cursor = 0, dirty = 0, ctrl = 0;
    FILE *file;

    if (argc != 2) {
        fprintf(stderr, "Usage: kedit <file>\n");
        return 1;
    }
    file = fopen(argv[1], "r");
    if (file != NULL) {
        length = fread(text, 1, EDIT_MAX - 1, file);
        fclose(file);
    }
    tty_raw(1);
    while (key_poll(&(int){0}, &(uint8_t){0}, &(uint8_t){0})) { }
    for (;;) {
        int pressed;
        uint8_t scan, extended;
        draw(argv[1], text, length, cursor, dirty);
        while (!key_poll(&pressed, &scan, &extended)) { }
        if (!pressed) {
            if (scan == 0x1d) ctrl = 0;
            continue;
        }
        if (scan == 0x1d) { ctrl = 1; continue; }
        if (ctrl && scan == 0x1f) {
            if (save_file(argv[1], text, length) == 0) dirty = 0;
            continue;
        }
        if (ctrl && scan == 0x10) break;
        if (extended && scan == 0x4b && cursor > 0) { cursor--; continue; }
        if (extended && scan == 0x4d && cursor < length) { cursor++; continue; }
        if (scan == 0x0e && cursor > 0) {
            memmove(text + cursor - 1, text + cursor, length - cursor);
            cursor--; length--; dirty = 1; continue;
        }
        if (scan == 0x1c && length < EDIT_MAX - 1) {
            memmove(text + cursor + 1, text + cursor, length - cursor);
            text[cursor++] = '\n'; length++; dirty = 1; continue;
        }
        char ch = scan_to_char(scan);
        if (ch && length < EDIT_MAX - 1) {
            memmove(text + cursor + 1, text + cursor, length - cursor);
            text[cursor++] = ch; length++; dirty = 1;
        }
    }
    tty_raw(0);
    printf("\033[2J\033[H");
    return 0;
}

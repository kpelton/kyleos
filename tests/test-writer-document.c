#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "extras/wordprocessor/writer_document.h"

#define DOCUMENT_PATH "/tmp/os3-writer-document.kwp"
#define PLAIN_PATH "/tmp/os3-writer-plain.txt"
#define LARGE_TEXT_LENGTH 20000

static int check(int condition, const char *message)
{
    if (!condition) fprintf(stderr, "FAIL writer document: %s\n", message);
    return !condition;
}

static int write_plain_file(void)
{
    static const char text[] = "plain text\nsecond line";
    int fd = open(PLAIN_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) return -1;
    if (write(fd, text, sizeof(text) - 1) != (int)sizeof(text) - 1) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int main(void)
{
    struct writer_document saved;
    struct writer_document loaded;
    int failures = 0;

    writer_document_init(&saved);
    writer_document_init(&loaded);
    failures += check(saved.font_scale == 1 && saved.bold == 0 &&
                      saved.text_length == 0,
                      "new document defaults");
    strcpy(saved.text, "A styled document\nwith two lines.");
    saved.text_length = (int)strlen(saved.text);
    saved.font_scale = 3;
    saved.bold = 1;
    failures += check(writer_document_save(&saved, DOCUMENT_PATH) == 0,
                      "save KWP file");
    failures += check(writer_document_load(&loaded, DOCUMENT_PATH) == 0,
                      "load KWP file");
    failures += check(loaded.font_scale == 3 && loaded.bold == 1,
                      "formatting round trip");
    failures += check(loaded.text_length == saved.text_length &&
                      strcmp(loaded.text, saved.text) == 0,
                      "text round trip");

    failures += check(write_plain_file() == 0, "create plain-text fixture");
    failures += check(writer_document_load(&loaded, PLAIN_PATH) == 0,
                      "open plain text");
    failures += check(loaded.font_scale == 1 && loaded.bold == 0,
                      "plain-text formatting defaults");
    failures += check(strcmp(loaded.text, "plain text\nsecond line") == 0,
                      "plain-text contents");

    failures += check(
        writer_document_reserve(&saved, LARGE_TEXT_LENGTH + 1) == 0,
        "grow document beyond initial capacity");
    if (saved.text_capacity > LARGE_TEXT_LENGTH) {
        for (int index = 0; index < LARGE_TEXT_LENGTH; index++)
            saved.text[index] = (char)('a' + index % 26);
        saved.text[LARGE_TEXT_LENGTH] = '\0';
        saved.text_length = LARGE_TEXT_LENGTH;
        saved.font_scale = 2;
        saved.bold = 0;
        failures += check(writer_document_save(&saved, DOCUMENT_PATH) == 0,
                          "save large document");
        failures += check(writer_document_load(&loaded, DOCUMENT_PATH) == 0,
                          "load large document");
        failures += check(loaded.text_length == LARGE_TEXT_LENGTH &&
                          loaded.text_capacity > LARGE_TEXT_LENGTH &&
                          memcmp(loaded.text, saved.text,
                                 LARGE_TEXT_LENGTH) == 0,
                          "large document round trip without truncation");
    }

    writer_document_destroy(&saved);
    writer_document_destroy(&loaded);
    unlink(DOCUMENT_PATH);
    unlink(PLAIN_PATH);
    if (failures == 0) puts("Writer document tests passed");
    return failures != 0;
}

#include "extras/wordprocessor/writer_document.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WRITER_HEADER_MAX 48

static int write_full(int fd, const char *data, int length)
{
    int total = 0;

    while (total < length) {
        int count = write(fd, data + total, (size_t)(length - total));

        if (count <= 0) return -1;
        total += count;
    }
    return 0;
}

void writer_document_init(struct writer_document *document)
{
    memset(document, 0, sizeof(*document));
    document->font_scale = 1;
    writer_document_reserve(document, WRITER_TEXT_INITIAL_CAPACITY);
}

void writer_document_destroy(struct writer_document *document)
{
    if (document == NULL) return;
    free(document->text);
    memset(document, 0, sizeof(*document));
}

int writer_document_reserve(struct writer_document *document, int capacity)
{
    char *grown;
    int new_capacity;

    if (document == NULL || capacity <= 0) return -1;
    if (capacity <= document->text_capacity) return 0;
    new_capacity = document->text_capacity > 0 ? document->text_capacity :
                                                WRITER_TEXT_INITIAL_CAPACITY;
    while (new_capacity < capacity) {
        if (new_capacity > INT_MAX / 2) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2;
    }
    grown = realloc(document->text, (size_t)new_capacity);
    if (grown == NULL) return -1;
    document->text = grown;
    document->text_capacity = new_capacity;
    if (document->text_length == 0) document->text[0] = '\0';
    return 0;
}

int writer_document_load(struct writer_document *document, const char *path)
{
    struct writer_document loaded;
    char *input = NULL;
    int input_capacity = 0;
    int input_length = 0;
    int fd;
    int count;
    int scale;
    int bold;
    char *body;

    writer_document_init(&loaded);
    if (loaded.text == NULL) return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        writer_document_destroy(&loaded);
        return -1;
    }
    input_capacity = WRITER_TEXT_INITIAL_CAPACITY;
    input = malloc((size_t)input_capacity);
    if (input == NULL) {
        close(fd);
        writer_document_destroy(&loaded);
        return -1;
    }
    for (;;) {
        char *grown;

        if (input_length == input_capacity - 1) {
            if (input_capacity > INT_MAX / 2) {
                free(input);
                close(fd);
                writer_document_destroy(&loaded);
                return -1;
            }
            input_capacity *= 2;
            grown = realloc(input, (size_t)input_capacity);
            if (grown == NULL) {
                free(input);
                close(fd);
                writer_document_destroy(&loaded);
                return -1;
            }
            input = grown;
        }
        count = read(fd, input + input_length,
                     (size_t)(input_capacity - input_length - 1));
        if (count <= 0) break;
        input_length += count;
    }
    close(fd);
    if (count < 0) {
        free(input);
        writer_document_destroy(&loaded);
        return -1;
    }
    input[input_length] = '\0';

    body = strchr(input, '\n');
    if (body != NULL && sscanf(input, "KWP1 size=%d bold=%d", &scale,
                               &bold) == 2 &&
        scale >= WRITER_FONT_MIN && scale <= WRITER_FONT_MAX &&
        (bold == 0 || bold == 1)) {
        body++;
        loaded.font_scale = scale;
        loaded.bold = bold;
    } else {
        body = input;
    }
    loaded.text_length = input_length - (int)(body - input);
    if (writer_document_reserve(&loaded, loaded.text_length + 1) < 0) {
        free(input);
        writer_document_destroy(&loaded);
        return -1;
    }
    memcpy(loaded.text, body, (size_t)loaded.text_length);
    loaded.text[loaded.text_length] = '\0';
    free(input);
    writer_document_destroy(document);
    *document = loaded;
    return 0;
}

int writer_document_save(const struct writer_document *document,
                         const char *path)
{
    char header[WRITER_HEADER_MAX];
    int header_length;
    int fd;
    int failed;

    if (document == NULL || path == NULL ||
        document->font_scale < WRITER_FONT_MIN ||
        document->font_scale > WRITER_FONT_MAX ||
        (document->bold != 0 && document->bold != 1) ||
        document->text == NULL || document->text_length < 0 ||
        document->text_length >= document->text_capacity)
        return -1;
    header_length = snprintf(header, sizeof(header),
                             "KWP1 size=%d bold=%d\n",
                             document->font_scale, document->bold);
    if (header_length <= 0 || header_length >= (int)sizeof(header)) return -1;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    failed = write_full(fd, header, header_length) < 0 ||
             write_full(fd, document->text, document->text_length) < 0;
    close(fd);
    return failed ? -1 : 0;
}

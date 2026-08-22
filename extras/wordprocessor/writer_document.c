#include "extras/wordprocessor/writer_document.h"

#include <fcntl.h>
#include <stdio.h>
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
}

int writer_document_load(struct writer_document *document, const char *path)
{
    struct writer_document loaded;
    char input[WRITER_TEXT_MAX + WRITER_HEADER_MAX];
    int fd;
    int count;
    int scale;
    int bold;
    char *body;

    writer_document_init(&loaded);
    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    count = read(fd, input, sizeof(input) - 1);
    close(fd);
    if (count < 0) return -1;
    input[count] = '\0';

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
    loaded.text_length = (int)strlen(body);
    if (loaded.text_length >= WRITER_TEXT_MAX)
        loaded.text_length = WRITER_TEXT_MAX - 1;
    memcpy(loaded.text, body, (size_t)loaded.text_length);
    loaded.text[loaded.text_length] = '\0';
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
        document->text_length < 0 ||
        document->text_length >= WRITER_TEXT_MAX)
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

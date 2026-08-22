#ifndef KYLEOS_WRITER_DOCUMENT_H
#define KYLEOS_WRITER_DOCUMENT_H

#define WRITER_TEXT_MAX 8192
#define WRITER_PATH_MAX 128
#define WRITER_FONT_MIN 1
#define WRITER_FONT_MAX 3

struct writer_document {
    char text[WRITER_TEXT_MAX];
    int text_length;
    int font_scale;
    int bold;
};

void writer_document_init(struct writer_document *document);
int writer_document_load(struct writer_document *document, const char *path);
int writer_document_save(const struct writer_document *document,
                         const char *path);

#endif

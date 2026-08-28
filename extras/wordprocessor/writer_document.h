#ifndef KYLEOS_WRITER_DOCUMENT_H
#define KYLEOS_WRITER_DOCUMENT_H

#define WRITER_TEXT_INITIAL_CAPACITY 8192
#define WRITER_PATH_MAX 128
#define WRITER_FONT_MIN 1
#define WRITER_FONT_MAX 3

struct writer_document {
    /* Text storage grows on demand; text_capacity includes the terminator. */
    char *text;
    int text_length;
    int text_capacity;
    int font_scale;
    int bold;
};

void writer_document_init(struct writer_document *document);
void writer_document_destroy(struct writer_document *document);
int writer_document_reserve(struct writer_document *document, int capacity);
/* Replaces an initialized document on success and leaves it intact on error. */
int writer_document_load(struct writer_document *document, const char *path);
int writer_document_save(const struct writer_document *document,
                         const char *path);

#endif

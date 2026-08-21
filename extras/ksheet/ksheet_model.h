#ifndef KYLEOS_KSHEET_MODEL_H
#define KYLEOS_KSHEET_MODEL_H

#include <stddef.h>

#define KSHEET_DEFAULT_ROWS 100
#define KSHEET_DEFAULT_COLS 26
#define KSHEET_MAX_ROWS 65536
#define KSHEET_MAX_COLS 4096
#define KSHEET_MAX_CELLS 65536
#define KSHEET_CELL_TEXT_MAX 64

enum ksheet_error {
    KSHEET_OK,
    KSHEET_ERROR_PARSE,
    KSHEET_ERROR_DIV_ZERO,
    KSHEET_ERROR_REF,
    KSHEET_ERROR_VALUE,
    KSHEET_ERROR_CYCLE
};

struct ksheet_slot;

struct ksheet {
    struct ksheet_slot *slots;
    size_t slot_capacity;
    size_t cell_count;
    size_t tombstone_count;
    int row_count;
    int column_count;
};

void ksheet_init(struct ksheet *sheet);
void ksheet_clear(struct ksheet *sheet);
void ksheet_destroy(struct ksheet *sheet);
int ksheet_ensure_size(struct ksheet *sheet, int rows, int columns);
int ksheet_row_count(const struct ksheet *sheet);
int ksheet_column_count(const struct ksheet *sheet);
int ksheet_set_cell(struct ksheet *sheet, int row, int column,
                    const char *input);
const char *ksheet_cell_input(const struct ksheet *sheet,
                              int row, int column);
int ksheet_evaluate_cell(const struct ksheet *sheet, int row, int column,
                         double *value, int *error);
int ksheet_format_cell(const struct ksheet *sheet, int row, int column,
                       char *output, size_t capacity);
void ksheet_cell_name(int row, int column, char *output, size_t capacity);
void ksheet_column_name(int column, char *output, size_t capacity);

int ksheet_import_csv(struct ksheet *sheet,
                      const char *data, size_t length);
int ksheet_export_csv(const struct ksheet *sheet,
                      char *output, size_t capacity, size_t *length);

#endif

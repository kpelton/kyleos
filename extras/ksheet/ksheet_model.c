#include "extras/ksheet/ksheet_model.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KSHEET_INITIAL_SLOTS 128

struct ksheet_slot {
    int row;
    int column;
    unsigned char state;
    char input[KSHEET_CELL_TEXT_MAX];
};

struct formula_visit {
    int row;
    int column;
    const struct formula_visit *previous;
};

struct formula_parser {
    const struct ksheet *sheet;
    const char *input;
    size_t position;
    const struct formula_visit *visiting;
    int error;
};

static int valid_cell(int row, int column)
{
    return row >= 0 && row < KSHEET_MAX_ROWS &&
           column >= 0 && column < KSHEET_MAX_COLS;
}

static size_t cell_hash(int row, int column)
{
    uint32_t value = (uint32_t)row * 2654435761U;
    value ^= (uint32_t)column * 2246822519U;
    value ^= value >> 16;
    return (size_t)value;
}

static struct ksheet_slot *find_slot(const struct ksheet *sheet,
                                     int row, int column)
{
    size_t mask;
    size_t index;

    if (sheet == NULL || sheet->slots == NULL ||
        sheet->slot_capacity == 0)
        return NULL;
    mask = sheet->slot_capacity - 1;
    index = cell_hash(row, column) & mask;
    for (size_t probe = 0; probe < sheet->slot_capacity; probe++) {
        struct ksheet_slot *slot = &sheet->slots[index];

        if (slot->state == 0) return NULL;
        if (slot->state == 1 && slot->row == row && slot->column == column)
            return slot;
        index = (index + 1) & mask;
    }
    return NULL;
}

static void insert_existing(struct ksheet_slot *slots, size_t capacity,
                            const struct ksheet_slot *source)
{
    size_t mask = capacity - 1;
    size_t index = cell_hash(source->row, source->column) & mask;

    while (slots[index].state == 1) index = (index + 1) & mask;
    slots[index] = *source;
}

static int resize_slots(struct ksheet *sheet, size_t capacity)
{
    struct ksheet_slot *slots;

    slots = malloc(capacity * sizeof(*slots));
    if (slots == NULL) return -1;
    memset(slots, 0, capacity * sizeof(*slots));
    for (size_t index = 0; index < sheet->slot_capacity; index++)
        if (sheet->slots[index].state == 1)
            insert_existing(slots, capacity, &sheet->slots[index]);
    free(sheet->slots);
    sheet->slots = slots;
    sheet->slot_capacity = capacity;
    sheet->tombstone_count = 0;
    return 0;
}

static struct ksheet_slot *create_slot(struct ksheet *sheet,
                                       int row, int column)
{
    size_t mask;
    size_t index;
    size_t tombstone = (size_t)-1;

    if (sheet->cell_count >= KSHEET_MAX_CELLS) return NULL;
    if (sheet->slot_capacity == 0) {
        if (resize_slots(sheet, KSHEET_INITIAL_SLOTS) < 0) return NULL;
    } else if ((sheet->cell_count + sheet->tombstone_count + 1) * 10 >=
               sheet->slot_capacity * 7) {
        size_t capacity = sheet->slot_capacity;

        if (sheet->cell_count * 10 >= sheet->slot_capacity * 6)
            capacity *= 2;
        if (resize_slots(sheet, capacity) < 0) return NULL;
    }
    mask = sheet->slot_capacity - 1;
    index = cell_hash(row, column) & mask;
    for (;;) {
        struct ksheet_slot *slot = &sheet->slots[index];

        if (slot->state == 0) {
            if (tombstone != (size_t)-1) {
                index = tombstone;
                sheet->tombstone_count--;
            }
            slot = &sheet->slots[index];
            memset(slot, 0, sizeof(*slot));
            slot->state = 1;
            slot->row = row;
            slot->column = column;
            sheet->cell_count++;
            return slot;
        }
        if (slot->state == 2 && tombstone == (size_t)-1)
            tombstone = index;
        index = (index + 1) & mask;
    }
}

void ksheet_init(struct ksheet *sheet)
{
    if (sheet == NULL) return;
    memset(sheet, 0, sizeof(*sheet));
    sheet->row_count = KSHEET_DEFAULT_ROWS;
    sheet->column_count = KSHEET_DEFAULT_COLS;
}

void ksheet_clear(struct ksheet *sheet)
{
    if (sheet == NULL) return;
    if (sheet->slots != NULL)
        memset(sheet->slots, 0,
               sheet->slot_capacity * sizeof(*sheet->slots));
    sheet->cell_count = 0;
    sheet->tombstone_count = 0;
    sheet->row_count = KSHEET_DEFAULT_ROWS;
    sheet->column_count = KSHEET_DEFAULT_COLS;
}

void ksheet_destroy(struct ksheet *sheet)
{
    if (sheet == NULL) return;
    free(sheet->slots);
    memset(sheet, 0, sizeof(*sheet));
}

int ksheet_ensure_size(struct ksheet *sheet, int rows, int columns)
{
    if (sheet == NULL || rows < 1 || columns < 1 ||
        rows > KSHEET_MAX_ROWS || columns > KSHEET_MAX_COLS)
        return -1;
    if (rows > sheet->row_count) sheet->row_count = rows;
    if (columns > sheet->column_count) sheet->column_count = columns;
    return 0;
}

int ksheet_row_count(const struct ksheet *sheet)
{
    return sheet != NULL ? sheet->row_count : 0;
}

int ksheet_column_count(const struct ksheet *sheet)
{
    return sheet != NULL ? sheet->column_count : 0;
}

int ksheet_set_cell(struct ksheet *sheet, int row, int column,
                    const char *input)
{
    struct ksheet_slot *slot;

    if (sheet == NULL || !valid_cell(row, column)) return -1;
    if (input == NULL) input = "";
    slot = find_slot(sheet, row, column);
    if (input[0] == '\0') {
        if (slot != NULL) {
            slot->state = 2;
            slot->input[0] = '\0';
            sheet->cell_count--;
            sheet->tombstone_count++;
        }
        return 0;
    }
    if (slot == NULL) slot = create_slot(sheet, row, column);
    if (slot == NULL) return -1;
    strncpy(slot->input, input, KSHEET_CELL_TEXT_MAX - 1);
    slot->input[KSHEET_CELL_TEXT_MAX - 1] = '\0';
    ksheet_ensure_size(sheet, row + 1, column + 1);
    return 0;
}

const char *ksheet_cell_input(const struct ksheet *sheet,
                              int row, int column)
{
    struct ksheet_slot *slot;

    if (sheet == NULL || !valid_cell(row, column)) return "";
    slot = find_slot(sheet, row, column);
    return slot != NULL ? slot->input : "";
}

void ksheet_column_name(int column, char *output, size_t capacity)
{
    char reversed[16];
    int length = 0;

    if (output == NULL || capacity == 0) return;
    if (column < 0 || column >= KSHEET_MAX_COLS) {
        output[0] = '\0';
        return;
    }
    column++;
    while (column > 0 && length < (int)sizeof(reversed) - 1) {
        column--;
        reversed[length++] = (char)('A' + column % 26);
        column /= 26;
    }
    {
        size_t position = 0;

        while (length > 0 && position + 1 < capacity)
            output[position++] = reversed[--length];
        output[position] = '\0';
    }
}

void ksheet_cell_name(int row, int column, char *output, size_t capacity)
{
    char column_name[16];

    if (output == NULL || capacity == 0) return;
    if (!valid_cell(row, column)) {
        output[0] = '\0';
        return;
    }
    ksheet_column_name(column, column_name, sizeof(column_name));
    snprintf(output, capacity, "%s%d", column_name, row + 1);
}

static void skip_spaces(struct formula_parser *parser)
{
    while (isspace((unsigned char)parser->input[parser->position]))
        parser->position++;
}

static double parse_expression(struct formula_parser *parser);

static double evaluate_cell_internal(const struct ksheet *sheet,
                                     int row, int column,
                                     const struct formula_visit *visiting,
                                     int *error)
{
    const char *input;
    char *end;
    double value;

    if (!valid_cell(row, column)) {
        *error = KSHEET_ERROR_REF;
        return 0;
    }
    input = ksheet_cell_input(sheet, row, column);
    if (input[0] == '\0') return 0;
    if (input[0] != '=') {
        value = strtod(input, &end);
        while (isspace((unsigned char)*end)) end++;
        if (end == input || *end != '\0') {
            *error = KSHEET_ERROR_VALUE;
            return 0;
        }
        return value;
    }

    for (const struct formula_visit *visit = visiting;
         visit != NULL; visit = visit->previous) {
        if (visit->row == row && visit->column == column) {
            *error = KSHEET_ERROR_CYCLE;
            return 0;
        }
    }
    {
        struct formula_parser parser;
        struct formula_visit visit;

        visit.row = row;
        visit.column = column;
        visit.previous = visiting;
        parser.sheet = sheet;
        parser.input = input + 1;
        parser.position = 0;
        parser.visiting = &visit;
        parser.error = KSHEET_OK;
        value = parse_expression(&parser);
        skip_spaces(&parser);
        if (parser.error == KSHEET_OK &&
            parser.input[parser.position] != '\0')
            parser.error = KSHEET_ERROR_PARSE;
        *error = parser.error;
    }
    return value;
}

static int parse_cell_reference(struct formula_parser *parser,
                                int *row, int *column)
{
    size_t start;
    int parsed_row = 0;
    int parsed_column = 0;
    int letters = 0;

    skip_spaces(parser);
    start = parser->position;
    if (!isalpha((unsigned char)parser->input[parser->position])) return 0;
    while (isalpha((unsigned char)parser->input[parser->position])) {
        int digit = toupper((unsigned char)
                            parser->input[parser->position]) - 'A' + 1;

        if (parsed_column > (KSHEET_MAX_COLS - digit) / 26) {
            parser->error = KSHEET_ERROR_REF;
            return 0;
        }
        parsed_column = parsed_column * 26 + digit;
        parser->position++;
        letters++;
    }
    parsed_column--;
    if (!isdigit((unsigned char)parser->input[parser->position])) {
        parser->position = start;
        return 0;
    }
    while (isdigit((unsigned char)parser->input[parser->position])) {
        if (parsed_row > KSHEET_MAX_ROWS / 10) {
            parser->error = KSHEET_ERROR_REF;
            return 0;
        }
        parsed_row = parsed_row * 10 +
                     parser->input[parser->position] - '0';
        parser->position++;
    }
    parsed_row--;
    if (letters == 0 || !valid_cell(parsed_row, parsed_column)) {
        parser->error = KSHEET_ERROR_REF;
        return 0;
    }
    *row = parsed_row;
    *column = parsed_column;
    return 1;
}

static double parse_range_function(struct formula_parser *parser,
                                   const char *name)
{
    int first_row;
    int first_column;
    int last_row;
    int last_column;
    int row_start;
    int row_end;
    int column_start;
    int column_end;
    int count = 0;
    double total = 0;
    double result = 0;

    skip_spaces(parser);
    if (parser->input[parser->position++] != '(' ||
        !parse_cell_reference(parser, &first_row, &first_column)) {
        if (parser->error == KSHEET_OK) parser->error = KSHEET_ERROR_PARSE;
        return 0;
    }
    skip_spaces(parser);
    if (parser->input[parser->position++] != ':' ||
        !parse_cell_reference(parser, &last_row, &last_column)) {
        if (parser->error == KSHEET_OK) parser->error = KSHEET_ERROR_PARSE;
        return 0;
    }
    skip_spaces(parser);
    if (parser->input[parser->position++] != ')') {
        parser->error = KSHEET_ERROR_PARSE;
        return 0;
    }
    row_start = first_row < last_row ? first_row : last_row;
    row_end = first_row > last_row ? first_row : last_row;
    column_start = first_column < last_column ? first_column : last_column;
    column_end = first_column > last_column ? first_column : last_column;
    if ((size_t)(row_end - row_start + 1) *
        (size_t)(column_end - column_start + 1) > KSHEET_MAX_CELLS) {
        parser->error = KSHEET_ERROR_REF;
        return 0;
    }
    for (int row = row_start; row <= row_end; row++) {
        for (int column = column_start; column <= column_end; column++) {
            int error = KSHEET_OK;
            double value = evaluate_cell_internal(
                parser->sheet, row, column, parser->visiting, &error);

            if (error != KSHEET_OK) {
                parser->error = error;
                return 0;
            }
            if (count == 0 || (strcmp(name, "MIN") == 0 && value < result) ||
                (strcmp(name, "MAX") == 0 && value > result))
                result = value;
            total += value;
            count++;
        }
    }
    if (strcmp(name, "SUM") == 0) return total;
    if (strcmp(name, "AVG") == 0) return count != 0 ? total / count : 0;
    return result;
}

static double parse_factor(struct formula_parser *parser)
{
    const char *input = parser->input;
    size_t start;
    char identifier[8];
    int identifier_length = 0;
    int row;
    int column;
    double value;
    char *end;

    skip_spaces(parser);
    if (input[parser->position] == '+') {
        parser->position++;
        return parse_factor(parser);
    }
    if (input[parser->position] == '-') {
        parser->position++;
        return -parse_factor(parser);
    }
    if (input[parser->position] == '(') {
        parser->position++;
        value = parse_expression(parser);
        skip_spaces(parser);
        if (input[parser->position] != ')') {
            parser->error = KSHEET_ERROR_PARSE;
            return 0;
        }
        parser->position++;
        return value;
    }
    if (isalpha((unsigned char)input[parser->position])) {
        start = parser->position;
        while (isalpha((unsigned char)input[parser->position])) {
            if (identifier_length < (int)sizeof(identifier) - 1)
                identifier[identifier_length++] =
                    (char)toupper((unsigned char)input[parser->position]);
            parser->position++;
        }
        identifier[identifier_length] = '\0';
        skip_spaces(parser);
        if (input[parser->position] == '(') {
            if (strcmp(identifier, "SUM") != 0 &&
                strcmp(identifier, "MIN") != 0 &&
                strcmp(identifier, "MAX") != 0 &&
                strcmp(identifier, "AVG") != 0) {
                parser->error = KSHEET_ERROR_PARSE;
                return 0;
            }
            return parse_range_function(parser, identifier);
        }
        parser->position = start;
        if (!parse_cell_reference(parser, &row, &column)) {
            if (parser->error == KSHEET_OK) parser->error = KSHEET_ERROR_PARSE;
            return 0;
        }
        {
            int error = KSHEET_OK;
            value = evaluate_cell_internal(parser->sheet, row, column,
                                           parser->visiting, &error);
            if (error != KSHEET_OK) parser->error = error;
        }
        return value;
    }
    value = strtod(input + parser->position, &end);
    if (end == input + parser->position) {
        parser->error = KSHEET_ERROR_PARSE;
        return 0;
    }
    parser->position = (size_t)(end - input);
    return value;
}

static double parse_term(struct formula_parser *parser)
{
    double value = parse_factor(parser);

    while (parser->error == KSHEET_OK) {
        char operation;
        double right;

        skip_spaces(parser);
        operation = parser->input[parser->position];
        if (operation != '*' && operation != '/') break;
        parser->position++;
        right = parse_factor(parser);
        if (parser->error != KSHEET_OK) return 0;
        if (operation == '*') value *= right;
        else if (right == 0) {
            parser->error = KSHEET_ERROR_DIV_ZERO;
            return 0;
        } else value /= right;
    }
    return value;
}

static double parse_expression(struct formula_parser *parser)
{
    double value = parse_term(parser);

    while (parser->error == KSHEET_OK) {
        char operation;
        double right;

        skip_spaces(parser);
        operation = parser->input[parser->position];
        if (operation != '+' && operation != '-') break;
        parser->position++;
        right = parse_term(parser);
        if (parser->error != KSHEET_OK) return 0;
        if (operation == '+') value += right;
        else value -= right;
    }
    return value;
}

int ksheet_evaluate_cell(const struct ksheet *sheet, int row, int column,
                         double *value, int *error)
{
    int local_error = KSHEET_OK;
    double local_value;

    if (sheet == NULL || !valid_cell(row, column)) {
        if (error != NULL) *error = KSHEET_ERROR_REF;
        if (value != NULL) *value = 0;
        return -1;
    }
    local_value = evaluate_cell_internal(sheet, row, column,
                                         NULL, &local_error);
    if (value != NULL) *value = local_value;
    if (error != NULL) *error = local_error;
    return local_error == KSHEET_OK ? 0 : -1;
}

static const char *error_text(int error)
{
    switch (error) {
    case KSHEET_ERROR_DIV_ZERO: return "#DIV/0!";
    case KSHEET_ERROR_REF: return "#REF!";
    case KSHEET_ERROR_VALUE: return "#VALUE!";
    case KSHEET_ERROR_CYCLE: return "#CYCLE!";
    default: return "#ERROR!";
    }
}

int ksheet_format_cell(const struct ksheet *sheet, int row, int column,
                       char *output, size_t capacity)
{
    const char *input;
    double value;
    int error;

    if (output == NULL || capacity == 0) return -1;
    output[0] = '\0';
    input = ksheet_cell_input(sheet, row, column);
    if (input[0] != '=') {
        strncpy(output, input, capacity - 1);
        output[capacity - 1] = '\0';
        return 0;
    }
    if (ksheet_evaluate_cell(sheet, row, column, &value, &error) < 0) {
        strncpy(output, error_text(error), capacity - 1);
        output[capacity - 1] = '\0';
        return -1;
    }
    snprintf(output, capacity, "%.10g", value);
    return 0;
}

static int csv_append(char *output, size_t capacity, size_t *position,
                      char value)
{
    if (*position + 1 >= capacity) return -1;
    output[(*position)++] = value;
    output[*position] = '\0';
    return 0;
}

int ksheet_export_csv(const struct ksheet *sheet,
                      char *output, size_t capacity, size_t *length)
{
    int last_row = -1;
    int *last_columns = NULL;
    size_t position = 0;
    int result = -1;

    if (sheet == NULL || output == NULL || capacity == 0) return -1;
    output[0] = '\0';
    for (size_t index = 0; index < sheet->slot_capacity; index++)
        if (sheet->slots[index].state == 1 &&
            sheet->slots[index].row > last_row)
            last_row = sheet->slots[index].row;
    if (last_row >= 0) {
        last_columns = malloc((size_t)(last_row + 1) * sizeof(*last_columns));
        if (last_columns == NULL) return -1;
        for (int row = 0; row <= last_row; row++) last_columns[row] = -1;
        for (size_t index = 0; index < sheet->slot_capacity; index++) {
            const struct ksheet_slot *slot = &sheet->slots[index];

            if (slot->state == 1 &&
                slot->column > last_columns[slot->row])
                last_columns[slot->row] = slot->column;
        }
    }
    for (int row = 0; row <= last_row; row++) {
        int last_column = last_columns[row];

        for (int column = 0; column <= last_column; column++) {
            const char *input = ksheet_cell_input(sheet, row, column);
            int quoted = strchr(input, ',') != NULL ||
                         strchr(input, '"') != NULL ||
                         strchr(input, '\n') != NULL ||
                         strchr(input, '\r') != NULL;

            if (column > 0 && csv_append(output, capacity, &position, ',') < 0)
                goto done;
            if (quoted && csv_append(output, capacity, &position, '"') < 0)
                goto done;
            for (size_t index = 0; input[index] != '\0'; index++) {
                if (input[index] == '"' &&
                    csv_append(output, capacity, &position, '"') < 0)
                    goto done;
                if (csv_append(output, capacity, &position, input[index]) < 0)
                    goto done;
            }
            if (quoted && csv_append(output, capacity, &position, '"') < 0)
                goto done;
        }
        if (csv_append(output, capacity, &position, '\n') < 0) goto done;
    }
    if (length != NULL) *length = position;
    result = 0;
done:
    free(last_columns);
    return result;
}

int ksheet_import_csv(struct ksheet *sheet,
                      const char *data, size_t length)
{
    char field[KSHEET_CELL_TEXT_MAX];
    size_t field_length = 0;
    size_t position = 0;
    int row = 0;
    int column = 0;
    int maximum_column = 0;
    int quoted = 0;

    if (sheet == NULL || data == NULL) return -1;
    ksheet_clear(sheet);
    while (position <= length && row < KSHEET_MAX_ROWS) {
        char ch = position < length ? data[position] : '\0';

        if (quoted) {
            if (ch == '"' && position + 1 < length &&
                data[position + 1] == '"') {
                if (field_length < sizeof(field) - 1)
                    field[field_length++] = '"';
                position += 2;
                continue;
            }
            if (ch == '"') {
                quoted = 0;
                position++;
                continue;
            }
            if (ch == '\0') return -1;
            if (field_length < sizeof(field) - 1)
                field[field_length++] = ch;
            position++;
            continue;
        }
        if (ch == '"' && field_length == 0) {
            quoted = 1;
            position++;
            continue;
        }
        if (ch != ',' && ch != '\n' && ch != '\r' && ch != '\0') {
            if (field_length < sizeof(field) - 1)
                field[field_length++] = ch;
            position++;
            continue;
        }
        field[field_length] = '\0';
        if (column >= KSHEET_MAX_COLS ||
            ksheet_set_cell(sheet, row, column, field) < 0)
            return -1;
        if (column > maximum_column) maximum_column = column;
        field_length = 0;
        if (ch == ',') {
            column++;
            position++;
        } else {
            row++;
            column = 0;
            if (ch == '\r' && position + 1 < length &&
                data[position + 1] == '\n')
                position += 2;
            else if (ch != '\0') position++;
            else break;
        }
    }
    if (row >= KSHEET_MAX_ROWS && position <= length) return -1;
    if (ksheet_ensure_size(sheet,
                           row > 0 ? row : KSHEET_DEFAULT_ROWS,
                           maximum_column + 1 > KSHEET_DEFAULT_COLS ?
                           maximum_column + 1 : KSHEET_DEFAULT_COLS) < 0)
        return -1;
    return quoted ? -1 : 0;
}

#include <stdio.h>
#include <string.h>

#include "extras/ksheet/ksheet_model.h"

static int failures;

static void expect_int(const char *name, int actual, int expected)
{
    if (actual == expected) return;
    fprintf(stderr, "FAIL %s: got %d, expected %d\n",
            name, actual, expected);
    failures++;
}

static void expect_number(const char *name, double actual, double expected)
{
    double difference = actual - expected;
    if (difference < 0) difference = -difference;
    if (difference <= 0.000001) return;
    fprintf(stderr, "FAIL %s: got %.8f, expected %.8f\n",
            name, actual, expected);
    failures++;
}

static void expect_text(const char *name, const char *actual,
                        const char *expected)
{
    if (strcmp(actual, expected) == 0) return;
    fprintf(stderr, "FAIL %s: got '%s', expected '%s'\n",
            name, actual, expected);
    failures++;
}

static double evaluate(const struct ksheet *sheet, int row, int column,
                       int expected_error, const char *name)
{
    double value = 0;
    int error = -1;
    int result = ksheet_evaluate_cell(sheet, row, column, &value, &error);

    expect_int(name, error, expected_error);
    expect_int("evaluation return", result,
               expected_error == KSHEET_OK ? 0 : -1);
    return value;
}

int main(void)
{
    struct ksheet sheet;
    struct ksheet imported;
    char output[512];
    char formatted[64];
    char name[16];
    size_t output_length = 0;

    ksheet_init(&sheet);
    ksheet_init(&imported);
    ksheet_set_cell(&sheet, 0, 0, "2");
    ksheet_set_cell(&sheet, 1, 0, "3");
    ksheet_set_cell(&sheet, 0, 1, "=A1+A2*4");
    ksheet_set_cell(&sheet, 1, 1, "=(A1+A2)*4");
    ksheet_set_cell(&sheet, 0, 2, "=SUM(A1:A2)");
    ksheet_set_cell(&sheet, 1, 2, "=avg(A1:A2)");
    ksheet_set_cell(&sheet, 2, 2, "=MIN(A2:A1)");
    ksheet_set_cell(&sheet, 3, 2, "=MAX(A1:A2)");

    expect_number("precedence", evaluate(&sheet, 0, 1, KSHEET_OK,
                                          "precedence error"), 14);
    expect_number("parentheses", evaluate(&sheet, 1, 1, KSHEET_OK,
                                           "parentheses error"), 20);
    expect_number("sum", evaluate(&sheet, 0, 2, KSHEET_OK,
                                   "sum error"), 5);
    expect_number("average", evaluate(&sheet, 1, 2, KSHEET_OK,
                                       "average error"), 2.5);
    expect_number("minimum", evaluate(&sheet, 2, 2, KSHEET_OK,
                                       "minimum error"), 2);
    expect_number("maximum", evaluate(&sheet, 3, 2, KSHEET_OK,
                                       "maximum error"), 3);

    ksheet_set_cell(&sheet, 0, 3, "=A1/0");
    evaluate(&sheet, 0, 3, KSHEET_ERROR_DIV_ZERO, "division by zero");
    ksheet_format_cell(&sheet, 0, 3, formatted, sizeof(formatted));
    expect_text("division display", formatted, "#DIV/0!");

    ksheet_set_cell(&sheet, 0, 4, "=E2");
    ksheet_set_cell(&sheet, 1, 4, "=E1");
    evaluate(&sheet, 0, 4, KSHEET_ERROR_CYCLE, "cycle");
    ksheet_set_cell(&sheet, 0, 5, "hello");
    ksheet_set_cell(&sheet, 0, 6, "=F1+1");
    evaluate(&sheet, 0, 6, KSHEET_ERROR_VALUE, "text reference");
    ksheet_set_cell(&sheet, 0, 7, "=AA1");
    expect_number("empty AA reference",
                  evaluate(&sheet, 0, 7, KSHEET_OK,
                           "AA reference error"), 0);
    ksheet_set_cell(&sheet, 100, 26, "9");
    ksheet_set_cell(&sheet, 100, 27, "=AA101+1");
    expect_number("dynamic row and column",
                  evaluate(&sheet, 100, 27, KSHEET_OK,
                           "dynamic reference error"), 10);
    expect_int("dynamic rows", ksheet_row_count(&sheet), 101);
    expect_int("dynamic columns", ksheet_column_count(&sheet), 28);
    expect_int("maximum sparse cell",
               ksheet_set_cell(&sheet, KSHEET_MAX_ROWS - 1,
                               KSHEET_MAX_COLS - 1, "42"), 0);
    ksheet_set_cell(&sheet, 2, 8, "=FAN65536+1");
    expect_number("maximum sparse reference",
                  evaluate(&sheet, 2, 8, KSHEET_OK,
                           "maximum sparse reference error"), 43);
    expect_int("maximum dynamic rows", ksheet_row_count(&sheet),
               KSHEET_MAX_ROWS);
    expect_int("maximum dynamic columns", ksheet_column_count(&sheet),
               KSHEET_MAX_COLS);

    for (int index = 0; index < 300; index++)
        expect_int("sparse hash insert",
                   ksheet_set_cell(&sheet, 200 + index, index % 40, "1"), 0);
    for (int index = 0; index < 300; index += 2) {
        expect_int("sparse hash delete",
                   ksheet_set_cell(&sheet, 200 + index, index % 40, ""), 0);
        expect_int("sparse hash reinsert",
                   ksheet_set_cell(&sheet, 200 + index, index % 40, "2"), 0);
    }
    expect_text("sparse hash retained",
                ksheet_cell_input(&sheet, 499, 19), "1");

    ksheet_format_cell(&sheet, 0, 1, formatted, sizeof(formatted));
    expect_text("formula display", formatted, "14");
    ksheet_format_cell(&sheet, 0, 5, formatted, sizeof(formatted));
    expect_text("text display", formatted, "hello");
    ksheet_cell_name(99, 25, name, sizeof(name));
    expect_text("cell name", name, "Z100");
    ksheet_cell_name(100, 26, name, sizeof(name));
    expect_text("multi-letter cell name", name, "AA101");
    ksheet_cell_name(KSHEET_MAX_ROWS - 1, KSHEET_MAX_COLS - 1,
                     name, sizeof(name));
    expect_text("maximum cell name", name, "FAN65536");

    ksheet_clear(&sheet);
    ksheet_set_cell(&sheet, 0, 0, "plain");
    ksheet_set_cell(&sheet, 0, 1, "two,parts");
    ksheet_set_cell(&sheet, 0, 2, "say \"hi\"");
    ksheet_set_cell(&sheet, 1, 0, "=1+2");
    expect_int("CSV export",
               ksheet_export_csv(&sheet, output, sizeof(output),
                                 &output_length), 0);
    expect_text("CSV bytes", output,
                "plain,\"two,parts\",\"say \"\"hi\"\"\"\n=1+2\n");
    expect_int("CSV length", (int)output_length, (int)strlen(output));
    expect_int("CSV import",
               ksheet_import_csv(&imported, output, output_length), 0);
    expect_text("roundtrip plain", ksheet_cell_input(&imported, 0, 0),
                "plain");
    expect_text("roundtrip comma", ksheet_cell_input(&imported, 0, 1),
                "two,parts");
    expect_text("roundtrip quote", ksheet_cell_input(&imported, 0, 2),
                "say \"hi\"");
    expect_number("roundtrip formula",
                  evaluate(&imported, 1, 0, KSHEET_OK,
                           "roundtrip formula error"), 3);

    ksheet_destroy(&sheet);
    ksheet_destroy(&imported);

    if (failures != 0) {
        fprintf(stderr, "FAIL KSheet model: %d failure(s)\n", failures);
        return 1;
    }
    puts("PASS KSheet model and formulas");
    return 0;
}

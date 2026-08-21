#include <stdio.h>
#include <string.h>

#include "extras/editor/c_syntax.h"

static int span_is(const char *source, const unsigned char *classes,
                   const char *needle, int occurrence, int value)
{
    const char *found = source;

    while (occurrence-- >= 0) {
        found = strstr(found, needle);
        if (found == NULL) return 0;
        if (occurrence >= 0) found++;
    }
    for (int i = 0; needle[i] != '\0'; i++)
        if (classes[found - source + i] != value) return 0;
    return 1;
}

int main(void)
{
    static const char source[] =
        "#include <stdint.h>\n"
        "static uint32_t value = 0x2aU; // counter\n"
        "const char *text = \"return \\\"ok\\\"\";\n"
        "/* while hidden */\n"
        "int sum = 1+2;\n"
        "int returning(void) { return value; }\n";
    unsigned char classes[sizeof(source) - 1];
    int failures = 0;

    c_syntax_highlight(source, (int)strlen(source), classes);
    failures += !span_is(source, classes, "#include <stdint.h>", 0,
                         C_SYNTAX_PREPROCESSOR);
    failures += !span_is(source, classes, "static", 0, C_SYNTAX_KEYWORD);
    failures += !span_is(source, classes, "uint32_t", 0, C_SYNTAX_TYPE);
    failures += !span_is(source, classes, "0x2aU", 0, C_SYNTAX_NUMBER);
    failures += !span_is(source, classes, "// counter", 0,
                         C_SYNTAX_COMMENT);
    failures += !span_is(source, classes, "\"return \\\"ok\\\"\"", 0,
                         C_SYNTAX_STRING);
    failures += !span_is(source, classes, "/* while hidden */", 0,
                         C_SYNTAX_COMMENT);
    failures += !span_is(source, classes, "1", 0, C_SYNTAX_NUMBER);
    failures += !span_is(source, classes, "+", 0, C_SYNTAX_TEXT);
    failures += !span_is(source, classes, "2", 2, C_SYNTAX_NUMBER);
    failures += !span_is(source, classes, "returning", 0, C_SYNTAX_TEXT);
    failures += !span_is(source, classes, "return", 2, C_SYNTAX_KEYWORD);
    if (failures != 0) {
        fprintf(stderr, "C syntax highlighting failures: %d\n", failures);
        return 1;
    }
    puts("C syntax highlighting tests passed");
    return 0;
}

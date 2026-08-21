#include "extras/editor/c_syntax.h"

static int identifier_start(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static int identifier_part(char ch)
{
    return identifier_start(ch) || (ch >= '0' && ch <= '9');
}

static int span_equals(const char *text, int start, int end,
                       const char *word)
{
    int index = 0;

    while (start + index < end && word[index] != '\0') {
        if (text[start + index] != word[index]) return 0;
        index++;
    }
    return start + index == end && word[index] == '\0';
}

static int word_in(const char *text, int start, int end,
                   const char *const *words, int count)
{
    for (int word = 0; word < count; word++)
        if (span_equals(text, start, end, words[word])) return 1;
    return 0;
}

static unsigned char identifier_class(const char *text, int start, int end)
{
    static const char *const keywords[] = {
        "auto", "break", "case", "const", "continue", "default",
        "do", "else", "enum", "extern", "for", "goto", "if",
        "inline", "register", "restrict", "return", "sizeof", "static",
        "struct", "switch", "typedef", "union", "volatile", "while",
        "_Alignas", "_Alignof", "_Atomic", "_Generic", "_Noreturn",
        "_Static_assert", "_Thread_local"
    };
    static const char *const types[] = {
        "char", "double", "float", "int", "long", "short", "signed",
        "unsigned", "void", "_Bool", "_Complex", "size_t", "ssize_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t",
        "int16_t", "int32_t", "int64_t", "FILE", "NULL"
    };

    if (word_in(text, start, end, keywords,
                (int)(sizeof(keywords) / sizeof(keywords[0]))))
        return C_SYNTAX_KEYWORD;
    if (word_in(text, start, end, types,
                (int)(sizeof(types) / sizeof(types[0]))))
        return C_SYNTAX_TYPE;
    return C_SYNTAX_TEXT;
}

static void paint(unsigned char *classes, int start, int end,
                  unsigned char value)
{
    for (int position = start; position < end; position++)
        classes[position] = value;
}

void c_syntax_highlight(const char *text, int length,
                        unsigned char *classes)
{
    int position = 0;
    int beginning_of_line = 1;

    if (text == 0 || classes == 0 || length <= 0) return;
    paint(classes, 0, length, C_SYNTAX_TEXT);
    while (position < length) {
        int start = position;
        char ch = text[position];

        if (ch == '\n') {
            beginning_of_line = 1;
            position++;
            continue;
        }
        if (beginning_of_line && (ch == ' ' || ch == '\t' || ch == '\r')) {
            position++;
            continue;
        }
        if (beginning_of_line && ch == '#') {
            while (position < length && text[position] != '\n') position++;
            paint(classes, start, position, C_SYNTAX_PREPROCESSOR);
            beginning_of_line = 0;
            continue;
        }
        beginning_of_line = 0;
        if (ch == '/' && position + 1 < length &&
            text[position + 1] == '/') {
            position += 2;
            while (position < length && text[position] != '\n') position++;
            paint(classes, start, position, C_SYNTAX_COMMENT);
            continue;
        }
        if (ch == '/' && position + 1 < length &&
            text[position + 1] == '*') {
            position += 2;
            while (position < length) {
                if (text[position] == '*' && position + 1 < length &&
                    text[position + 1] == '/') {
                    position += 2;
                    break;
                }
                position++;
            }
            paint(classes, start, position, C_SYNTAX_COMMENT);
            continue;
        }
        if (ch == '"' || ch == '\'') {
            char quote = ch;

            position++;
            while (position < length) {
                if (text[position] == '\\' && position + 1 < length) {
                    position += 2;
                    continue;
                }
                if (text[position++] == quote) break;
            }
            paint(classes, start, position, C_SYNTAX_STRING);
            continue;
        }
        if ((ch >= '0' && ch <= '9') ||
            (ch == '.' && position + 1 < length &&
             text[position + 1] >= '0' && text[position + 1] <= '9')) {
            position++;
            while (position < length) {
                ch = text[position];
                if (identifier_part(ch) || ch == '.') {
                    position++;
                } else if ((ch == '+' || ch == '-') && position > start &&
                           (text[position - 1] == 'e' ||
                            text[position - 1] == 'E' ||
                            text[position - 1] == 'p' ||
                            text[position - 1] == 'P')) {
                    position++;
                } else {
                    break;
                }
            }
            paint(classes, start, position, C_SYNTAX_NUMBER);
            continue;
        }
        if (identifier_start(ch)) {
            unsigned char value;

            position++;
            while (position < length && identifier_part(text[position]))
                position++;
            value = identifier_class(text, start, position);
            paint(classes, start, position, value);
            continue;
        }
        position++;
    }
}

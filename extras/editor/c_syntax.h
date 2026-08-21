#ifndef KYLEOS_C_SYNTAX_H
#define KYLEOS_C_SYNTAX_H

enum c_syntax_class {
    C_SYNTAX_TEXT,
    C_SYNTAX_KEYWORD,
    C_SYNTAX_TYPE,
    C_SYNTAX_NUMBER,
    C_SYNTAX_STRING,
    C_SYNTAX_COMMENT,
    C_SYNTAX_PREPROCESSOR
};

void c_syntax_highlight(const char *text, int length,
                        unsigned char *classes);

#endif

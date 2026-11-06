/*
 * ed.c - small 1970s-style line editor for KyleOS/Newlib
 *
 * Supported commands:
 *   [addr]p        print line
 *   [a,b]p         print range
 *   [a,b]n         print range with line numbers
 *   [addr]a        append after line; '.' alone ends input
 *   [addr]i        insert before line; '.' alone ends input
 *   [a,b]c         replace range; '.' alone ends input
 *   [a,b]d         delete range
 *   [a,b]s/x/y/    replace first x with y on each addressed line
 *   [addr]=        print addressed line number
 *   w [file]       write whole buffer
 *   f [file]       show/set current filename
 *   q              quit (refuses if modified)
 *   Q              quit without saving
 *
 * Addresses:
 *   number         1-based line number
 *   .              current line
 *   $              last line
 *   a,b            inclusive range
 *
 * This intentionally does not implement regexes.  s/old/new/ is a literal
 * substring replacement, which keeps the editor small and useful on KyleOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_CAP 64

static char **lines;
static size_t line_count;
static size_t line_cap;
static long current = -1;              /* zero-based, -1 when buffer is empty */
static int dirty;
static char filename[256];

static void error_msg(const char *msg)
{
    if (msg && *msg)
        printf("? %s\n", msg);
    else
        printf("?\n");
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);

    if (!p)
        return NULL;

    memcpy(p, s, n);
    return p;
}

/* Read one arbitrarily-sized line, without the trailing newline. */
static char *read_line(FILE *fp)
{
    size_t cap = 128;
    size_t len = 0;
    char *buf = malloc(cap);
    int ch;

    if (!buf)
        return NULL;

    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n')
            break;

        if (len + 1 >= cap) {
            size_t new_cap = cap * 2;
            char *new_buf = realloc(buf, new_cap);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
            cap = new_cap;
        }

        buf[len++] = (char)ch;
    }

    if (ch == EOF && len == 0) {
        free(buf);
        return NULL;
    }

    if (len && buf[len - 1] == '\r')
        len--;

    buf[len] = '\0';
    return buf;
}

static int ensure_capacity(size_t need)
{
    char **new_lines;
    size_t new_cap;

    if (need <= line_cap)
        return 0;

    new_cap = line_cap ? line_cap : INITIAL_CAP;
    while (new_cap < need)
        new_cap *= 2;

    new_lines = realloc(lines, new_cap * sizeof(*lines));
    if (!new_lines)
        return -1;

    lines = new_lines;
    line_cap = new_cap;
    return 0;
}

/* Insert an owned string at zero-based index pos. */
static int insert_owned(size_t pos, char *text)
{
    if (pos > line_count || ensure_capacity(line_count + 1) < 0)
        return -1;

    if (pos < line_count) {
        memmove(&lines[pos + 1], &lines[pos],
                (line_count - pos) * sizeof(*lines));
    }

    lines[pos] = text;
    line_count++;
    current = (long)pos;
    dirty = 1;
    return 0;
}

static void delete_range(size_t first, size_t last)
{
    size_t i;
    size_t count = last - first + 1;

    for (i = first; i <= last; i++)
        free(lines[i]);

    if (last + 1 < line_count) {
        memmove(&lines[first], &lines[last + 1],
                (line_count - last - 1) * sizeof(*lines));
    }

    line_count -= count;

    if (line_count == 0)
        current = -1;
    else if (first < line_count)
        current = (long)first;
    else
        current = (long)line_count - 1;

    dirty = 1;
}

static void print_range(size_t first, size_t last, int numbered)
{
    size_t i;

    for (i = first; i <= last; i++) {
        if (numbered)
            printf("%lu\t%s\n", (unsigned long)(i + 1), lines[i]);
        else
            printf("%s\n", lines[i]);
    }

    current = (long)last;
}

/* Read lines from stdin until a line containing only '.' is entered. */
static int input_lines(size_t pos)
{
    char *s;

    for (;;) {
        s = read_line(stdin);
        if (!s)
            return 0;

        if (strcmp(s, ".") == 0) {
            free(s);
            return 0;
        }

        if (insert_owned(pos, s) < 0) {
            free(s);
            error_msg("out of memory");
            return -1;
        }

        pos++;
    }
}

static int load_file(const char *name)
{
    FILE *fp;
    char *s;

    fp = fopen(name, "r");
    if (!fp) {
        /* ed traditionally allows starting a new, nonexistent file. */
        printf("%s: new file\n", name);
        return 0;
    }

    while ((s = read_line(fp)) != NULL) {
        if (ensure_capacity(line_count + 1) < 0) {
            free(s);
            fclose(fp);
            error_msg("out of memory");
            return -1;
        }

        lines[line_count++] = s;
    }

    fclose(fp);

    if (line_count)
        current = (long)line_count - 1;

    printf("%lu\n", (unsigned long)line_count);
    dirty = 0;
    return 0;
}

static int write_file(const char *name)
{
    FILE *fp;
    size_t i;
    unsigned long bytes = 0;

    if (!name || !*name) {
        error_msg("no current filename");
        return -1;
    }

    fp = fopen(name, "w");
    if (!fp) {
        error_msg("cannot open file for writing");
        return -1;
    }

    for (i = 0; i < line_count; i++) {
        size_t n = strlen(lines[i]);

        if (fwrite(lines[i], 1, n, fp) != n || fputc('\n', fp) == EOF) {
            fclose(fp);
            error_msg("write failed");
            return -1;
        }

        bytes += (unsigned long)n + 1;
    }

    if (fclose(fp) != 0) {
        error_msg("write failed");
        return -1;
    }

    printf("%lu\n", bytes);
    dirty = 0;
    return 0;
}

static void skip_spaces(const char **p)
{
    while (**p == ' ' || **p == '\t')
        (*p)++;
}

/* Parse one ed address and return it as a 1-based line number. */
static int parse_address(const char **p, long *out)
{
    const char *s = *p;
    long value = 0;

    skip_spaces(&s);

    if (*s == '.') {
        if (current < 0)
            return 0;
        value = current + 1;
        s++;
    } else if (*s == '$') {
        if (line_count == 0)
            return 0;
        value = (long)line_count;
        s++;
    } else if (isdigit((unsigned char)*s)) {
        while (isdigit((unsigned char)*s)) {
            value = value * 10 + (*s - '0');
            s++;
        }
    } else {
        return 0;
    }

    *p = s;
    *out = value;
    return 1;
}

/* Parse optional addr or addr,addr. Values returned are 1-based. */
static int parse_range(const char **p, long *first, long *last)
{
    const char *s = *p;
    long a, b;

    if (!parse_address(&s, &a))
        return 0;

    skip_spaces(&s);

    if (*s == ',') {
        s++;
        if (!parse_address(&s, &b))
            return -1;
    } else {
        b = a;
    }

    *p = s;
    *first = a;
    *last = b;
    return 1;
}

static int validate_range(long first, long last)
{
    if (first < 1 || last < first || last > (long)line_count) {
        error_msg("bad address");
        return -1;
    }
    return 0;
}

/* Literal first-occurrence replacement. */
static int substitute_line(size_t index, const char *old, const char *new_text)
{
    char *at;
    char *replacement;
    size_t prefix, old_len, new_len, suffix, total;

    old_len = strlen(old);
    new_len = strlen(new_text);

    if (old_len == 0)
        return 0;

    at = strstr(lines[index], old);
    if (!at)
        return 0;

    prefix = (size_t)(at - lines[index]);
    suffix = strlen(at + old_len);
    total = prefix + new_len + suffix + 1;

    replacement = malloc(total);
    if (!replacement)
        return -1;

    memcpy(replacement, lines[index], prefix);
    memcpy(replacement + prefix, new_text, new_len);
    memcpy(replacement + prefix + new_len, at + old_len, suffix + 1);

    free(lines[index]);
    lines[index] = replacement;
    return 1;
}

/* Parse s<delim>old<delim>new<delim>. No regexes; backslash escapes delimiter. */
static int parse_sub_part(const char **p, char delim, char *out, size_t out_size)
{
    size_t n = 0;
    const char *s = *p;

    while (*s && *s != delim) {
        char ch = *s++;

        if (ch == '\\' && *s) {
            if (*s == delim || *s == '\\')
                ch = *s++;
        }

        if (n + 1 >= out_size)
            return -1;

        out[n++] = ch;
    }

    if (*s != delim)
        return -1;

    out[n] = '\0';
    *p = s + 1;
    return 0;
}

static int do_substitute(long first, long last, const char *args)
{
    char delim;
    char old[512];
    char new_text[512];
    size_t i;
    int changed = 0;

    if (!*args) {
        error_msg("bad substitute");
        return -1;
    }

    delim = *args++;

    if (parse_sub_part(&args, delim, old, sizeof(old)) < 0 ||
        parse_sub_part(&args, delim, new_text, sizeof(new_text)) < 0) {
        error_msg("bad substitute");
        return -1;
    }

    for (i = (size_t)(first - 1); i <= (size_t)(last - 1); i++) {
        int r = substitute_line(i, old, new_text);
        if (r < 0) {
            error_msg("out of memory");
            return -1;
        }
        if (r > 0)
            changed = 1;
    }

    if (!changed) {
        error_msg("no match");
        return -1;
    }

    current = last - 1;
    dirty = 1;
    return 0;
}

static void free_buffer(void)
{
    size_t i;

    for (i = 0; i < line_count; i++)
        free(lines[i]);

    free(lines);
}

static void usage(const char *prog)
{
    printf("usage: %s [file]\n", prog);
}

int main(int argc, char **argv)
{
    char *cmd;
    int quit_warning = 0;

    if (argc > 2) {
        usage(argv[0]);
        return 1;
    }

    filename[0] = '\0';

    if (argc == 2) {
        strncpy(filename, argv[1], sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';

        if (load_file(filename) < 0) {
            free_buffer();
            return 1;
        }
    }

    for (;;) {
        const char *p;
        long first = 0, last = 0;
        int range_status;
        int had_range;
        char op;

        /* A visible prompt is convenient on KyleOS; classic ed is usually silent. */
        printf("* ");
        fflush(stdout);

        cmd = read_line(stdin);
        if (!cmd)
            break;

        p = cmd;
        skip_spaces(&p);

        if (*p == '\0') {
            free(cmd);
            continue;
        }

        range_status = parse_range(&p, &first, &last);
        if (range_status < 0) {
            error_msg("bad address");
            free(cmd);
            continue;
        }
        had_range = (range_status > 0);

        skip_spaces(&p);
        op = *p;

        /* A bare address means print that line. */
        if (had_range && op == '\0') {
            if (validate_range(first, last) == 0)
                print_range((size_t)(first - 1), (size_t)(last - 1), 0);
            free(cmd);
            quit_warning = 0;
            continue;
        }

        if (op)
            p++;
        skip_spaces(&p);

        switch (op) {
        case 'p':
        case 'n':
            if (!had_range) {
                if (current < 0) {
                    error_msg("empty buffer");
                    break;
                }
                first = last = current + 1;
            }
            if (validate_range(first, last) == 0)
                print_range((size_t)(first - 1), (size_t)(last - 1), op == 'n');
            break;

        case 'a': {
            size_t pos;

            if (had_range && first != last) {
                error_msg("bad address");
                break;
            }

            if (had_range) {
                if (first < 0 || first > (long)line_count) {
                    error_msg("bad address");
                    break;
                }
                pos = (size_t)first;       /* append after 1-based line */
            } else {
                pos = current < 0 ? 0 : (size_t)current + 1;
            }

            input_lines(pos);
            break;
        }

        case 'i': {
            size_t pos;

            if (had_range && first != last) {
                error_msg("bad address");
                break;
            }

            if (line_count == 0) {
                pos = 0;
            } else if (had_range) {
                if (first < 1 || first > (long)line_count) {
                    error_msg("bad address");
                    break;
                }
                pos = (size_t)(first - 1);
            } else {
                pos = current < 0 ? 0 : (size_t)current;
            }

            input_lines(pos);
            break;
        }

        case 'c': {
            size_t pos;

            if (!had_range) {
                if (current < 0) {
                    error_msg("empty buffer");
                    break;
                }
                first = last = current + 1;
            }

            if (validate_range(first, last) < 0)
                break;

            pos = (size_t)(first - 1);
            delete_range(pos, (size_t)(last - 1));
            input_lines(pos);
            break;
        }

        case 'd':
            if (!had_range) {
                if (current < 0) {
                    error_msg("empty buffer");
                    break;
                }
                first = last = current + 1;
            }

            if (validate_range(first, last) == 0)
                delete_range((size_t)(first - 1), (size_t)(last - 1));
            break;

        case 's':
            if (!had_range) {
                if (current < 0) {
                    error_msg("empty buffer");
                    break;
                }
                first = last = current + 1;
            }

            if (validate_range(first, last) == 0)
                do_substitute(first, last, p);
            break;

        case '=':
            if (!had_range) {
                if (current < 0) {
                    printf("0\n");
                } else {
                    printf("%ld\n", current + 1);
                }
            } else if (first == last && first >= 0 && first <= (long)line_count) {
                printf("%ld\n", first);
            } else {
                error_msg("bad address");
            }
            break;

        case 'w': {
            const char *name = p;

            if (had_range) {
                error_msg("addressed write not supported");
                break;
            }

            if (*name) {
                strncpy(filename, name, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';
            }

            write_file(filename);
            break;
        }

        case 'f':
            if (had_range) {
                error_msg("bad address");
                break;
            }

            if (*p) {
                strncpy(filename, p, sizeof(filename) - 1);
                filename[sizeof(filename) - 1] = '\0';
            }

            if (*filename)
                printf("%s\n", filename);
            else
                error_msg("no current filename");
            break;

        case 'q':
            if (had_range) {
                error_msg("bad address");
                break;
            }

            if (dirty && !quit_warning) {
                error_msg("modified buffer; use q again or Q to quit");
                quit_warning = 1;
                free(cmd);
                continue;
            }

            free(cmd);
            free_buffer();
            return 0;

        case 'Q':
            free(cmd);
            free_buffer();
            return 0;

        default:
            error_msg("unknown command");
            break;
        }

        if (op != 'q')
            quit_warning = 0;

        free(cmd);
    }

    free_buffer();
    return 0;
}

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* KyleOS does not yet expose getcwd/realpath.  TCC only uses these for
 * diagnostics and include-cache keys, so preserving the supplied path is
 * sufficient until the VFS grows canonical-path support. */
char *getcwd(char *buffer, size_t size)
{
    if (buffer == NULL || size < 2) {
        errno = ERANGE;
        return NULL;
    }
    buffer[0] = '.';
    buffer[1] = '\0';
    return buffer;
}

char *realpath(const char *path, char *resolved)
{
    size_t length;

    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    length = strlen(path) + 1;
    if (resolved == NULL) {
        resolved = malloc(length);
        if (resolved == NULL)
            return NULL;
    }
    memcpy(resolved, path, length);
    return resolved;
}

int execvp(const char *file, char *const argv[])
{
    (void)file;
    (void)argv;
    errno = ENOSYS;
    return -1;
}

#ifndef _PTY_H_
#define _PTY_H_

#ifdef __cplusplus
extern "C" {
#endif

struct termios;
struct winsize;

int openpty(int *master, int *slave, char *name,
            const struct termios *termios_pointer,
            const struct winsize *window_size);

#ifdef __cplusplus
}
#endif

#endif

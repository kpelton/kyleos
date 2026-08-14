/* KyleOS system backend for the original Quake engine. */
#include "quakedef.h"
#include "kyleos_syscalls.h"

#include <errno.h>

qboolean isDedicated;
int nostdout;

#define MAX_FILE_HANDLES 64
static FILE *open_files[MAX_FILE_HANDLES];
static int raw_mode_enabled;

static int allocate_file_handle(FILE *file)
{
    int handle;

    for (handle = 1; handle < MAX_FILE_HANDLES; handle++) {
        if (open_files[handle] == NULL) {
            open_files[handle] = file;
            return handle;
        }
    }
    fclose(file);
    Sys_Error("Quake exhausted its file handles");
    return -1;
}

static FILE *file_from_handle(int handle)
{
    if (handle <= 0 || handle >= MAX_FILE_HANDLES ||
        open_files[handle] == NULL)
        Sys_Error("Invalid Quake file handle %d", handle);
    return open_files[handle];
}

static void restore_console(void)
{
    if (raw_mode_enabled) {
        kyleos_tty_raw(0);
        raw_mode_enabled = 0;
    }
}

int Sys_FileOpenRead(char *path, int *handle)
{
    FILE *file = fopen(path, "rb");
    long length;

    if (file == NULL) {
        *handle = -1;
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        *handle = -1;
        return -1;
    }
    *handle = allocate_file_handle(file);
    return (int)length;
}

int Sys_FileOpenWrite(char *path)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL)
        Sys_Error("Unable to open %s for writing", path);
    return allocate_file_handle(file);
}

void Sys_FileClose(int handle)
{
    fclose(file_from_handle(handle));
    open_files[handle] = NULL;
}

void Sys_FileSeek(int handle, int position)
{
    fseek(file_from_handle(handle), position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
    return fread(dest, 1, count, file_from_handle(handle));
}

int Sys_FileWrite(int handle, void *data, int count)
{
    return fwrite(data, 1, count, file_from_handle(handle));
}

int Sys_FileTime(char *path)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return -1;
    fclose(file);
    return 1;
}

void Sys_mkdir(char *path)
{
    (void)path;
}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
    (void)startaddr;
    (void)length;
}

void Sys_Error(char *error, ...)
{
    va_list args;

    restore_console();
    fprintf(stderr, "Quake error: ");
    va_start(args, error);
    vfprintf(stderr, error, args);
    va_end(args);
    fprintf(stderr, "\n");
    Host_Shutdown();
    exit(1);
}

void Sys_Printf(char *format, ...)
{
    va_list args;

    if (nostdout)
        return;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void Sys_DebugLog(char *file, char *format, ...)
{
    va_list args;
    FILE *output = fopen(file, "ab");

    if (output == NULL)
        return;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    fclose(output);
}

void Sys_Quit(void)
{
    Host_Shutdown();
    restore_console();
    exit(0);
}

double Sys_FloatTime(void)
{
    static uint32_t base;
    uint32_t now = kyleos_ticks_ms();

    if (base == 0)
        base = now;
    return (double)(uint32_t)(now - base) / 1000.0;
}

char *Sys_ConsoleInput(void)
{
    return NULL;
}

void Sys_Sleep(void)
{
    kyleos_sleep_ms(1);
}

void Sys_LowFPPrecision(void)
{
}

void Sys_HighFPPrecision(void)
{
}

void Sys_Init(void)
{
}

int main(int argc, char **argv)
{
    double oldtime;
    quakeparms_t parms;
    int argument;

    memset(&parms, 0, sizeof(parms));
    COM_InitArgv(argc, argv);
    parms.argc = com_argc;
    parms.argv = com_argv;
    parms.memsize = 8 * 1024 * 1024;

    argument = COM_CheckParm("-mem");
    if (argument && argument + 1 < com_argc)
        parms.memsize = (int)(Q_atof(com_argv[argument + 1]) * 1024 * 1024);
    parms.membase = malloc(parms.memsize);
    if (parms.membase == NULL)
        Sys_Error("Unable to allocate %d bytes for Quake", parms.memsize);
    parms.basedir = "/usr/share/quake";

    atexit(restore_console);
    raw_mode_enabled = 1;

    Host_Init(&parms);
    Sys_Init();
    oldtime = Sys_FloatTime();

    for (;;) {
        double newtime = Sys_FloatTime();
        double frametime = newtime - oldtime;

        if (frametime <= 0) {
            kyleos_sleep_ms(1);
            continue;
        }
        oldtime = newtime;
        Host_Frame(frametime);
    }
}

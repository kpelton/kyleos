/* Aqua KUI system backend for the original Quake engine. */
#include "quakedef.h"
#include "quake_aqua.h"

#include "extras/kui/kui_client.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_FILE_HANDLES 64
#define QUAKE_AQUA_FRAME_MS 33U
#define QUAKE_AQUA_STARTUP_MS 5000U

qboolean isDedicated;
int nostdout;

static FILE *open_files[MAX_FILE_HANDLES];
static int protocol_output = 1;
static int aqua_host_ready;

static int separate_protocol_output(void)
{
    int reserved[2];
    int log_fd;

    if (pipe(reserved) < 0) return -1;
    protocol_output = reserved[0];
    close(reserved[1]);
    if (dup2(1, protocol_output) < 0) {
        close(protocol_output);
        protocol_output = 1;
        return -1;
    }
    log_fd = open("/tmp/quake-aqua.log",
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) log_fd = open("/dev/null", O_WRONLY);
    if (log_fd < 0 || dup2(log_fd, 1) < 0 || dup2(log_fd, 2) < 0) {
        if (log_fd >= 0) close(log_fd);
        close(protocol_output);
        protocol_output = 1;
        return -1;
    }
    if (log_fd != 1 && log_fd != 2) close(log_fd);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    return 0;
}

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

int Sys_FileOpenRead(char *path, int *handle)
{
    FILE *file = fopen(path, "rb");
    long length;

    if (file == NULL) { *handle = -1; return -1; }
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

    if (file == NULL) Sys_Error("Unable to open %s for writing", path);
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

    if (file == NULL) return -1;
    fclose(file);
    return 1;
}

void Sys_mkdir(char *path) { (void)path; }
void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{ (void)startaddr; (void)length; }

void Sys_Error(char *error, ...)
{
    va_list args;

    fprintf(stderr, "Quake error: ");
    va_start(args, error);
    vfprintf(stderr, error, args);
    va_end(args);
    fprintf(stderr, "\n");
    if (aqua_host_ready) Host_Shutdown();
    exit(1);
}

void Sys_Printf(char *format, ...)
{
    va_list args;

    if (nostdout) return;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void Sys_DebugLog(char *file, char *format, ...)
{
    va_list args;
    FILE *output = fopen(file, "ab");

    if (output == NULL) return;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    fclose(output);
}

void Sys_Quit(void)
{
    if (aqua_host_ready) Host_Shutdown();
    exit(0);
}

double Sys_FloatTime(void)
{
    static uint32_t base;
    uint32_t now = kui_ticks_ms();

    if (base == 0) base = now;
    return (double)(uint32_t)(now - base) / 1000.0;
}

char *Sys_ConsoleInput(void) { return NULL; }
void Sys_Sleep(void) { kui_sleep_ms(1); }
void Sys_LowFPPrecision(void) { }
void Sys_HighFPPrecision(void) { }
void Sys_Init(void) { }

static int send_loading(struct kui_client *client,
                        const struct kui_event *event)
{
    const char *message = "Loading Quake...";
    int x = (event->width - kui_text_width(message)) / 2;
    int y = event->height / 2 - 8;

    kui_client_begin(client, event, "Quake");
    kui_client_set_constraints(client, 320, 200, 640, 400);
    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_BLACK);
    kui_draw_text(&client->surface, x, y, message,
                  KUI_WHITE, KUI_BLACK, 0);
    kui_client_set_timer(client, QUAKE_AQUA_STARTUP_MS);
    return kui_client_send_fd(client, protocol_output);
}

int main(int argc, char **argv)
{
    static char *default_argv[] = { "quake-aqua", NULL };
    struct kui_client client;
    struct kui_event event;
    quakeparms_t parms;
    double oldtime;
    int argument;
    int first_frame = 1;

    if (separate_protocol_output() < 0) return 1;
    kui_client_init(&client);
    if (kui_client_read_event(&event) < 0 || event.type != KUI_EVENT_INIT ||
        send_loading(&client, &event) < 0) {
        kui_client_free(&client);
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "--kui-client") == 0) {
        argc = 1;
        argv = default_argv;
    }
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
    Host_Init(&parms);
    aqua_host_ready = 1;
    Sys_Init();
    oldtime = Sys_FloatTime();
    puts("quake-aqua: engine initialization complete");

    while (kui_client_read_event(&event) == 0) {
        int redraw = event.type == KUI_EVENT_RESIZE ||
                     event.type == KUI_EVENT_REFRESH;

        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_KEY)
            QuakeAqua_QueueKey(event.key_pressed, event.key_code,
                               event.key_extended);
        if (event.type == KUI_EVENT_POINTER)
            QuakeAqua_QueuePointer(event.x, event.y,
                                   event.buttons, event.wheel);
        if (event.type == KUI_EVENT_TIMER) {
            double newtime = Sys_FloatTime();
            double frametime = first_frame ? 1.0 / 30.0 : newtime - oldtime;

            oldtime = newtime;
            if (frametime <= 0) frametime = 1.0 / 1000.0;
            if (frametime > 0.1) frametime = 0.1;
            Host_Frame(frametime);
            redraw |= QuakeAqua_TakeFrameReady();
        }
        kui_client_begin(&client, &event, "Quake");
        kui_client_set_constraints(&client, 320, 200, 640, 400);
        if (redraw)
            QuakeAqua_DrawFrame(&client.surface);
        else
            kui_client_set_no_display(&client);
        /* Preserve the compositor's existing frame deadline while handling
         * key or pointer input.  Re-arming here would postpone the next tick
         * on every hardware key-repeat event, making held movement keys
         * visibly stutter. */
        if (event.type == KUI_EVENT_TIMER)
            kui_client_set_timer(&client, QUAKE_AQUA_FRAME_MS);
        if (first_frame)
            fprintf(stdout, "quake-aqua: first frame bytes=%u\n",
                    client.display.length);
        if (kui_client_send_fd(&client, protocol_output) < 0) {
            puts("quake-aqua: KUI response failed");
            break;
        }
        first_frame = 0;
    }
    puts("quake-aqua: event loop ended");
    if (aqua_host_ready) Host_Shutdown();
    kui_client_free(&client);
    close(protocol_output);
    return 0;
}

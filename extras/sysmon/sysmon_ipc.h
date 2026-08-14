#ifndef KYLEOS_SYSMON_IPC_H
#define KYLEOS_SYSMON_IPC_H

#define SYSMON_TEXT_MAX 8192

enum sysmon_command {
    SYSMON_COMMAND_REFRESH = 1,
    SYSMON_COMMAND_QUIT
};

struct sysmon_request {
    int command;
};

struct sysmon_response {
    int status;
    int text_length;
    char text[SYSMON_TEXT_MAX];
};

#endif

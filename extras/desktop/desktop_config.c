#include "extras/desktop/desktop_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (capacity == 0) return;
    length = strlen(source);
    if (length >= capacity) length = capacity - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static char *trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t') text++;
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
            end[-1] == '\n'))
        *--end = '\0';
    return text;
}

static int parse_boolean(const char *value, int *result)
{
    if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 ||
        strcmp(value, "1") == 0) {
        *result = 1;
        return 0;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0 ||
        strcmp(value, "0") == 0) {
        *result = 0;
        return 0;
    }
    return -1;
}

static void set_wallpaper(char *destination, const char *value)
{
    if (value[0] == '/' && strlen(value) < DESKTOP_CONFIG_PATH_MAX)
        copy_text(destination, DESKTOP_CONFIG_PATH_MAX, value);
}

void desktop_config_defaults(struct desktop_config *config)
{
    memset(config, 0, sizeof(*config));
    copy_text(config->aqua_wallpaper, sizeof(config->aqua_wallpaper),
              DESKTOP_WALLPAPER_AQUA);
    copy_text(config->win95_wallpaper, sizeof(config->win95_wallpaper),
              DESKTOP_WALLPAPER_WIN95);
    config->pointer_speed = 2;
    config->show_desktop_icons = 1;
}

int desktop_config_load(struct desktop_config *config, const char *path)
{
    char line[320];
    FILE *file;

    desktop_config_defaults(config);
    file = fopen(path, "r");
    if (file == NULL) return -1;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals;
        char *key = trim(line);
        char *value;

        if (*key == '\0' || *key == '#') continue;
        equals = strchr(key, '=');
        if (equals == NULL) continue;
        *equals = '\0';
        value = trim(equals + 1);
        key = trim(key);
        if (strcmp(key, "wallpaper_aqua") == 0)
            set_wallpaper(config->aqua_wallpaper, value);
        else if (strcmp(key, "wallpaper_win95") == 0)
            set_wallpaper(config->win95_wallpaper, value);
        else if (strcmp(key, "pointer_speed") == 0) {
            int speed = atoi(value);

            if (speed >= 1 && speed <= 4) config->pointer_speed = speed;
        } else if (strcmp(key, "show_desktop_icons") == 0) {
            int enabled;

            if (parse_boolean(value, &enabled) == 0)
                config->show_desktop_icons = enabled;
        }
    }
    fclose(file);
    return 0;
}

int desktop_config_save(const struct desktop_config *config,
                        const char *path)
{
    char temporary[DESKTOP_CONFIG_PATH_MAX + 8];
    char backup[DESKTOP_CONFIG_PATH_MAX + 8];
    const char *slash;
    size_t directory_length;
    FILE *file;
    FILE *existing;
    int had_existing;
    int close_result;
    int result;

    slash = strrchr(path, '/');
    directory_length = slash != NULL ? (size_t)(slash - path + 1) : 0;
    if (directory_length + 12 >= sizeof(temporary)) return -1;
    snprintf(temporary, sizeof(temporary), "%.*sdeskcfg.tmp",
             (int)directory_length, path);
    snprintf(backup, sizeof(backup), "%.*sdeskcfg.bak",
             (int)directory_length, path);
    file = fopen(temporary, "w");
    if (file == NULL) return -1;
    result = fprintf(file,
                     "# KyleOS desktop settings\n"
                     "wallpaper_aqua=%s\n"
                     "wallpaper_win95=%s\n"
                     "pointer_speed=%d\n"
                     "show_desktop_icons=%s\n",
                     config->aqua_wallpaper, config->win95_wallpaper,
                     config->pointer_speed,
                     config->show_desktop_icons ? "true" : "false");
    close_result = fclose(file);
    if (result < 0 || close_result != 0) {
        remove(temporary);
        return -1;
    }
    existing = fopen(path, "r");
    had_existing = existing != NULL;
    if (existing != NULL) fclose(existing);
    remove(backup);
    if (had_existing && rename(path, backup) != 0) {
        remove(temporary);
        return -1;
    }
    if (rename(temporary, path) != 0) {
        if (had_existing) rename(backup, path);
        remove(temporary);
        return -1;
    }
    if (had_existing) remove(backup);
    return 0;
}

const char *desktop_config_wallpaper(const struct desktop_config *config,
                                     int aqua)
{
    return aqua ? config->aqua_wallpaper : config->win95_wallpaper;
}

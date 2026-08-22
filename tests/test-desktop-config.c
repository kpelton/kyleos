#include <stdio.h>
#include <string.h>

#include "extras/desktop/desktop_config.h"

static int check(int condition, const char *message)
{
    if (!condition) fprintf(stderr, "FAIL desktop config: %s\n", message);
    return !condition;
}

int main(void)
{
    const char *path = "/tmp/os3-desktop-config.conf";
    struct desktop_config saved;
    struct desktop_config loaded;
    FILE *file;
    int failures = 0;

    remove(path);
    desktop_config_defaults(&saved);
    failures += check(saved.pointer_speed == 2 && saved.show_desktop_icons,
                      "incorrect defaults");
    saved.pointer_speed = 4;
    saved.show_desktop_icons = 0;
    strcpy(saved.aqua_wallpaper, "/wallpapers/test.kimg");
    failures += check(desktop_config_save(&saved, path) == 0,
                      "unable to save settings");
    failures += check(desktop_config_load(&loaded, path) == 0,
                      "unable to load saved settings");
    failures += check(loaded.pointer_speed == 4 &&
                      !loaded.show_desktop_icons &&
                      strcmp(loaded.aqua_wallpaper,
                             "/wallpapers/test.kimg") == 0,
                      "saved values did not round trip");

    file = fopen(path, "w");
    if (file == NULL) return 2;
    fputs("pointer_speed=99\n"
          "show_desktop_icons=maybe\n"
          "wallpaper_aqua=relative.kimg\n"
          "unknown_key=ignored\n", file);
    fclose(file);
    failures += check(desktop_config_load(&loaded, path) == 0,
                      "unable to load malformed settings");
    failures += check(loaded.pointer_speed == 2 &&
                      loaded.show_desktop_icons &&
                      strcmp(loaded.aqua_wallpaper,
                             DESKTOP_WALLPAPER_AQUA) == 0,
                      "invalid values did not retain defaults");
    remove(path);
    if (failures == 0) puts("Desktop config tests passed");
    return failures != 0;
}

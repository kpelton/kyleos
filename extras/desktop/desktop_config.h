#ifndef KYLEOS_DESKTOP_CONFIG_H
#define KYLEOS_DESKTOP_CONFIG_H

#ifndef DESKTOP_CONFIG_PATH
#define DESKTOP_CONFIG_PATH "/etc/desktop.cfg"
#endif
#define DESKTOP_CONFIG_PATH_MAX 128

#define DESKTOP_WALLPAPER_AQUA "/usr/share/gui/aqua-wallpaper.kimg"
#define DESKTOP_WALLPAPER_WIN95 "/usr/share/gui/wallpaper.kimg"

struct desktop_config {
    char aqua_wallpaper[DESKTOP_CONFIG_PATH_MAX];
    char win95_wallpaper[DESKTOP_CONFIG_PATH_MAX];
    int pointer_speed;
    int show_desktop_icons;
};

void desktop_config_defaults(struct desktop_config *config);
int desktop_config_load(struct desktop_config *config, const char *path);
int desktop_config_save(const struct desktop_config *config,
                        const char *path);
const char *desktop_config_wallpaper(const struct desktop_config *config,
                                     int aqua);

#endif

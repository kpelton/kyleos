#include <stdio.h>
#include <string.h>

#include "extras/kui/kui_style.h"
#include "extras/kuid/shell_protocol.h"

static int check(int condition, const char *message)
{
    if (!condition)
        fprintf(stderr, "FAIL kui style: %s\n", message);
    return !condition;
}

int main(void)
{
    struct kui_shell_profile classic;
    struct kui_shell_profile aqua;
    int failures = 0;

    memset(&classic, 0, sizeof(classic));
    memset(&aqua, 0, sizeof(aqua));
    classic.magic = aqua.magic = KUI_SHELL_MAGIC;
    classic.version = aqua.version = KUI_SHELL_VERSION;
    kui_style_win95(&classic.style);
    kui_style_aqua(&aqua.style);
    failures += check(classic.style.id != aqua.style.id,
                      "environment IDs must differ");
    failures += check(!(classic.style.flags & KUI_STYLE_AQUA),
                      "classic style marked Aqua");
    failures += check(aqua.style.flags & KUI_STYLE_AQUA,
                      "Aqua style flag missing");
    failures += check(classic.style.panel_top == 0 &&
                      classic.style.panel_bottom == 32,
                      "classic work area metrics");
    failures += check(aqua.style.panel_top == 24 &&
                      aqua.style.panel_bottom == 64,
                      "Aqua menu bar and Dock metrics");
    failures += check(classic.style.title_active != aqua.style.title_active &&
                      classic.style.face != aqua.style.face,
                      "styles should not share their principal palette");
    failures += check(sizeof(struct kui_shell_request) <= 4096,
                      "shell request must fit atomically in a pipe");
    kui_style_apply(&aqua.style);
    failures += check(kui_style_current()->id == aqua.style.id &&
                      kui_style_current()->window_border == 3,
                      "style propagation");
    if (failures == 0)
        puts("KUI style and shell profile tests passed");
    return failures != 0;
}

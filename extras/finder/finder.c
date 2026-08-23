#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "extras/finder/finder_ipc.h"
#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"
#include "extras/kui/kui_dialog.h"
#include "extras/kui/kui_menu.h"

#define FINDER_HISTORY_MAX 16
#define CONTEXT_MENU_WIDTH 176
#define CONTEXT_MENU_ROW_HEIGHT 24
#define CONTEXT_MENU_HEIGHT (CONTEXT_MENU_ROW_HEIGHT * 4 + 4)
#define FINDER_TOOLBAR_HEIGHT 60
#define FINDER_ITEM_WIDTH 88
#define FINDER_LABEL_WIDTH 72
#define FINDER_SEARCH_WIDTH 220
#define FINDER_SEARCH_MAX 63

#define AQUA_BACK_RIGHT 60
#define AQUA_FORWARD_RIGHT 120
#define AQUA_COMPUTER_RIGHT 188
#define AQUA_HOME_RIGHT 240
#define AQUA_APPLICATIONS_RIGHT 318
#define AQUA_REFRESH_RIGHT 380
#define AQUA_SEARCH_LEFT 388

enum finder_context_kind {
    FINDER_CONTEXT_NONE,
    FINDER_CONTEXT_ITEM,
    FINDER_CONTEXT_BACKGROUND
};

enum finder_create_kind {
    FINDER_CREATE_NONE,
    FINDER_CREATE_FOLDER,
    FINDER_CREATE_TEXT_FILE
};

enum finder_menu_action {
    FINDER_MENU_NEW_FOLDER = 1,
    FINDER_MENU_NEW_TEXT_FILE,
    FINDER_MENU_PASTE,
    FINDER_MENU_SORT_TYPE
};

struct finder_selection {
    char names[FINDER_MAX_ITEMS][FINDER_NAME_MAX];
    int count;
    char anchor[FINDER_NAME_MAX];
};

static struct kui_menu_item finder_file_items[] = {
    { "New Folder", NULL, FINDER_MENU_NEW_FOLDER, 1 },
    { "New Text File", NULL, FINDER_MENU_NEW_TEXT_FILE, 1 },
    { "Paste", NULL, FINDER_MENU_PASTE, 0 },
    { "Sort by Type", NULL, FINDER_MENU_SORT_TYPE, 1 }
};

static const struct kui_menu finder_menus[] = {
    { "File", finder_file_items, 4 }
};

static int finder_selection_contains(const struct finder_selection *selection,
                                     const char *name)
{
    for (int i = 0; i < selection->count; i++)
        if (strcmp(selection->names[i], name) == 0) return 1;
    return 0;
}

static void finder_selection_clear(struct finder_selection *selection)
{
    selection->count = 0;
    selection->anchor[0] = '\0';
}

static void finder_selection_add(struct finder_selection *selection,
                                 const char *name)
{
    if (finder_selection_contains(selection, name) ||
        selection->count >= FINDER_MAX_ITEMS)
        return;
    strcpy(selection->names[selection->count++], name);
}

static void finder_selection_remove(struct finder_selection *selection,
                                    const char *name)
{
    for (int i = 0; i < selection->count; i++) {
        if (strcmp(selection->names[i], name) != 0) continue;
        if (i + 1 < selection->count)
            memmove(selection->names[i], selection->names[i + 1],
                    (selection->count - i - 1) * FINDER_NAME_MAX);
        selection->count--;
        return;
    }
}

static void finder_selection_only(struct finder_selection *selection,
                                  const char *name)
{
    finder_selection_clear(selection);
    finder_selection_add(selection, name);
    strcpy(selection->anchor, name);
}

static int finder_item_named(const struct finder_response *state,
                             const char *name)
{
    for (int item = 0; item < state->item_count; item++)
        if (strcmp(state->item_names[item], name) == 0) return item;
    return -1;
}

static void finder_selection_range(struct finder_selection *selection,
                                   const struct finder_response *state,
                                   int item, int additive)
{
    int anchor = finder_item_named(state, selection->anchor);
    int first;
    int last;

    if (anchor < 0) anchor = item;
    first = anchor < item ? anchor : item;
    last = anchor > item ? anchor : item;
    if (!additive) finder_selection_clear(selection);
    for (int index = first; index <= last; index++)
        finder_selection_add(selection, state->item_names[index]);
    strcpy(selection->anchor, state->item_names[anchor]);
}

static void finder_selection_prune(struct finder_selection *selection,
                                   const struct finder_response *state)
{
    for (int i = selection->count - 1; i >= 0; i--)
        if (finder_item_named(state, selection->names[i]) < 0)
            finder_selection_remove(selection, selection->names[i]);
    if (selection->anchor[0] != '\0' &&
        finder_item_named(state, selection->anchor) < 0)
        selection->anchor[0] = '\0';
}

static int finder_selection_copyable(
    const struct finder_selection *selection,
    const struct finder_response *state)
{
    if (selection->count == 0) return 0;
    for (int selected = 0; selected < selection->count; selected++) {
        int item = finder_item_named(state, selection->names[selected]);

        if (item < 0) return 0;
    }
    return 1;
}

struct kyleos_dirent {
    uint64_t inode;
    char name[257];
    int type;
    uint64_t size;
};

static int get_directory_entries(int fd, struct kyleos_dirent *entries,
                                 int count)
{
    register long rax asm("rax") = 16;
    asm volatile("int $0x80" : "+a"(rax)
                 : "D"((long)fd), "S"(entries), "d"((long)count)
                 : "memory");
    return (int)rax;
}

static void sort_by_type(struct finder_response *state)
{
    for (int i = 0; i < state->item_count; i++) {
        for (int j = i + 1; j < state->item_count; j++) {
            int swap = state->item_types[j] < state->item_types[i];
            if (!swap && state->item_types[j] == state->item_types[i] &&
                strcmp(state->item_names[j], state->item_names[i]) < 0)
                swap = 1;
            if (swap) {
                int type = state->item_types[i];
                char name[FINDER_NAME_MAX];
                state->item_types[i] = state->item_types[j];
                state->item_types[j] = type;
                strcpy(name, state->item_names[i]);
                strcpy(state->item_names[i], state->item_names[j]);
                strcpy(state->item_names[j], name);
            }
        }
    }
}

static const char *path_name(const char *path)
{
    const char *name = strrchr(path, '/');

    if (path[0] == '/' && path[1] == '\0') return "System Disk";
    return name != NULL ? name + 1 : path;
}

static void copy_path(char destination[FINDER_PATH_MAX], const char *source)
{
    size_t length = strlen(source);
    if (length >= FINDER_PATH_MAX) length = FINDER_PATH_MAX - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static int child_path(char destination[FINDER_PATH_MAX], const char *parent,
                      const char *name)
{
    size_t parent_length = strlen(parent);
    size_t name_length = strlen(name);
    size_t separator = strcmp(parent, "/") == 0 ? 0 : 1;

    if (parent_length + separator + name_length >= FINDER_PATH_MAX)
        return -1;
    memcpy(destination, parent, parent_length);
    if (separator != 0) destination[parent_length++] = '/';
    memcpy(destination + parent_length, name, name_length);
    destination[parent_length + name_length] = '\0';
    return 0;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static int path_exists(const char *path)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static int remove_empty_directory(const char *path)
{
    register long result asm("rax") = 29;

    asm volatile("int $0x80" : "+a"(result) : "D"(path) : "memory");
    return (int)result;
}

static int copy_file_contents(const char *source, const char *destination)
{
    unsigned char buffer[4096];
    int input = -1;
    int output = -1;
    int result = -1;

    input = open(source, O_RDONLY);
    if (input < 0) goto done;
    output = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output < 0) goto done;
    for (;;) {
        int count = read(input, buffer, sizeof(buffer));
        int written = 0;

        if (count < 0) goto done;
        if (count == 0) break;
        while (written < count) {
            int amount = write(output, buffer + written, count - written);
            if (amount <= 0) goto done;
            written += amount;
        }
    }
    result = 0;
done:
    if (output >= 0) close(output);
    if (input >= 0) close(input);
    if (result < 0 && output >= 0) unlink(destination);
    return result;
}

static int is_short_filename(const char *name)
{
    int base = 0;
    int extension = 0;
    int saw_dot = 0;

    if (name == NULL || name[0] == '\0') return 0;
    for (; *name != '\0'; name++) {
        if (*name == '.') {
            if (saw_dot || base == 0) return 0;
            saw_dot = 1;
        } else if (!saw_dot) {
            if (++base > 8) return 0;
        } else if (++extension > 3) {
            return 0;
        }
    }
    return base > 0;
}

static int make_new_path(char destination[FINDER_PATH_MAX],
                         const char *directory, int folder)
{
    char name[16];

    for (int number = 0; number <= 9999; number++) {
        if (folder) {
            if (number == 0) strcpy(name, "newfold");
            else snprintf(name, sizeof(name), "fold%04d", number);
        } else {
            if (number == 0) strcpy(name, "new.txt");
            else snprintf(name, sizeof(name), "new%04d.txt", number);
        }
        if (child_path(destination, directory, name) == 0 &&
            !path_exists(destination))
            return 0;
    }
    return -1;
}

static int make_copy_path(char destination[FINDER_PATH_MAX],
                          const char *directory, const char *source)
{
    const char *source_name = base_name(source);
    const char *dot = strrchr(source_name, '.');
    char extension[5];
    char name[16];

    if (is_short_filename(source_name) &&
        child_path(destination, directory, source_name) == 0 &&
        strcmp(destination, source) != 0 && !path_exists(destination))
        return 0;
    extension[0] = '\0';
    if (dot != NULL && strlen(dot) <= 4)
        strcpy(extension, dot);
    for (int number = 1; number <= 9999; number++) {
        snprintf(name, sizeof(name), "copy%d%s", number, extension);
        if (child_path(destination, directory, name) == 0 &&
            !path_exists(destination))
            return 0;
    }
    return -1;
}

static int read_copy_directory(const char *path,
                               struct kyleos_dirent **entries_out,
                               int *count_out)
{
    struct kyleos_dirent *entries;
    int fd;
    int count;

    entries = malloc(64 * sizeof(*entries));
    if (entries == NULL) return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(entries);
        return -1;
    }
    count = get_directory_entries(fd, entries, 64);
    close(fd);
    if (count < 0 || count == 64) {
        free(entries);
        return -1;
    }
    *entries_out = entries;
    *count_out = count;
    return 0;
}

static int remove_copy_tree(const char *path, int depth)
{
    struct stat path_stat;

    if (depth > 16 || stat(path, &path_stat) < 0) return -1;
    if (path_stat.st_mode != FINDER_ITEM_DIRECTORY) return unlink(path);
    struct kyleos_dirent *entries;
    int count;
    int result = 0;

    if (read_copy_directory(path, &entries, &count) < 0)
        return remove_empty_directory(path);
    for (int item = 0; item < count; item++) {
        char child[FINDER_PATH_MAX];

        if (strcmp(entries[item].name, ".") == 0 ||
            strcmp(entries[item].name, "..") == 0)
            continue;
        if (child_path(child, path, entries[item].name) < 0 ||
            remove_copy_tree(child, depth + 1) < 0)
            result = -1;
    }
    free(entries);
    if (remove_empty_directory(path) < 0) result = -1;
    return result;
}

static int copy_tree(const char *source, const char *destination, int depth)
{
    struct stat source_stat;
    struct kyleos_dirent *entries;
    int count;

    if (depth > 16 || stat(source, &source_stat) < 0) return -1;
    if (source_stat.st_mode != FINDER_ITEM_DIRECTORY)
        return copy_file_contents(source, destination);
    if (mkdir(destination, 0755) < 0) return -1;
    if (read_copy_directory(source, &entries, &count) < 0) {
        remove_empty_directory(destination);
        return -1;
    }
    for (int item = 0; item < count; item++) {
        char source_child[FINDER_PATH_MAX];
        char destination_child[FINDER_PATH_MAX];

        if (strcmp(entries[item].name, ".") == 0 ||
            strcmp(entries[item].name, "..") == 0)
            continue;
        if (child_path(source_child, source, entries[item].name) < 0 ||
            child_path(destination_child, destination,
                       entries[item].name) < 0 ||
            copy_tree(source_child, destination_child, depth + 1) < 0) {
            free(entries);
            remove_copy_tree(destination, 0);
            return -1;
        }
    }
    free(entries);
    return 0;
}

static int path_is_inside(const char *parent, const char *path)
{
    size_t length = strlen(parent);

    return strncmp(parent, path, length) == 0 && path[length] == '/';
}

static void load_directory(const char *path, struct finder_response *response)
{
    struct kyleos_dirent entries[64];
    int fd;
    int count;

    memset(response, 0, sizeof(*response));
    response->status = -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) return;
    count = get_directory_entries(fd, entries,
                                  (int)(sizeof(entries) / sizeof(entries[0])));
    close(fd);
    if (count < 0) return;

    strncpy(response->path, path, sizeof(response->path) - 1);
    strncpy(response->title, path_name(path), sizeof(response->title) - 1);
    for (int i = 0; i < count && response->item_count < FINDER_MAX_ITEMS;
         i++) {
        int slot;

        if (strcmp(entries[i].name, ".") == 0 ||
            strcmp(entries[i].name, "..") == 0)
            continue;
        slot = response->item_count++;
        strncpy(response->item_names[slot], entries[i].name,
                FINDER_NAME_MAX - 1);
        response->item_names[slot][FINDER_NAME_MAX - 1] = '\0';
        response->item_types[slot] = entries[i].type;
    }
    response->status = 0;
}

static void finder_filter(const struct finder_response *source,
                          struct finder_response *result, const char *query)
{
    size_t query_length = strlen(query);

    *result = *source;
    if (query_length == 0) return;
    result->item_count = 0;
    for (int i = 0; i < source->item_count; i++) {
        int match = 1;
        for (size_t q = 0; q < query_length && match; q++) {
            char wanted = query[q];
            int found = 0;
            if (wanted >= 'A' && wanted <= 'Z') wanted += 'a' - 'A';
            for (size_t n = 0; source->item_names[i][n] != '\0'; n++) {
                char actual = source->item_names[i][n];
                if (actual >= 'A' && actual <= 'Z') actual += 'a' - 'A';
                if (actual == wanted) { found = 1; break; }
            }
            if (!found) match = 0;
        }
        if (match && result->item_count < FINDER_MAX_ITEMS) {
            int slot = result->item_count++;
            strcpy(result->item_names[slot], source->item_names[i]);
            result->item_types[slot] = source->item_types[i];
        }
    }
}

static void history_push(char history[FINDER_HISTORY_MAX][FINDER_PATH_MAX],
                         int *history_count, int *history_index,
                         const char *path)
{
    if (*history_index >= 0 &&
        strcmp(history[*history_index], path) == 0)
        return;
    if (*history_index + 1 < *history_count)
        *history_count = *history_index + 1;
    if (*history_count == FINDER_HISTORY_MAX) {
        memmove(history[0], history[1],
                (FINDER_HISTORY_MAX - 1) * FINDER_PATH_MAX);
        (*history_count)--;
        (*history_index)--;
    }
    strncpy(history[*history_count], path, FINDER_PATH_MAX - 1);
    history[*history_count][FINDER_PATH_MAX - 1] = '\0';
    *history_index = *history_count;
    (*history_count)++;
}

static void parent_path(const char *path, char *parent)
{
    char *slash;

    copy_path(parent, path);
    slash = strrchr(parent, '/');
    if (slash == parent)
        parent[1] = '\0';
    else if (slash != NULL)
        *slash = '\0';
    else
        strcpy(parent, "/");
}

static int navigate_to_directory(
    const char *path, struct finder_response *state,
    struct finder_response *directory_state,
    char history[FINDER_HISTORY_MAX][FINDER_PATH_MAX],
    int *history_count, int *history_index, int *scroll_row,
    char search[FINDER_SEARCH_MAX + 1])
{
    struct finder_response response;

    load_directory(path, &response);
    if (response.status < 0) return -1;
    history_push(history, history_count, history_index, response.path);
    *directory_state = response;
    *state = response;
    search[0] = '\0';
    *scroll_row = 0;
    return 0;
}

static int finder_columns(int width)
{
    int columns = (width - 18 - KUI_SCROLLBAR_SIZE) / FINDER_ITEM_WIDTH;
    return columns > 0 ? columns : 1;
}

static int finder_is_aqua(void)
{
    return (kui_style_current()->flags & KUI_STYLE_AQUA) != 0;
}

static int finder_toolbar_top(void)
{
    return finder_is_aqua() ? 0 : KUI_MENU_BAR_HEIGHT;
}

static int finder_content_hit_top(void)
{
    return finder_toolbar_top() + FINDER_TOOLBAR_HEIGHT;
}

static int finder_content_draw_top(void)
{
    return finder_toolbar_top() + FINDER_TOOLBAR_HEIGHT + 5;
}

static int finder_visible_rows(int height)
{
    int rows = (height - finder_content_draw_top()) / 82;
    return rows > 0 ? rows : 1;
}

static int finder_max_scroll(const struct finder_response *state,
                             int width, int height)
{
    int rows = (state->item_count + finder_columns(width) - 1) /
               finder_columns(width);
    int maximum = rows - finder_visible_rows(height);
    return maximum > 0 ? maximum : 0;
}

static int finder_total_rows(const struct finder_response *state, int width)
{
    return (state->item_count + finder_columns(width) - 1) /
           finder_columns(width);
}

static void draw_folder(struct kui_surface *surface, int x, int y)
{
    if (finder_is_aqua()) {
        kui_fill_rect_alpha(surface, x + 4, y + 40, 54, 3, 0x40000000U);
        kui_fill_rect_alpha(surface, x + 7, y + 2, 25, 10, 0xe05b9bd8U);
        kui_fill_rect_alpha(surface, x + 10, y, 20, 3, 0xd0d9f4ffU);
        kui_fill_rect_alpha(surface, x + 2, y + 9, 56, 31, 0xe03583c9U);
        for (int row = 0; row < 25; row++) {
            uint32_t top = 0x0097dcf7U, bottom = 0x004b9edcU;
            uint32_t r = ((top >> 16) * (24 - row) +
                          (bottom >> 16) * row) / 24;
            uint32_t g = (((top >> 8) & 255) * (24 - row) +
                          ((bottom >> 8) & 255) * row) / 24;
            uint32_t b = ((top & 255) * (24 - row) +
                          (bottom & 255) * row) / 24;
            kui_fill_rect_alpha(surface, x + 2, y + 14 + row, 56, 1,
                                0xd0000000U | (r << 16) | (g << 8) | b);
        }
        kui_fill_rect_alpha(surface, x + 4, y + 15, 52, 4, 0x80e8f9ffU);
        kui_line(surface, x + 2, y + 39, x + 57, y + 39, 0x00316faeU);
        kui_line(surface, x + 5, y + 40, x + 54, y + 40, 0x007bc4edU);
        return;
    }
    kui_fill_rect(surface, x + 4, y, 22, 6, 0x00f2b632U);
    kui_fill_rect(surface, x, y + 5, 48, 32, 0x00ffd45bU);
    kui_outline_rect(surface, x, y + 5, 48, 32, 0x00956a13U);
}

static void draw_document(struct kui_surface *surface, int x, int y)
{
    if (finder_is_aqua()) {
        kui_fill_rect_alpha(surface, x + 8, y + 4, 38, 45,
                            0x50000000U);
        kui_fill_rect(surface, x + 6, y, 36, 46, 0x00fbfdffU);
        kui_outline_rect(surface, x + 6, y, 36, 46, 0x006c8297U);
        kui_fill_rect(surface, x + 33, y + 1, 8, 9, 0x00c9d9e8U);
        kui_line(surface, x + 32, y + 1, x + 41, y + 10, 0x006c8297U);
        for (int row = 0; row < 5; row++)
            kui_fill_rect(surface, x + 11, y + 14 + row * 6, 23, 1,
                          0x00799dccU);
        return;
    }
    kui_fill_rect(surface, x + 7, y, 34, 40, KUI_WHITE);
    kui_outline_rect(surface, x + 7, y, 34, 40, KUI_SHADOW);
    for (int row = 0; row < 4; row++)
        kui_fill_rect(surface, x + 13, y + 9 + row * 6, 22, 1,
                      0x0089a1bdU);
}

static uint32_t aqua_toolbar_background(int y)
{
    return y & 1 ? 0x00e7ebefU : 0x00f7f9faU;
}

static void draw_aqua_toolbar_label(struct kui_surface *surface, int center_x,
                                    const char *label, int disabled)
{
    kui_draw_text(surface, center_x - kui_text_width(label) / 2, 42, label,
                  disabled ? 0x0093999fU : KUI_BLACK,
                  aqua_toolbar_background(42), 0);
}

static void draw_aqua_history_button(struct kui_surface *surface, int center_x,
                                     const char *label, int direction,
                                     int disabled)
{
    uint32_t edge = disabled ? 0x00b9c0c6U : 0x007b858eU;
    uint32_t glyph = disabled ? 0x00abb1b6U : 0x00636c74U;
    int center_y = 20;

    for (int row = -11; row <= 11; row++) {
        int absolute = row < 0 ? -row : row;
        int half = 11 - absolute / 3;
        uint32_t face = row < -4 ? 0x00ffffffU :
                        row < 4 ? 0x00e7eaedU : 0x00c5cbd0U;

        kui_fill_rect(surface, center_x - half, center_y + row,
                      half * 2 + 1, 1, edge);
        if (half > 1)
            kui_fill_rect(surface, center_x - half + 1, center_y + row,
                          half * 2 - 1, 1, face);
    }
    for (int row = -5; row <= 5; row++) {
        int absolute = row < 0 ? -row : row;
        int x = direction < 0 ? center_x - 5 + absolute / 2 :
                                center_x + 4 - absolute / 2;

        kui_fill_rect(surface, x, center_y + row, 2, 1, glyph);
    }
    kui_fill_rect(surface, direction < 0 ? center_x - 3 : center_x - 6,
                  center_y - 1, 10, 3, glyph);
    draw_aqua_toolbar_label(surface, center_x, label, disabled);
}

static void draw_aqua_computer(struct kui_surface *surface, int center_x)
{
    kui_fill_rect_alpha(surface, center_x - 14, 31, 30, 3, 0x40000000U);
    kui_fill_rect(surface, center_x - 13, 8, 27, 20, 0x003a4651U);
    kui_outline_rect(surface, center_x - 13, 8, 27, 20, 0x001c252cU);
    kui_fill_rect(surface, center_x - 10, 11, 21, 14, 0x006ca7d1U);
    kui_fill_rect(surface, center_x - 9, 12, 19, 4, 0x00b9e3faU);
    kui_fill_rect(surface, center_x - 3, 28, 8, 2, 0x0049535bU);
    kui_fill_rect(surface, center_x - 7, 30, 16, 2, 0x00768088U);
    draw_aqua_toolbar_label(surface, center_x, "Computer", 0);
}

static void draw_aqua_home(struct kui_surface *surface, int center_x)
{
    kui_fill_rect_alpha(surface, center_x - 13, 31, 28, 3, 0x40000000U);
    for (int row = 0; row < 11; row++)
        kui_fill_rect(surface, center_x - row - 1, 8 + row,
                      row * 2 + 3, 1, 0x00933b27U);
    kui_fill_rect(surface, center_x - 10, 18, 21, 13, 0x00d6ad78U);
    kui_outline_rect(surface, center_x - 10, 18, 21, 13, 0x00775d42U);
    kui_fill_rect(surface, center_x - 3, 23, 7, 8, 0x00784e34U);
    kui_fill_rect(surface, center_x + 5, 20, 4, 4, 0x009ed3e5U);
    draw_aqua_toolbar_label(surface, center_x, "Home", 0);
}

static void draw_aqua_applications(struct kui_surface *surface, int center_x)
{
    kui_line(surface, center_x - 12, 31, center_x, 8, 0x003f6eb5U);
    kui_line(surface, center_x - 11, 31, center_x + 1, 8, 0x009cc9efU);
    kui_line(surface, center_x + 12, 31, center_x, 8, 0x00836a4aU);
    kui_line(surface, center_x + 11, 31, center_x - 1, 8, 0x00e7c987U);
    kui_fill_rect(surface, center_x - 8, 23, 17, 3, 0x00bf2d2dU);
    kui_fill_rect(surface, center_x - 7, 24, 15, 1, 0x00f2a2a2U);
    kui_fill_rect(surface, center_x - 2, 6, 4, 5, 0x00ded5c4U);
    draw_aqua_toolbar_label(surface, center_x, "Applications", 0);
}

static void draw_aqua_refresh(struct kui_surface *surface, int center_x)
{
    uint32_t dark = 0x002f638fU;
    uint32_t blue = 0x004d9bd3U;

    for (int row = -8; row <= 8; row++) {
        int absolute = row < 0 ? -row : row;
        int outer = 10 - absolute / 2;
        int inner = absolute < 5 ? 6 - absolute / 3 : 4;
        kui_fill_rect(surface, center_x - outer, 20 + row,
                      outer - inner, 2, blue);
        kui_fill_rect(surface, center_x + inner, 20 + row,
                      outer - inner, 2, dark);
    }
    for (int row = 0; row < 7; row++)
        kui_fill_rect(surface, center_x + 4 - row, 8 + row,
                      row + 2, 1, dark);
    for (int row = 0; row < 7; row++)
        kui_fill_rect(surface, center_x - 10, 26 + row,
                      row + 2, 1, blue);
    draw_aqua_toolbar_label(surface, center_x, "Refresh", 0);
}

static void draw_aqua_toolbar(struct kui_surface *surface, int width,
                              const struct finder_response *state,
                              const char *search, int search_focused)
{
    int search_width = width - AQUA_SEARCH_LEFT - 12;

    for (int row = 0; row < FINDER_TOOLBAR_HEIGHT; row++)
        kui_fill_rect(surface, 0, row, width, 1,
                      aqua_toolbar_background(row));
    kui_fill_rect(surface, 0, FINDER_TOOLBAR_HEIGHT - 2, width, 1,
                  0x00ffffffU);
    kui_fill_rect(surface, 0, FINDER_TOOLBAR_HEIGHT - 1, width, 1,
                  0x00858d95U);
    kui_fill_rect(surface, AQUA_FORWARD_RIGHT, 4, 1, 51, 0x00a0a7adU);
    kui_fill_rect(surface, AQUA_FORWARD_RIGHT + 1, 4, 1, 51, KUI_WHITE);
    draw_aqua_history_button(surface, 30, "Back", -1, !state->can_back);
    draw_aqua_history_button(surface, 90, "Forward", 1,
                             !state->can_forward);
    draw_aqua_computer(surface, 155);
    draw_aqua_home(surface, 214);
    draw_aqua_applications(surface, 279);
    draw_aqua_refresh(surface, 351);
    kui_fill_rect(surface, AQUA_SEARCH_LEFT + 7, 7,
                  search_width - 14, 25, KUI_WHITE);
    kui_fill_rect(surface, AQUA_SEARCH_LEFT + 4, 10,
                  search_width - 8, 19, KUI_WHITE);
    kui_outline_rect(surface, AQUA_SEARCH_LEFT + 4, 7,
                     search_width - 8, 25,
                     search_focused ? 0x00508fcaU : 0x0097a1aaU);
    for (int row = -4; row <= 4; row++) {
        int half = row < 0 ? 4 + row / 2 : 4 - row / 2;
        kui_fill_rect(surface, AQUA_SEARCH_LEFT + 16 - half, 17 + row,
                      half * 2 + 1, 1, 0x00717d88U);
    }
    kui_line(surface, AQUA_SEARCH_LEFT + 20, 24,
             AQUA_SEARCH_LEFT + 24, 28, 0x00717d88U);
    kui_draw_text(surface, AQUA_SEARCH_LEFT + 31, 14,
                  search[0] != '\0' ? search : "Search",
                  search[0] != '\0' ? KUI_BLACK : 0x00717d88U,
                  KUI_WHITE, 0);
}

static void finder_render(struct kui_client *client,
                          const struct kui_event *event,
                          const struct finder_response *state,
                          const struct finder_selection *selection,
                          int scroll_row, int context_kind,
                          int context_item, int context_x, int context_y,
                          const char *status_message,
                          const struct kui_scrollbar_state *scrollbar,
                          const char *search, int search_focused)
{
    int columns = finder_columns(event->width);
    int toolbar_top = finder_toolbar_top();
    int content_top = finder_content_draw_top();
    char location[148];

    kui_fill_rect(&client->surface, 0, 0, event->width, event->height,
                  KUI_WHITE);
    if (finder_is_aqua()) {
        draw_aqua_toolbar(&client->surface, event->width, state,
                          search, search_focused);
    } else {
        kui_fill_rect(&client->surface, 0, toolbar_top,
                      event->width, 58, KUI_LIGHT);
        kui_draw_button(&client->surface, 5, toolbar_top + 5,
                        55, 24, "Back", !state->can_back);
        kui_draw_button(&client->surface, 64, toolbar_top + 5,
                        70, 24, "Forward", !state->can_forward);
        kui_draw_button(&client->surface, 138, toolbar_top + 5,
                        40, 24, "Up", !state->can_up);
        kui_draw_button(&client->surface, 182, toolbar_top + 5,
                        64, 24, "Refresh", 0);
        snprintf(location, sizeof(location), "Location: %s", state->path);
        kui_draw_text(&client->surface, 7, toolbar_top + 36, location,
                      KUI_BLACK, KUI_LIGHT, 0);
    }
    for (int item = 0; item < state->item_count; item++) {
        char label[FINDER_NAME_MAX];
        int visible = item - scroll_row * columns;
        int row;
        int column;
        int x;
        int y;
        int label_width;
        uint32_t label_background = KUI_WHITE;
        uint32_t label_color = KUI_BLACK;
        int selected;

        if (visible < 0) continue;
        row = visible / columns;
        column = visible % columns;
        if (row >= finder_visible_rows(event->height)) break;
        x = 9 + column * FINDER_ITEM_WIDTH;
        y = content_top + row * 82;
        selected = finder_selection_contains(selection,
                                             state->item_names[item]) ||
                   (context_kind == FINDER_CONTEXT_ITEM &&
                    item == context_item);
        if (selected) {
            label_background = finder_is_aqua() ? 0x003b83ceU :
                                                  KUI_TITLE_BLUE;
            label_color = KUI_WHITE;
            kui_fill_rect(&client->surface, x + 2, y - 3, 76, 73,
                          label_background);
            kui_outline_rect(&client->surface, x + 2, y - 3, 76, 73,
                             finder_is_aqua() ? 0x00235d9fU : KUI_BLACK);
        }
        if (state->item_types[item] == FINDER_ITEM_DIRECTORY)
            draw_folder(&client->surface, x + 16, y);
        else
            draw_document(&client->surface, x + 16, y);
        label_width = kui_text_ellipsize(label, sizeof(label),
                                         state->item_names[item],
                                         FINDER_LABEL_WIDTH);
        kui_draw_text(&client->surface,
                      x + 40 - label_width / 2, y + 52,
                      label, label_color,
                      label_background, 0);
    }
    kui_draw_vscrollbar(
        &client->surface, event->width - KUI_SCROLLBAR_SIZE, content_top,
        event->height - content_top -
        ((status_message != NULL && status_message[0] != '\0') ? 21 : 0),
        scroll_row, finder_visible_rows(event->height),
        finder_total_rows(state, event->width), scrollbar);
    if (status_message != NULL && status_message[0] != '\0') {
        kui_fill_rect(&client->surface, 0, event->height - 21,
                      event->width, 21, KUI_LIGHT);
        kui_fill_rect(&client->surface, 0, event->height - 21,
                      event->width, 1, KUI_SHADOW);
        kui_draw_text(&client->surface, 7, event->height - 16,
                      status_message, KUI_BLACK, KUI_LIGHT, 0);
    }
    if (context_kind != FINDER_CONTEXT_NONE) {
        static const char *item_labels[] = { "Open", "Copy", "Delete" };
        static const char *background_labels[] = {
            "New Folder", "New Text File", "Paste", "Sort by Type"
        };
        const char **labels = context_kind == FINDER_CONTEXT_ITEM ?
                              item_labels : background_labels;

        kui_fill_rect(&client->surface, context_x + 4, context_y + 4,
                      CONTEXT_MENU_WIDTH, CONTEXT_MENU_HEIGHT,
                      0x00606060U);
        if (finder_is_aqua()) {
            for (int stripe = 0; stripe < CONTEXT_MENU_HEIGHT; stripe++)
                kui_fill_rect(&client->surface, context_x,
                              context_y + stripe, CONTEXT_MENU_WIDTH, 1,
                              stripe & 1 ? 0x00e0e5e9U : 0x00f9fafbU);
        } else {
            kui_fill_rect(&client->surface, context_x, context_y,
                          CONTEXT_MENU_WIDTH, CONTEXT_MENU_HEIGHT, KUI_FACE);
        }
        kui_outline_rect(&client->surface, context_x, context_y,
                         CONTEXT_MENU_WIDTH, CONTEXT_MENU_HEIGHT, KUI_BLACK);
        for (int row = 0; row < (context_kind == FINDER_CONTEXT_ITEM ? 3 : 4); row++) {
            int disabled = 0;
            uint32_t color;

            if (context_kind == FINDER_CONTEXT_ITEM && row == 1 &&
                !finder_selection_copyable(selection, state))
                disabled = 1;
            if (context_kind == FINDER_CONTEXT_BACKGROUND && row == 2 &&
                (event->clipboard_kind != KUI_CLIPBOARD_PATH ||
                 event->clipboard[0] == '\0'))
                disabled = 1;
            color = disabled ? 0x00909090U : KUI_BLACK;
            kui_draw_text(&client->surface, context_x + 12,
                          context_y + 9 + row * CONTEXT_MENU_ROW_HEIGHT,
                          labels[row], color,
                          finder_is_aqua() ?
                          (row & 1 ? 0x00e0e5e9U : 0x00f9fafbU) : KUI_FACE,
                          0);
        }
    }
}

static int finder_item_at(const struct finder_response *state,
                          int width, int height, int scroll_row,
                          int x, int y)
{
    int columns;
    int item;

    if (x < 9 || y < finder_content_hit_top() ||
        x >= width - KUI_SCROLLBAR_SIZE || y >= height)
        return -1;
    columns = finder_columns(width);
    item = scroll_row * columns +
           (y - finder_content_hit_top()) / 82 * columns +
           (x - 9) / FINDER_ITEM_WIDTH;
    return item >= 0 && item < state->item_count ? item : -1;
}

static void activate_item(struct finder_response *state,
                          struct finder_response *directory_state, int item,
                          char history[FINDER_HISTORY_MAX][FINDER_PATH_MAX],
                          int *history_count, int *history_index,
                          int *scroll_row, uint32_t *shell_action,
                          char action_path[FINDER_PATH_MAX])
{
    char path[FINDER_PATH_MAX];
    struct finder_response response;

    if (item < 0 || item >= state->item_count ||
        child_path(path, state->path, state->item_names[item]) < 0)
        return;
    if (state->item_types[item] == FINDER_ITEM_DIRECTORY) {
        load_directory(path, &response);
        if (response.status == 0) {
            history_push(history, history_count, history_index,
                         response.path);
            *directory_state = response;
            *state = response;
            *scroll_row = 0;
        }
    } else {
        copy_path(action_path, path);
        *shell_action = KUI_ACTION_OPEN_PATH;
    }
}

static int reload_current_directory(struct finder_response *state,
                                    struct finder_response *directory_state,
                                    const char *search)
{
    struct finder_response response;

    load_directory(state->path, &response);
    if (response.status < 0) return -1;
    *directory_state = response;
    finder_filter(directory_state, state, search);
    return 0;
}

static int delete_selected_items(
    const struct finder_response *directory_state,
    const struct finder_selection *selection,
    char *status_message, size_t status_capacity)
{
    int deleted = 0;
    int failed = 0;

    for (int selected = 0; selected < selection->count; selected++) {
        const char *name = selection->names[selected];
        int item = finder_item_named(directory_state, name);
        char path[FINDER_PATH_MAX];
        int result;

        if (item < 0 ||
            child_path(path, directory_state->path, name) < 0) {
            failed++;
            continue;
        }
        result = directory_state->item_types[item] == FINDER_ITEM_DIRECTORY ?
                 remove_empty_directory(path) : unlink(path);
        if (result == 0)
            deleted++;
        else
            failed++;
    }
    if (deleted == 1 && failed == 0)
        snprintf(status_message, status_capacity, "Deleted %s",
                 selection->names[0]);
    else if (deleted > 0 && failed == 0)
        snprintf(status_message, status_capacity, "Deleted %d items",
                 deleted);
    else if (deleted > 0)
        snprintf(status_message, status_capacity,
                 "Deleted %d items; %d failed", deleted, failed);
    else if (failed > 0)
        strcpy(status_message, "Delete failed (folders must be empty)");
    return deleted;
}

static int begin_create_dialog(struct kui_input_dialog *dialog,
                               int *create_kind,
                               const struct finder_response *state,
                               int folder)
{
    char path[FINDER_PATH_MAX];

    if (make_new_path(path, state->path, folder) < 0) return -1;
    *create_kind = folder ? FINDER_CREATE_FOLDER : FINDER_CREATE_TEXT_FILE;
    kui_input_dialog_open(dialog,
                          folder ? "New Folder" : "New Text File",
                          folder ? "Folder name:" : "File name:",
                          base_name(path), FINDER_NAME_MAX - 1);
    return 0;
}

static int copy_selection_to_clipboard(
    const struct finder_response *state,
    const struct finder_selection *selection,
    char *clipboard, size_t clipboard_capacity,
    char *status, size_t status_capacity)
{
    size_t used = 0;

    clipboard[0] = '\0';
    if (!finder_selection_copyable(selection, state)) {
        strcpy(status, "Nothing selected to copy");
        return -1;
    }
    for (int selected = 0; selected < selection->count; selected++) {
        char path[FINDER_PATH_MAX];
        size_t length;

        if (child_path(path, state->path,
                       selection->names[selected]) < 0)
            goto too_large;
        length = strlen(path);
        if (used + (used != 0 ? 1 : 0) + length >= clipboard_capacity)
            goto too_large;
        if (used != 0) clipboard[used++] = '\n';
        memcpy(clipboard + used, path, length);
        used += length;
        clipboard[used] = '\0';
    }
    if (selection->count == 1)
        snprintf(status, status_capacity, "Copied %s",
                 selection->names[0]);
    else
        snprintf(status, status_capacity, "Copied %d items",
                 selection->count);
    return 0;

too_large:
    clipboard[0] = '\0';
    strcpy(status, "Too many selected paths to copy");
    return -1;
}

static int paste_clipboard_file(const struct finder_response *state,
                                const char *clipboard,
                                char *status, size_t status_size)
{
    struct stat source_stat;
    char path[FINDER_PATH_MAX];

    if (clipboard == NULL || clipboard[0] == '\0') {
        snprintf(status, status_size, "Nothing to paste");
    } else if (stat(clipboard, &source_stat) < 0) {
        snprintf(status, status_size, "Copied file no longer exists");
    } else if (make_copy_path(path, state->path, clipboard) < 0) {
        snprintf(status, status_size, "Could not choose a copy name");
    } else if (source_stat.st_mode == FINDER_ITEM_DIRECTORY &&
               path_is_inside(clipboard, path)) {
        snprintf(status, status_size, "Cannot copy a folder into itself");
    } else if (copy_tree(clipboard, path, 0) < 0) {
        snprintf(status, status_size, "Paste failed");
    } else {
        snprintf(status, status_size, "Pasted %s", base_name(path));
        return 0;
    }
    return -1;
}

static int paste_clipboard_files(const struct finder_response *state,
                                 const char *clipboard,
                                 char *status, size_t status_size)
{
    const char *cursor = clipboard;
    int pasted = 0;
    int failed = 0;
    char last_status[96];

    last_status[0] = '\0';
    while (cursor != NULL && cursor[0] != '\0') {
        const char *newline = strchr(cursor, '\n');
        size_t length = newline != NULL ? (size_t)(newline - cursor) :
                                          strlen(cursor);
        char source[FINDER_PATH_MAX];

        if (length == 0) {
            cursor = newline != NULL ? newline + 1 : NULL;
            continue;
        }
        if (length >= sizeof(source)) {
            failed++;
        } else {
            memcpy(source, cursor, length);
            source[length] = '\0';
            if (paste_clipboard_file(state, source, last_status,
                                     sizeof(last_status)) == 0)
                pasted++;
            else
                failed++;
        }
        cursor = newline != NULL ? newline + 1 : NULL;
    }
    if (pasted == 1 && failed == 0) {
        strcpy(status, last_status);
    } else if (pasted > 0 && failed == 0) {
        snprintf(status, status_size, "Pasted %d items", pasted);
    } else if (pasted > 0) {
        snprintf(status, status_size, "Pasted %d items; %d failed",
                 pasted, failed);
    } else if (last_status[0] != '\0') {
        strcpy(status, last_status);
    } else {
        strcpy(status, "Nothing to paste");
    }
    return pasted > 0 ? 0 : -1;
}

static int context_menu_row(int menu_x, int menu_y, int x, int y, int rows)
{
    int row;

    if (!kui_in_rect(x, y, menu_x + 1, menu_y + 1,
                     CONTEXT_MENU_WIDTH - 2, CONTEXT_MENU_HEIGHT - 2))
        return -1;
    row = (y - menu_y - 2) / CONTEXT_MENU_ROW_HEIGHT;
    return row >= 0 && row < rows ? row : -1;
}

static int embedded_main(void)
{
    struct finder_response state;
    struct finder_response directory_state;
    struct kui_input_dialog create_dialog;
    struct kui_menu_state menu_state;
    struct kui_scrollbar_state scrollbar;
    char history[FINDER_HISTORY_MAX][FINDER_PATH_MAX];
    int history_count = 0;
    int history_index = -1;
    int scroll_row = 0;
    int context_kind = FINDER_CONTEXT_NONE;
    int context_item = -1;
    int context_x = 0;
    int context_y = 0;
    int create_kind = FINDER_CREATE_NONE;
    char status_message[96];
    struct kui_client client;
    struct kui_event event;
    char search[FINDER_SEARCH_MAX + 1];
    int search_focused = 0;
    struct finder_selection selection;
    char last_click_name[FINDER_NAME_MAX];
    char last_click_path[FINDER_PATH_MAX];
    uint32_t last_click_ms = 0;

    memset(&state, 0, sizeof(state));
    memset(&directory_state, 0, sizeof(directory_state));
    memset(&create_dialog, 0, sizeof(create_dialog));
    memset(&scrollbar, 0, sizeof(scrollbar));
    memset(&selection, 0, sizeof(selection));
    memset(history, 0, sizeof(history));
    status_message[0] = '\0';
    search[0] = '\0';
    last_click_name[0] = '\0';
    last_click_path[0] = '\0';
    kui_menu_init(&menu_state);
    kui_client_init(&client);

    for (;;) {
        struct finder_response response;
        char action_path[KUI_PROTOCOL_CLIPBOARD_MAX];
        uint32_t shell_action = KUI_ACTION_NONE;
        int menu_action = 0;
        int menu_consumed = 0;
        int scrollbar_hit;
        char previous_path[FINDER_PATH_MAX];

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        copy_path(previous_path, state.path);
        scrollbar_hit = !create_dialog.active &&
            event.type == KUI_EVENT_POINTER && event.button == 1 &&
            kui_in_rect(event.x, event.y,
                        event.width - KUI_SCROLLBAR_SIZE,
                        finder_content_draw_top(), KUI_SCROLLBAR_SIZE,
                        event.height - finder_content_draw_top() -
                        (status_message[0] != '\0' ? 21 : 0));
        if (!create_dialog.active || scrollbar.active_direction != 0)
            scroll_row = kui_vscrollbar_handle(
                &scrollbar, &event, event.width - KUI_SCROLLBAR_SIZE,
                finder_content_draw_top(),
                event.height - finder_content_draw_top() -
                (status_message[0] != '\0' ? 21 : 0), scroll_row,
                finder_visible_rows(event.height),
                finder_total_rows(&state, event.width));
        response = state;
        response.status = -1;
        response.action = FINDER_ACTION_NONE;
        response.activated_path[0] = '\0';
        action_path[0] = '\0';
        finder_file_items[2].enabled =
            event.clipboard_kind == KUI_CLIPBOARD_PATH &&
            event.clipboard[0] != '\0';
        if (event.type == KUI_EVENT_INIT) {
            const char *path = event.argument[0] != '\0' ? event.argument : "/";
            load_directory(path, &response);
            if (response.status == 0) {
                history_push(history, &history_count, &history_index,
                             response.path);
                directory_state = response;
                state = response;
                scroll_row = 0;
                finder_filter(&directory_state, &state, search);
                context_kind = FINDER_CONTEXT_NONE;
                create_kind = FINDER_CREATE_NONE;
                kui_input_dialog_close(&create_dialog);
                kui_menu_close(&menu_state);
            }
        } else if (create_dialog.active) {
            int result = kui_input_dialog_handle(&create_dialog, &event);

            if (result == KUI_INPUT_DIALOG_ACCEPT) {
                char path[FINDER_PATH_MAX];
                const char *name = create_dialog.value;
                int created = -1;

                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
                    strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
                    kui_input_dialog_set_error(&create_dialog,
                                               "Invalid name");
                } else if (child_path(path, state.path, name) < 0) {
                    kui_input_dialog_set_error(&create_dialog,
                                               "Path is too long");
                } else if (path_exists(path)) {
                    kui_input_dialog_set_error(&create_dialog,
                                               "That name already exists");
                } else if (create_kind == FINDER_CREATE_FOLDER) {
                    created = mkdir(path, 0755);
                    if (created < 0)
                        kui_input_dialog_set_error(
                            &create_dialog,
                            "Create failed (try an 8.3 name)");
                } else if (create_kind == FINDER_CREATE_TEXT_FILE) {
                    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                    if (fd >= 0) {
                        close(fd);
                        created = 0;
                    } else {
                        kui_input_dialog_set_error(
                            &create_dialog,
                            "Create failed (try an 8.3 name)");
                    }
                }
                if (created == 0) {
                    snprintf(status_message, sizeof(status_message),
                             create_kind == FINDER_CREATE_FOLDER ?
                             "Created folder %s" : "Created %s", name);
                    kui_input_dialog_close(&create_dialog);
                    create_kind = FINDER_CREATE_NONE;
                    reload_current_directory(&state, &directory_state,
                                             search);
                    shell_action = KUI_ACTION_FILE_SAVED;
                }
            } else if (result == KUI_INPUT_DIALOG_CANCEL) {
                create_kind = FINDER_CREATE_NONE;
            }
        } else if (!finder_is_aqua() &&
                   (menu_action = kui_menu_handle(
                        &menu_state, &event, finder_menus, 1,
                        &menu_consumed)) != 0) {
            context_kind = FINDER_CONTEXT_NONE;
            if (menu_action == FINDER_MENU_NEW_FOLDER) {
                if (begin_create_dialog(&create_dialog, &create_kind,
                                        &state, 1) < 0)
                    strcpy(status_message, "Could not choose a folder name");
            } else if (menu_action == FINDER_MENU_NEW_TEXT_FILE) {
                if (begin_create_dialog(&create_dialog, &create_kind,
                                        &state, 0) < 0)
                    strcpy(status_message, "Could not choose a file name");
            } else if (menu_action == FINDER_MENU_PASTE &&
                       event.clipboard_kind == KUI_CLIPBOARD_PATH &&
                       paste_clipboard_files(&state, event.clipboard,
                                             status_message,
                                             sizeof(status_message)) == 0) {
                reload_current_directory(&state, &directory_state, search);
                shell_action = KUI_ACTION_FILE_SAVED;
            } else if (menu_action == FINDER_MENU_SORT_TYPE) {
                sort_by_type(&directory_state);
                finder_filter(&directory_state, &state, search);
                scroll_row = 0;
                strcpy(status_message, "Sorted by type");
            }
        } else if (menu_consumed) {
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_REFRESH) {
            load_directory(state.path, &response);
            if (response.status == 0) {
                directory_state = response;
                finder_filter(&directory_state, &state, search);
            }
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_RESIZE) {
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            scroll_row = kui_scrollbar_wheel(
                scroll_row, event.wheel, finder_visible_rows(event.height),
                finder_total_rows(&state, event.width));
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_POINTER && event.button == 2) {
            if (event.y < finder_toolbar_top() + 58) {
                context_kind = FINDER_CONTEXT_NONE;
            } else {
                context_item = finder_item_at(&state, event.width,
                                              event.height, scroll_row,
                                              event.x, event.y);
                context_kind = context_item >= 0 ? FINDER_CONTEXT_ITEM :
                                                   FINDER_CONTEXT_BACKGROUND;
                if (context_item >= 0 &&
                    !finder_selection_contains(
                        &selection, state.item_names[context_item]))
                    finder_selection_only(
                        &selection, state.item_names[context_item]);
                else if (context_item < 0)
                    finder_selection_clear(&selection);
                last_click_name[0] = '\0';
                context_x = event.x;
                context_y = event.y;
                if (context_x + CONTEXT_MENU_WIDTH + 5 > event.width)
                    context_x = event.width - CONTEXT_MENU_WIDTH - 5;
                if (context_y + CONTEXT_MENU_HEIGHT + 5 > event.height)
                    context_y = event.height - CONTEXT_MENU_HEIGHT - 5;
                if (context_x < 2) context_x = 2;
                if (context_y < 2) context_y = 2;
            }
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   context_kind != FINDER_CONTEXT_NONE) {
            int menu_kind = context_kind;
            int menu_item = context_item;
            int row = context_menu_row(context_x, context_y,
                                       event.x, event.y,
                                       menu_kind == FINDER_CONTEXT_ITEM ?
                                       3 : 4);

            context_kind = FINDER_CONTEXT_NONE;
            if (row >= 0 && menu_kind == FINDER_CONTEXT_ITEM &&
                menu_item >= 0 && menu_item < state.item_count) {
                if (row == 0) {
                    activate_item(&state, &directory_state, menu_item, history,
                                  &history_count, &history_index,
                                  &scroll_row, &shell_action, action_path);
                    status_message[0] = '\0';
                } else if (row == 1) {
                    if (copy_selection_to_clipboard(
                            &state, &selection, action_path,
                            sizeof(action_path), status_message,
                            sizeof(status_message)) == 0)
                        shell_action = KUI_ACTION_COPY_PATH;
                } else if (row == 2) {
                    if (delete_selected_items(
                            &directory_state, &selection, status_message,
                            sizeof(status_message)) > 0) {
                        reload_current_directory(&state, &directory_state,
                                                 search);
                        shell_action = KUI_ACTION_FILE_SAVED;
                    }
                }
            } else if (row >= 0 &&
                       menu_kind == FINDER_CONTEXT_BACKGROUND) {
                int result = -1;

                if (row == 0) {
                    if (begin_create_dialog(&create_dialog, &create_kind,
                                            &state, 1) < 0)
                        strcpy(status_message,
                               "Could not choose a folder name");
                } else if (row == 1) {
                    if (begin_create_dialog(&create_dialog, &create_kind,
                                            &state, 0) < 0)
                        strcpy(status_message,
                               "Could not choose a file name");
                } else if (row == 2 &&
                           event.clipboard_kind == KUI_CLIPBOARD_PATH &&
                           event.clipboard[0] != '\0') {
                    result = paste_clipboard_files(
                        &state, event.clipboard, status_message,
                        sizeof(status_message));
                } else if (row == 3) {
                    sort_by_type(&directory_state);
                    finder_filter(&directory_state, &state, search);
                    scroll_row = 0;
                    strcpy(status_message, "Sorted by type");
                    result = 1;
                }
                if (result == 0) {
                    reload_current_directory(&state, &directory_state,
                                             search);
                    shell_action = KUI_ACTION_FILE_SAVED;
                }
            }
        } else if (scrollbar_hit) {
            context_kind = FINDER_CONTEXT_NONE;
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
                   event.y >= finder_toolbar_top() &&
                   event.y < finder_toolbar_top() +
                   (finder_is_aqua() ? FINDER_TOOLBAR_HEIGHT : 31)) {
            if (finder_is_aqua() && event.x < AQUA_BACK_RIGHT &&
                state.can_back && history_index > 0) {
                load_directory(history[history_index - 1], &response);
                if (response.status == 0) { history_index--; directory_state = response; state = response; search[0] = '\0'; }
            } else if (finder_is_aqua() &&
                       event.x < AQUA_FORWARD_RIGHT &&
                       state.can_forward &&
                       history_index + 1 < history_count) {
                load_directory(history[history_index + 1], &response);
                if (response.status == 0) { history_index++; directory_state = response; state = response; search[0] = '\0'; }
            } else if (finder_is_aqua() && event.x >= AQUA_SEARCH_LEFT) {
                search_focused = 1;
            } else if (finder_is_aqua() &&
                       event.x >= AQUA_FORWARD_RIGHT &&
                       event.x < AQUA_COMPUTER_RIGHT) {
                navigate_to_directory("/", &state, &directory_state,
                                      history, &history_count,
                                      &history_index, &scroll_row, search);
            } else if (finder_is_aqua() &&
                       event.x >= AQUA_COMPUTER_RIGHT &&
                       event.x < AQUA_HOME_RIGHT) {
                if (navigate_to_directory("/home", &state,
                                          &directory_state, history,
                                          &history_count, &history_index,
                                          &scroll_row, search) < 0)
                    navigate_to_directory("/", &state, &directory_state,
                                          history, &history_count,
                                          &history_index, &scroll_row,
                                          search);
            } else if (finder_is_aqua() &&
                       event.x >= AQUA_HOME_RIGHT &&
                       event.x < AQUA_APPLICATIONS_RIGHT) {
                navigate_to_directory("/bin", &state, &directory_state,
                                      history, &history_count,
                                      &history_index, &scroll_row, search);
            } else if (finder_is_aqua() &&
                       event.x >= AQUA_APPLICATIONS_RIGHT &&
                       event.x < AQUA_REFRESH_RIGHT) {
                load_directory(state.path, &response);
                if (response.status == 0) {
                    directory_state = response;
                    finder_filter(&directory_state, &state, search);
                }
            } else if (!finder_is_aqua() && event.x < 62 &&
                       state.can_back && history_index > 0) {
                load_directory(history[history_index - 1], &response);
                if (response.status == 0) { history_index--; directory_state = response; state = response; search[0] = '\0'; }
            } else if (!finder_is_aqua() && event.x < 136 &&
                       state.can_forward &&
                       history_index + 1 < history_count) {
                load_directory(history[history_index + 1], &response);
                if (response.status == 0) { history_index++; directory_state = response; state = response; search[0] = '\0'; }
            } else if (!finder_is_aqua() && event.x < 180 && state.can_up) {
                char path[FINDER_PATH_MAX];
                parent_path(state.path, path);
                load_directory(path, &response);
                if (response.status == 0) {
                    history_push(history, &history_count, &history_index,
                                 response.path);
                    directory_state = response;
                    state = response;
                    search[0] = '\0';
                }
            } else if (!finder_is_aqua() && event.x >= 182 &&
                       event.x < 246) {
                load_directory(state.path, &response);
                if (response.status == 0) {
                    directory_state = response;
                    finder_filter(&directory_state, &state, search);
                }
            }
            scroll_row = 0;
            context_kind = FINDER_CONTEXT_NONE;
            status_message[0] = '\0';
        } else if (event.type == KUI_EVENT_POINTER && event.button == 1) {
            int item = finder_item_at(&state, event.width, event.height,
                                      scroll_row, event.x, event.y);
            int ctrl = (event.modifiers & KUI_MOD_CTRL) != 0;
            int shift = (event.modifiers & KUI_MOD_SHIFT) != 0;

            if (item < 0) {
                if (!ctrl && !shift) finder_selection_clear(&selection);
                last_click_name[0] = '\0';
            } else if (shift) {
                finder_selection_range(&selection, &state, item, ctrl);
                last_click_name[0] = '\0';
            } else if (ctrl) {
                if (finder_selection_contains(
                        &selection, state.item_names[item]))
                    finder_selection_remove(&selection,
                                            state.item_names[item]);
                else
                    finder_selection_add(&selection,
                                         state.item_names[item]);
                strcpy(selection.anchor, state.item_names[item]);
                last_click_name[0] = '\0';
            } else {
                uint32_t now = kui_ticks_ms();
                int double_click =
                    strcmp(last_click_name, state.item_names[item]) == 0 &&
                    strcmp(last_click_path, state.path) == 0 &&
                    (uint32_t)(now - last_click_ms) <= 450U;

                finder_selection_only(&selection, state.item_names[item]);
                if (double_click) {
                    activate_item(&state, &directory_state, item, history,
                                  &history_count, &history_index,
                                  &scroll_row, &shell_action, action_path);
                    last_click_name[0] = '\0';
                } else {
                    strcpy(last_click_name, state.item_names[item]);
                    copy_path(last_click_path, state.path);
                    last_click_ms = now;
                }
            }
            status_message[0] = '\0';
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   (event.modifiers & KUI_MOD_CTRL) &&
                   !event.key_extended && event.key_code == 30) {
            finder_selection_clear(&selection);
            for (int item = 0; item < state.item_count; item++)
                finder_selection_add(&selection, state.item_names[item]);
            if (state.item_count > 0)
                strcpy(selection.anchor, state.item_names[0]);
            search_focused = 0;
            last_click_name[0] = '\0';
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   search_focused && finder_is_aqua()) {
            size_t length = strlen(search);
            if (!event.key_extended && event.key_code == 14) {
                if (length > 0) search[length - 1] = '\0';
            } else if (!event.key_extended && event.key_code == 28) {
                search_focused = 0;
            } else if (event.character >= 32 && event.character < 127 &&
                       !(event.modifiers & (KUI_MOD_CTRL | KUI_MOD_ALT)) &&
                       length < FINDER_SEARCH_MAX) {
                search[length] = event.character;
                search[length + 1] = '\0';
            }
            finder_filter(&directory_state, &state, search);
            scroll_row = 0;
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   event.key_extended &&
                   (event.key_code == 73 || event.key_code == 81)) {
            scroll_row += event.key_code == 73 ?
                -finder_visible_rows(event.height) :
                 finder_visible_rows(event.height);
        }
        state.can_back = history_index > 0;
        state.can_forward = history_index + 1 < history_count;
        state.can_up = state.path[0] != '\0' && strcmp(state.path, "/") != 0;
        if (strcmp(previous_path, state.path) != 0) {
            finder_selection_clear(&selection);
            last_click_name[0] = '\0';
        } else {
            finder_selection_prune(&selection, &state);
        }
        if (scroll_row < 0) scroll_row = 0;
        if (scroll_row > finder_max_scroll(&state, event.width, event.height))
            scroll_row = finder_max_scroll(&state, event.width, event.height);
        kui_client_begin(&client, &event,
                         state.title[0] != '\0' ? state.title : "Finder");
        kui_client_set_constraints(&client, 480, 243, 700, 504);
        finder_render(&client, &event, &state, &selection, scroll_row,
                      context_kind, context_item, context_x, context_y,
                      status_message, &scrollbar, search, search_focused);
        if (!finder_is_aqua())
            kui_menu_render(&client.surface, &menu_state, finder_menus, 1);
        kui_input_dialog_render(&client.surface, &event, &create_dialog);
        kui_client_set_timer(&client, kui_scrollbar_timer_ms(&scrollbar));
        if (shell_action != KUI_ACTION_NONE)
            kui_client_set_action(&client, shell_action, action_path);
        if (kui_client_send(&client) < 0) break;
    }
    kui_client_free(&client);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "--embedded") == 0 ||
                     strcmp(argv[1], "--kui-client") == 0))
        return embedded_main();
    puts("Finder is a desktop application. Run 'desktop' first.");
    return 0;
}

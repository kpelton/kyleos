#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "extras/kui/kui.h"
#include "extras/kui/kui_client.h"

#define HELP_SIDEBAR_WIDTH 176
#define HELP_HEADER_HEIGHT 52
#define HELP_TOPIC_TOP 38
#define HELP_TOPIC_HEIGHT 25
#define HELP_LINE_HEIGHT 18

struct help_topic {
    const char *title;
    const char *body;
};

static const struct help_topic topics[] = {
    {
        "Getting Started",
        "Welcome to KyleOS Help.\n"
        "\n"
        "Use the list on the left to choose a topic.\n"
        "Scroll articles with the wheel, arrow keys,\n"
        "Page Up/Page Down, or the scroll bar.\n"
        "\n"
        "Starting applications\n"
        "- Aqua: click K or an icon in the Dock.\n"
        "- Classic: use Start or a desktop icon.\n"
        "- Click a document to open its matching app.\n"
        "\n"
        "Using the pointer\n"
        "- Click once to select or activate.\n"
        "- Hold the left button while dragging items.\n"
        "- Right-click where offered for more actions.\n"
        "- Roll the wheel to move through long views.\n"
        "\n"
        "Using the keyboard\n"
        "- Arrow keys move through text and grids.\n"
        "- Ctrl is used for common file shortcuts.\n"
        "- Alt+Q closes the desktop environment.\n"
        "\n"
        "Closing the desktop\n"
        "- Press Alt+Q to return to the shell.\n"
        "- Aqua also provides Quit Aqua in K.\n"
    },
    {
        "Aqua Desktop",
        "Aqua is KyleOS's Mac OS X-style desktop.\n"
        "\n"
        "Top bar\n"
        "- K opens the application menu.\n"
        "- The active application appears beside K.\n"
        "- The clock is at the right edge.\n"
        "\n"
        "Dock\n"
        "- Click an app icon to open the app.\n"
        "- Click a running app to bring it forward.\n"
        "- Icons enlarge as the pointer approaches.\n"
        "- A mark under an icon means it is running.\n"
        "- Minimized windows remain available there.\n"
    },
    {
        "Windows",
        "Every KUI app runs in a desktop window.\n"
        "\n"
        "- Click a window to bring it to the front.\n"
        "- Drag the title bar to move it.\n"
        "- Drag the lower-right grip to resize it.\n"
        "- Use the title-bar controls to close,\n"
        "  minimize, or maximize the window.\n"
        "- Click its taskbar or Dock icon to restore.\n"
        "\n"
        "Scroll bars\n"
        "- Click an arrow to move one step.\n"
        "- Hold an arrow for continuous scrolling.\n"
        "- Click the trough to move one page.\n"
        "- The mouse wheel moves three rows.\n"
    },
    {
        "Finder",
        "Finder browses files and folders.\n"
        "\n"
        "- Back, Forward, and Up change folders.\n"
        "- Click a folder to enter it.\n"
        "- Click a document to open its default app.\n"
        "- Use the wheel or scroll bar for more items.\n"
        "- Page Up/Page Down move by a full view.\n"
        "\n"
        "Right-click an item\n"
        "- Open, Copy, or Delete.\n"
        "- Folders must be empty before deletion.\n"
        "\n"
        "Right-click empty space\n"
        "- Create a folder or text file, or Paste.\n"
        "- File > provides the same create actions.\n"
    },
    {
        "Text Editor",
        "Text Editor opens and saves plain text.\n"
        "\n"
        "File shortcuts\n"
        "- Ctrl+O: Open\n"
        "- Ctrl+S: Save\n"
        "- Ctrl+Shift+S: Save As\n"
        "\n"
        "Editing\n"
        "- Click or drag to place/select text.\n"
        "- Shift+arrows extends the selection.\n"
        "- Ctrl+X/C/V cuts, copies, and pastes.\n"
        "- Ctrl+A selects all text.\n"
        "- Delete and Backspace remove text.\n"
        "- The wheel and scroll bar move the page.\n"
        "- Right-click for the editing menu.\n"
    },
    {
        "Notes",
        "Notes is a quick plain-text notebook.\n"
        "\n"
        "- It opens /NOTES.TXT by default.\n"
        "- Click or drag to place and select text.\n"
        "- Ctrl+X/C/V cuts, copies, and pastes.\n"
        "- Ctrl+A selects all text.\n"
        "- Ctrl+O opens another text file.\n"
        "- Ctrl+S saves; Ctrl+Shift+S uses Save As.\n"
        "- The wheel and scroll bar move the page.\n"
        "\n"
        "Notes always saves ordinary plain text, so\n"
        "its files also work in Text Editor.\n"
    },
    {
        "Writer",
        "Writer creates formatted KWP documents.\n"
        "\n"
        "Toolbar\n"
        "- Open, Save, and Save As manage documents.\n"
        "- B toggles bold for the document.\n"
        "- A- and A+ decrease or increase font size.\n"
        "- Size 1, 2, and 3 are available.\n"
        "\n"
        "Keyboard\n"
        "- Ctrl+B toggles bold.\n"
        "- Ctrl+plus/minus changes font size.\n"
        "- Ctrl+X/C/V and Ctrl+A edit selections.\n"
        "- Ctrl+O/S opens or saves a document.\n"
        "- Ctrl+Shift+S opens Save As.\n"
        "\n"
        "KWP files preserve the text, size, and bold\n"
        "setting. Formatting applies to the complete\n"
        "document. Use an 8.3 filename when saving.\n"
    },
    {
        "C IDE",
        "C IDE adds source tools to Text Editor.\n"
        "\n"
        "- C syntax is highlighted while you type.\n"
        "- Open, Save, selection, and clipboard\n"
        "  controls work like Text Editor.\n"
        "- F5 or Build compiles the current source.\n"
        "- F6 or Build and Run compiles and starts it.\n"
        "- Save the source before building.\n"
        "- Build output appears in the status area.\n"
        "\n"
        "The default hello.c is a small starting\n"
        "example for KyleOS programs.\n"
    },
    {
        "Terminal",
        "Terminal runs command-line programs.\n"
        "\n"
        "- Type a command and press Enter.\n"
        "- Backspace edits the current command.\n"
        "- Ctrl+L clears the visible history.\n"
        "- Use the wheel or scroll bar for history.\n"
        "\n"
        "Built-in commands\n"
        "- clear: clear the terminal\n"
        "- pwd: show the current folder\n"
        "- cd PATH: change the current folder\n"
        "- jobs: list background and stopped jobs\n"
        "- fg %N / bg %N: resume a shell job\n"
        "- kill [-SIGNAL] PID|%N: signal a process or job\n"
        "\n"
        "Programs are found in /bin and /usr/bin.\n"
        "GUI names such as editor and calculator\n"
        "open their corresponding desktop apps.\n"
    },
    {
        "KSheet",
        "KSheet edits CSV spreadsheets.\n"
        "\n"
        "- Click a cell to select it.\n"
        "- Drag across cells to select a range.\n"
        "- Type to replace a cell, or press F2 to edit.\n"
        "- Enter moves down; Tab moves right.\n"
        "- Arrow keys move between cells.\n"
        "- Shift+arrows extends a selection.\n"
        "- Delete clears selected cells.\n"
        "- Ctrl+X/C/V uses tabular clipboard data.\n"
        "\n"
        "Files\n"
        "- Ctrl+N creates a new sheet.\n"
        "- Ctrl+O/S opens or saves CSV files.\n"
        "- Ctrl+Shift+S opens Save As.\n"
    },
    {
        "Paint",
        "Paint creates 256 x 192 KIMG pictures.\n"
        "\n"
        "- Choose Pencil or Eraser.\n"
        "- Choose one of 16 colors.\n"
        "- Choose a 1, 3, 5, 9, or 15 pixel brush.\n"
        "- Hold the left button and drag to draw.\n"
        "- The canvas scales with the window.\n"
        "\n"
        "Files\n"
        "- Ctrl+O opens a KIMG image.\n"
        "- Ctrl+S saves the current image.\n"
        "- Ctrl+Shift+S opens Save As.\n"
        "- Edit > Clear Image erases the canvas.\n"
        "- Unsaved destructive actions ask first.\n"
    },
    {
        "Calculator",
        "Calculator performs floating-point arithmetic.\n"
        "\n"
        "- Click the on-screen keys or use keyboard\n"
        "  digits, decimal point, and +, -, *, /.\n"
        "- Enter or = calculates the result.\n"
        "- C clears the calculator.\n"
        "- Backspace removes the last digit.\n"
        "- N changes the number's sign.\n"
        "- % divides the displayed value by 100.\n"
        "- Division by zero displays an error; clear\n"
        "  it with C before continuing.\n"
    },
    {
        "System Monitor",
        "System Monitor shows live kernel data.\n"
        "\n"
        "- The process list comes from /proc/processes.\n"
        "- Memory totals come from /proc/meminfo.\n"
        "- The display refreshes once per second.\n"
        "- Resize the window to show wider fields.\n"
        "\n"
        "This app is read-only. It does not stop or\n"
        "change processes.\n"
    },
    {
        "Minesweeper",
        "Clear every safe square without hitting a mine.\n"
        "\n"
        "- Left-click a covered square to reveal it.\n"
        "- Right-click to place or remove a flag.\n"
        "- Numbers count mines in adjacent squares.\n"
        "- The first revealed area is kept safe.\n"
        "- Click Restart/New Game or press R to reset.\n"
        "- Game > New Game also resets the board.\n"
    },
    {
        "Solitaire",
        "Solitaire is one-card Klondike.\n"
        "\n"
        "- Click the stock to deal a card.\n"
        "- Click the empty stock to recycle the waste.\n"
        "- Click a card or stack to select it.\n"
        "- Click a destination to move the selection.\n"
        "- Build tableau columns downward with\n"
        "  alternating colors.\n"
        "- Build foundations upward by suit from Ace.\n"
        "- Click New Deal or press R to redeal.\n"
    },
    {
        "Tetris",
        "Arrange falling pieces into complete rows.\n"
        "\n"
        "- Left/Right: move\n"
        "- Down: soft drop\n"
        "- Up: rotate clockwise\n"
        "- Z: rotate counterclockwise\n"
        "- Space: hard drop\n"
        "- P: pause or resume\n"
        "- R: start a new game\n"
        "\n"
        "The Game menu also provides New Game and\n"
        "Pause/Resume. Speed increases with level.\n"
    },
    {
        "City Builder",
        "Zone and grow a small simulated city.\n"
        "\n"
        "Choose a tool, then click or drag on the map.\n"
        "- 1: Bulldozer ($25)\n"
        "- 2: Road ($10)\n"
        "- 3: Residential ($100)\n"
        "- 4: Commercial ($150)\n"
        "- 5: Industrial ($150)\n"
        "- 6: Park ($250)\n"
        "\n"
        "- Space pauses or resumes simulation.\n"
        "- N creates a new city.\n"
        "- Watch funds, population, and demand.\n"
        "- Zones grow when roads and demand allow it.\n"
    },
    {
        "Parallel Writers",
        "Parallel Writers demonstrates multitasking.\n"
        "\n"
        "- Open Parallel Writers from the app menu.\n"
        "- Writer A automatically launches Writer B.\n"
        "- Both windows update on independent timers.\n"
        "- Move, overlap, minimize, and restore them\n"
        "  while their output continues.\n"
        "\n"
        "The demo is informational and has no controls\n"
        "inside either window.\n"
    },
    {
        "Duke 3D",
        "Duke Nukem 3D runs as an exclusive app.\n"
        "\n"
        "- Starting it temporarily leaves the desktop.\n"
        "- Use the game's own keyboard controls/menus.\n"
        "- Exit the game to return to the KUI desktop.\n"
        "\n"
        "The game requires its data file in the image.\n"
        "If it does not start, rebuild the image with\n"
        "the configured Duke 3D assets.\n"
    },
    {
        "Help",
        "This application documents the KyleOS desktop.\n"
        "\n"
        "- Choose articles in the left sidebar.\n"
        "- Use Up/Down for one-line scrolling.\n"
        "- Use Page Up/Page Down for a full page.\n"
        "- Use the wheel or the Aqua scroll bar.\n"
        "- Hold a scroll arrow to keep moving.\n"
        "\n"
        "Documentation is built into the Help binary,\n"
        "so it remains available without data files.\n"
    }
};

static int topic_count(void)
{
    return (int)(sizeof(topics) / sizeof(topics[0]));
}

static int body_line_count(const char *body)
{
    int lines = 1;

    for (; *body != '\0'; body++)
        if (*body == '\n') lines++;
    return lines;
}

static int visible_lines(int height)
{
    int lines = (height - HELP_HEADER_HEIGHT - 10) / HELP_LINE_HEIGHT;

    return lines > 0 ? lines : 1;
}

static int maximum_scroll(int selected, int height)
{
    int maximum = body_line_count(topics[selected].body) -
                  visible_lines(height);

    return maximum > 0 ? maximum : 0;
}

static void clamp_scroll(int *scroll, int selected, int height)
{
    int maximum = maximum_scroll(selected, height);

    if (*scroll < 0) *scroll = 0;
    if (*scroll > maximum) *scroll = maximum;
}

static void draw_sidebar(struct kui_surface *surface, int height,
                         int selected)
{
    kui_fill_rect(surface, 0, 0, HELP_SIDEBAR_WIDTH, height, 0x00e6eaeeU);
    kui_fill_rect(surface, HELP_SIDEBAR_WIDTH - 1, 0, 1, height,
                  0x009aa3acU);
    kui_draw_text(surface, 14, 10, "HELP TOPICS", 0x005b6570U,
                  0x00e6eaeeU, 0);
    for (int index = 0; index < topic_count(); index++) {
        int y = HELP_TOPIC_TOP + index * HELP_TOPIC_HEIGHT;
        uint32_t text = KUI_BLACK;

        if (index == selected) {
            kui_fill_rect(surface, 7, y, HELP_SIDEBAR_WIDTH - 15,
                          HELP_TOPIC_HEIGHT - 2, KUI_TITLE_BLUE);
            kui_fill_rect(surface, 10, y + 1, HELP_SIDEBAR_WIDTH - 21, 1,
                          0x00c6e8ffU);
            text = KUI_WHITE;
        }
        kui_draw_text(surface, 15, y + 3, topics[index].title, text,
                      index == selected ? KUI_TITLE_BLUE : 0x00e6eaeeU, 0);
    }
}

static void draw_article(struct kui_surface *surface, int width, int height,
                         int selected, int scroll,
                         const struct kui_scrollbar_state *scrollbar)
{
    const char *cursor = topics[selected].body;
    int line = 0;
    int drawn = 0;
    int page = visible_lines(height);
    int total = body_line_count(cursor);
    int left = HELP_SIDEBAR_WIDTH + 18;
    int text_width = width - left - KUI_SCROLLBAR_SIZE - 10;

    kui_fill_rect(surface, HELP_SIDEBAR_WIDTH, 0,
                  width - HELP_SIDEBAR_WIDTH, height, KUI_WHITE);
    kui_fill_rect(surface, HELP_SIDEBAR_WIDTH, 0,
                  width - HELP_SIDEBAR_WIDTH, HELP_HEADER_HEIGHT,
                  0x00f4f6f8U);
    kui_fill_rect(surface, HELP_SIDEBAR_WIDTH, HELP_HEADER_HEIGHT - 1,
                  width - HELP_SIDEBAR_WIDTH, 1, 0x00b4bbc2U);
    kui_draw_text(surface, left, 14, topics[selected].title,
                  KUI_TITLE_BLUE, 0x00f4f6f8U, 0);

    while (*cursor != '\0' && drawn < page) {
        const char *end = strchr(cursor, '\n');
        int length = end != NULL ? (int)(end - cursor) : (int)strlen(cursor);

        if (line >= scroll) {
            char text[96];

            if (length >= (int)sizeof(text)) length = sizeof(text) - 1;
            memcpy(text, cursor, (size_t)length);
            text[length] = '\0';
            if (kui_mono_text_width(text) > text_width) {
                int columns = text_width / 8;
                if (columns < 0) columns = 0;
                if (length > columns) text[columns] = '\0';
            }
            kui_draw_mono_text(surface, left,
                               HELP_HEADER_HEIGHT + 6 +
                               drawn * HELP_LINE_HEIGHT,
                               text,
                               text[0] == '-' ? 0x002f67a3U : KUI_BLACK,
                               KUI_WHITE, 0);
            drawn++;
        }
        line++;
        if (end == NULL) break;
        cursor = end + 1;
    }
    kui_draw_vscrollbar(surface, width - KUI_SCROLLBAR_SIZE,
                        HELP_HEADER_HEIGHT,
                        height - HELP_HEADER_HEIGHT,
                        scroll, page, total, scrollbar);
}

static int topic_at(int x, int y)
{
    int index;

    if (x < 0 || x >= HELP_SIDEBAR_WIDTH || y < HELP_TOPIC_TOP)
        return -1;
    index = (y - HELP_TOPIC_TOP) / HELP_TOPIC_HEIGHT;
    return index >= 0 && index < topic_count() ? index : -1;
}

static int embedded_main(void)
{
    struct kui_scrollbar_state scrollbar;
    struct kui_client client;
    struct kui_event event;
    int selected = 0;
    int scroll = 0;

    memset(&scrollbar, 0, sizeof(scrollbar));
    kui_client_init(&client);
    for (;;) {
        int scrollbar_hit;

        if (kui_client_read_event(&event) < 0) break;
        if (event.type == KUI_EVENT_CLOSE) break;
        if (event.type == KUI_EVENT_INIT) {
            selected = 0;
            scroll = 0;
            memset(&scrollbar, 0, sizeof(scrollbar));
        }
        scrollbar_hit = event.type == KUI_EVENT_POINTER &&
            event.button == 1 &&
            kui_in_rect(event.x, event.y,
                        event.width - KUI_SCROLLBAR_SIZE,
                        HELP_HEADER_HEIGHT, KUI_SCROLLBAR_SIZE,
                        event.height - HELP_HEADER_HEIGHT);
        scroll = kui_vscrollbar_handle(
            &scrollbar, &event, event.width - KUI_SCROLLBAR_SIZE,
            HELP_HEADER_HEIGHT, event.height - HELP_HEADER_HEIGHT,
            scroll, visible_lines(event.height),
            body_line_count(topics[selected].body));
        if (event.type == KUI_EVENT_POINTER && event.button == 1 &&
            !scrollbar_hit) {
            int topic = topic_at(event.x, event.y);

            if (topic >= 0) {
                selected = topic;
                scroll = 0;
                memset(&scrollbar, 0, sizeof(scrollbar));
            }
        } else if (event.type == KUI_EVENT_POINTER && event.wheel != 0) {
            scroll = kui_scrollbar_wheel(
                scroll, event.wheel, visible_lines(event.height),
                body_line_count(topics[selected].body));
        } else if (event.type == KUI_EVENT_KEY && event.key_pressed &&
                   event.key_extended) {
            if (event.key_code == 72) scroll--;
            else if (event.key_code == 80) scroll++;
            else if (event.key_code == 73)
                scroll -= visible_lines(event.height);
            else if (event.key_code == 81)
                scroll += visible_lines(event.height);
        }
        clamp_scroll(&scroll, selected, event.height);
        kui_client_begin(&client, &event, "KyleOS Help");
        kui_client_set_constraints(&client, 650, 500, 760, 540);
        draw_sidebar(&client.surface, event.height, selected);
        draw_article(&client.surface, event.width, event.height,
                     selected, scroll, &scrollbar);
        kui_client_set_timer(&client, kui_scrollbar_timer_ms(&scrollbar));
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
    puts("Help is a desktop application. Run 'desktop' first.");
    return 0;
}

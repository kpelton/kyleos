# KyleOS GUI Architecture

## System overview

```mermaid
flowchart TB
    K[Kernel\nprocesses, pipes, files, framebuffer/input]
    D[Desktop compositor\n/bin/kuid]
    S[Shell window manager\nextras/desktop/shell.c]
    P[KUI protocol\nextras/kui/kui_protocol.h]
    L[libkui\nrendering, widgets, menus, scrollbars]
    A[GUI applications\nFinder, Notes, Writer, Editor, KSheet, Paint, Help]
    F[Shared file dialog\nopen/save/overwrite confirmation]

    K --> D
    D --> S
    S <--> P
    P <--> A
    A --> L
    L --> P
    A --> F
    F --> K
```

The desktop owns the screen and application windows. Applications never draw
directly to the framebuffer. Each application receives a `kui_event`, builds a
display command buffer with libkui, and returns a `kui_response`.

## Application lifecycle

```mermaid
sequenceDiagram
    participant User
    participant Shell as /bin/kuid
    participant App as GUI app
    participant Kernel

    User->>Shell: click icon/menu
    Shell->>Kernel: pipe + fork + exec
    Kernel-->>App: stdin/stdout KUI streams
    Shell->>App: KUI_EVENT_INIT
    App-->>Shell: title, constraints, display commands
    Shell-->>User: composed window
    User->>Shell: pointer/key/timer
    Shell->>App: event with serial
    App-->>Shell: response + display commands/action
    Shell-->>User: redraw window
    User->>Shell: close window
    Shell->>App: KUI_EVENT_CLOSE
    Shell->>Kernel: close pipes/reap child
```

Once per desktop iteration, the compositor passes the shell response pipe and
every visible application's response pipe to one nonblocking `poll()` call.
`POLLHUP`, `POLLERR`, or `POLLNVAL` removes a disconnected window immediately.
After sending an event, bounded KUI response reads use `poll()` with the
remaining protocol deadline instead of repeatedly checking descriptor status.
Terminal uses the same API to drain command output without blocking its event
loop.

## Rendering pipeline

1. The shell creates a window-sized `kui_surface` command buffer.
2. The app records rectangles, text, lines, bitmaps, and scrollbar primitives.
3. `kui_client_send()` serializes the response and command buffer over stdout.
4. The shell validates the protocol serial and display size.
5. The shell replays commands into the window pixel buffer.
6. The compositor composites windows, menus, taskbar, and cursor into the frame.

The current display-command limit is 512 KiB (`KUI_PROTOCOL_MAX_DISPLAY`).
Text commands can request monospace or proportional glyphs, opacity, bold
stroke weight, and integer scaling from 1x through 4x. Writer uses these
validated command flags so styled text follows the same client/compositor path
as every other KUI primitive.

## Input and scrolling

```mermaid
flowchart LR
    I[Keyboard / mouse / wheel] --> S[Shell]
    S --> E[kui_event]
    E --> W[App event loop]
    W --> SB[Shared scrollbar state]
    SB --> R[scroll position]
    R --> V[visible rows/columns]
    V --> D[rendered display commands]
```

Wheel movement is normalized by `kui_scrollbar_wheel()`. Arrow buttons use the
same scrollbar state and timer, with an initial delay followed by repeat ticks.

## Save As flow

```mermaid
sequenceDiagram
    participant App
    participant Dialog as kui_file_dialog
    participant FS as VFS/filesystem
    participant Shell

    App->>Dialog: open SAVE dialog
    Dialog->>App: filename field + directory
    App-->>Shell: dialog display commands
    Shell->>App: typed filename / Enter
    Dialog->>FS: test target existence
    alt target exists
        Dialog-->>App: overwrite confirmation
        Shell->>App: confirmation Enter
    end
    Dialog-->>App: selected absolute path
    App->>FS: write file
    App->>App: safely copy path with bounded memmove
    App-->>Shell: KUI_ACTION_FILE_SAVED
```

Save operations use bounded overlap-safe path copies. The shell appends
lifecycle and protocol diagnostics to `/shell.log` without using stdout, which
is reserved for the KUI protocol.

## Main source locations

| Layer | Source |
|---|---|
| Desktop/window manager | `extras/desktop/shell.c` |
| Protocol definitions | `extras/kui/kui_protocol.h` |
| Rendering/widgets | `extras/kui/kui.c` and `extras/kui/*.c` |
| Shared file dialog | `extras/kui/kui_file_dialog.c` |
| KSheet | `extras/ksheet/ksheet.c` |
| Editor/CIDE | `extras/editor/editor.c`, `extras/cide/cide.c` |
| Notes | `extras/editor/editor.c` (`NOTES_APP`) |
| Writer/KWP documents | `extras/wordprocessor` |
| Terminal | `extras/terminal/terminal.c` |
| Help | `extras/help/help.c` |

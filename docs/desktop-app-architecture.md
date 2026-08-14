# KyleOS desktop application architecture

```text
+--------------------------- KERNEL ----------------------------+
| mouse/key queues        processes + pipes       framebuffer   |
+----------+----------------------+----------------------+-------+
           |                      |                      ^
           v                      v                      |
+------------------------ DESKTOP SHELL -------------------------+
| input routing | window manager | launcher | compositor        |
|                                                               |
| generic client connections      cached client content surfaces|
+------------------+-------------------------------+-------------+
                   |                               ^
       generic KUI events              recorded KUI display lists
                   |                               |
                   v                               |
+---------------------- APPLICATION PROCESSES -------------------+
| Finder | Editor | Terminal | Calculator | Games | Sysmon      |
|                                                               |
| Each client owns its state, rendering, hit testing and timers. |
+---------------------------------------------------------------+

 /usr/share/applications/*.app
             |
             +---- name, executable, icon, launch argument,
                   placement, file association and window size
```

## Ownership

| Component | Owns |
| --- | --- |
| Kernel | Input queues, scheduling, pipes and framebuffer hardware |
| Desktop shell | Window chrome, focus, placement, input routing, process lifecycle and final composition |
| Application process | Application state, content rendering, content input behavior and timers |
| Application manifests | Discovery, icons, executable paths, default dimensions and file associations |
| KUI client runtime | Versioned events, responses and recorded display-list transport |

The shell has no application enum, application state, or application-specific
IPC headers. Adding a windowed application requires an executable that speaks
the KUI protocol and a manifest under `/usr/share/applications`.

## Event transaction

1. The shell sends a generic `INIT`, `POINTER`, `KEY`, `RESIZE`, `TIMER`,
   `REFRESH`, or `CLOSE` event using content-local coordinates.
2. The client updates its private state and records its content through normal
   KUI drawing primitives.
3. The client returns properties, an optional shell action, and a display list.
4. The shell validates and replays that list into the window's cached content
   surface.
5. Moving, focusing, exposing, or minimizing a window reuses the cache; it does
   not contact the client.
6. The compositor combines the wallpaper, icons, cached content, window chrome,
   taskbar, menus and cursor, then presents the final framebuffer.

Shell actions are deliberately generic: `OPEN_PATH`, `LAUNCH`, `FILE_SAVED`,
and `CLOSE_SELF`. For example, Finder requests `OPEN_PATH` instead of naming
the editor, and Terminal requests `LAUNCH` instead of returning an application
enum.

## Shared-memory path

Shared memory is not required for the current applications. Their retained
display lists fit the existing pipe model and are transferred only after client
content changes. A future protocol version can add a shared-surface frame type
plus damage rectangles for high-frequency raster clients such as Doom, Quake,
video players, or animation tools. The desktop remains the only process that
presents the physical framebuffer.

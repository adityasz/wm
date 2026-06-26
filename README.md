# WM

A Hyprland plugin that provides app and window switchers and some dispatchers.

Tiled layout WIP.

## Installation

See the [CMake experimental features guide](https://github.com/Kitware/CMake/blob/master/Help/dev/experimental.rst)
(for the right CMake version) to get the value for the `import std` gate.

- Generate the build system:
  ```console
  $ cmake --preset release-build   \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=<UUID>
  ```
- Compile: `cmake --build build/release -j [<jobs>]`
- Install: `cmake --install build/release --prefix ~/.local` will install the
  plugin at `~/.local/lib/libwm.so`.

### Options

The following options can be set when generating the build system:

- `-DSWITCHER_MOD=<KEY>`: The modifier key for the app and window switcher
  [default for release build: `KEY_LEFTMETA`]. See
  [`/usr/include/linux/input-event-codes.h`](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h)
  for the list of keys.

  Use [<kbd>shift</kbd>]<kbd>tab</kbd> for switching between applications, and
  [<kbd>shift</kbd>]<kbd>\`</kbd> for switching between windows of an app. These
  are hardcoded in
  [`lib/WindowManager/WindowManager.cpp`](lib/WindowManager/WindowManager.cpp)`:WindowManager::on_key_press(IKeyboard::SKeyEvent e, Event::SCallbackInfo &info)`.
  It is essential to keep these values compile-time constants since this
  function is run on each keypress and release.

- `-DDEBUG_LOGS=<ON|OFF>`: Enable/disable debug logs.

## Configuration

```lua
hl.config({
    plugin = {
        wm = {
            app_switcher = {
                container = {
                    background_color = "#ffffff11",
                    border_color = "#80808011",
                    padding = 20,
                    radius = 50,
                    border_width = 1,
                },
                selection = {
                    background_color = "#00000011",
                    padding = 10,
                    radius = 40,
                },
                label = {
                    font_family = "Inter",
                    font_color = "#ffffff",
                    font_size = 0,
                    separation = 0,
                },
                icons = {
                    size = 120,
                    separation = 40,
                    theme = "", -- themes must be comma-separated, e.g., "a,b,c"
                }
            }
        }
    }
})
```

`hl.plugin.wm.{focus,move}_or_exec` take a table with `class` and `exec` fields.
E.g.,

```lua
hl.bind("SUPER + 1", hl.plugin.wm.focus_or_exec({ class = "kitty", exec = "kitty" }))
hl.bind("SUPER + SHIFT + 1", hl.plugin.wm.move_or_exec({ class = "kitty", exec = "kitty" }))
```

[Example binds](https://github.com/adityasz/dotfiles/blob/4b005be7d9979768b1af7a16a103815431189a2e/.config/hypr/keymap.lua#L52).

# WM

An app and window switcher for Hyprland. Works similar to macOS's command-tab/grave thing:
- Shows icons for apps instead of window previews.
- Switches windows of an app directly (instead of showing previews in a container).

## Installation

See
[`CMakeLists.txt`](https://github.com/adityasz/wm/blob/master/CMakeLists.txt)
for the full list of dependencies.

```console
$ cmake -B build/release -S . -GNinja -DCMAKE_BUILD_TYPE=Release
$ cmake --build build/release [-j<JOBS>]
$ hyprctl plugin load $(realpath build/release/libwm.so)
```
  
- The modifier key for switching apps and windows can be set with
  `-DSWITCHER_MOD=<KEY>`. The default is `KEY_LEFTMETA`. See
  [`/usr/include/linux/input-event-codes.h`](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h)
  for the list of keys. Use [<kbd>shift</kbd>]<kbd>tab</kbd> for switching
  between applications, and [<kbd>shift</kbd>]<kbd>\`</kbd> for switching
  between windows of an app. These are hardcoded in
  [`src/WindowManager.cpp`](src/WindowManager.cpp)`:WindowManager::on_key_press(uint32_t, wl_keyboard_key_state)`.
  It is essential to keep these values compile time constants since this
  function is run on each keypress and release.

- The number of *quick access apps* (see [dispatchers](#dispatchers) below) can
  be set by generating the build system with `-DNUM_QUICK_ACCESS_APPS=<NUM>`.
  The default value is 10. This is a compile time constant to get static
  storage.
  
- Debug logs can be enabled by generating the build system with `-DDEBUG_LOGS=ON`.

## Dispatchers

- `exec`: A dispatcher to launch (a new window of) an app.

- `focusorexec`: A dispatcher to focus the last used window of an app or launch
  it.

- `moveorexec`: A dispatcher to focus the last used window of an app, moving it
  to the active workspace on the active monitor if needed, or launch it if there
  is no window.

## Configuration

Read [`lib.cpp`](src/lib.cpp) for supported configuration keys.

> [!NOTE]
>
> Sometimes, wallpaper colors are visible through the container blur instead of
> the window behind it. Not sure what causes this. Also, shadows in app icons do
> not look right. Functionality is not affected; PRs with fixes are welcome.

### Example

```hyprlang
plugin {
    wm {
        app_0 {
            class = kitty
            command = kitty
        }
        app_1 {
            class = org.kde.dolphin
            command = dolphin
        }
        app_switcher {
            container {
                padding = 20
            }
            selection {
                padding = 10
            }
            icons {
                size = 120
                separation = 40
            }
            label {
                font_color = rgba(ffffffff)
                font_size = 12
                separation = 10
            }
        }
    }
}

bind = SUPER,       0, wm:focusorexec, 0
bind = SUPER SHIFT, 0, wm:moveorexec,  0
bind = SUPER CTRL,  0, wm:exec,        0
bind = SUPER,       1, wm:focusorexec, 1
bind = SUPER SHIFT, 1, wm:moveorexec,  1
bind = SUPER CTRL,  1, wm:exec,        1
```

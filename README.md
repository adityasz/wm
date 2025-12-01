# WM

A Hyprland plugin that provides app and window switchers and some
dispatchers.

## Dispatchers

| Dispatcher    | Description                                                                                                                                 | Params |
|---------------|---------------------------------------------------------------------------------------------------------------------------------------------|--------|
| `exec`        | Launch a quick access app                                                                                                                   | `int`  |
| `focusorexec` | Focus the last used window of a quick access app or launch it                                                                               | `int`  |
| `moveorexec`  | Focus the last used window of an app, moving it to the active workspace on the active monitor if needed, or launch it if there is no window | `int`  |

## Installation

See
[`CMakeLists.txt`](https://github.com/adityasz/wm/blob/master/CMakeLists.txt)
for the full list of dependencies.

```console
$ cmake -B build/release -S . -GNinja -DCMAKE_BUILD_TYPE=Release
$ cmake --build build/release [-j <JOBS>]
$ hyprctl plugin load $(realpath build/release/libwm.so)
```

### Options

The following options are provided when generating the build system (i.e., in
the `cmake -B build -S .` command):

- `-DSWITCHER_MOD=<KEY>`: The modifier key for the app and window switcher
  [default: `KEY_LEFTMETA`]. See
  [`/usr/include/linux/input-event-codes.h`](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h)
  for the list of keys.

  Use [<kbd>shift</kbd>]<kbd>tab</kbd> for switching between applications, and
  [<kbd>shift</kbd>]<kbd>\`</kbd> for switching between windows of an app. These
  are hardcoded in
  [`src/WindowManager.cpp`](src/WindowManager.cpp)`:WindowManager::on_key_press(uint32_t, wl_keyboard_key_state)`.
  It is essential to keep these values compile time constants since this
  function is run on each keypress and release.

- `-DNUM_QUICK_ACCESS_APPS=<NUM>`: The number of *quick access apps* [default: 10]. This is a compile time
  constant to get static storage.

- `-DDEBUG_LOGS=ON`: Enable debug logs.

## Configuration

Read [`lib.cpp`](src/lib.cpp) for supported configuration keys.

TODO: markdown table

> [!NOTE]
>
> Shadows in app icons do not look right. Functionality is not affected;
> PRs with fixes are welcome.

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

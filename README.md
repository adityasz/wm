# WM

A Hyprland plugin that provides app and window switchers and some
dispatchers:

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
  [`lib/WindowManager/WindowManager.cpp`](lib/WindowManager/WindowManager.cpp)
  `:WindowManager::on_key_press(uint32_t, wl_keyboard_key_state)`.
  It is essential to keep these values compile time constants since this
  function is run on each keypress and release.

- `-DNUM_QUICK_ACCESS_APPS=<NUM>`: The number of *quick access apps* [default: 10]. This is a compile time
  constant to get static storage.

- `-DDEBUG_LOGS=ON`: Enable debug logs.

## Configuration

```hyprlang
plugin {
    wm {
        # Quick access apps
        # Format:
        #     app_<n> = <class>, <command>
        # where 0 <= n < NUM_QUICK_ACCESS_APPS
        # [default: app_<n> = "", "", for all n]
        # Example:
        app_0 = kitty, kitty
        app_1 = org.gnome.SystemMonitor, gnome-system-monitor
        
        # This is the default config of the app switcher
        app_switcher {
            container {
                background_color = rgba(ffffff11)
                border_color = rgba(80808011)
                padding = 20
                radius = 35
                border_width = 1
            }
            selection {
                background_color = rgba(00000011)
                padding = 10
                radius = 30
            }
            label {
                font_family = Inter
                font_color = rgba(ffffff)
                font_size = 0
                separation = 0
            }
            icons {
                size = 120
                separation = 40
                theme = ""
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

> [!NOTE]
>
> Shadows in app icons do not look right. Functionality is not affected;
> PRs with fixes are welcome.

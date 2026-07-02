# WM

A Hyprland plugin that improves window management.

## Installation

- Install [cxxmgen](https://github.com/adityasz/cxxmgen). Note that gcc does
  not like an export statement in a wrapper module because some Hyprland header
  does a `using identifier = (anonymous struct)` (instead of the usual `struct
  identifier {}` for some reason). Until I read the C++ standard again to figure
  out which compiler is following the standard correctly (and maybe patch
  cxxmgen if needed), use clang to build this project. (Modules speed up compile
  times a lot, and hence there is no reason to not use them[^1].)
- Generate the build system:
  ```console
  $ cmake --preset release-build   \
      -DCMAKE_C_COMPILER=clang     \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=<UUID>
  ```
  See the [CMake experimental features guide](https://github.com/Kitware/CMake/blob/master/Help/dev/experimental.rst)
  (for the right CMake version) to get the value for the `import std` gate.
- Compile: `cmake --build build/release -j [<jobs>]`.
- Install: `cmake --install build/release --prefix ~/.local` will install the
  plugin at `~/.local/lib/libwm.so`.
- Load it with `hl.plugin.load(os.getenv("HOME") .. "/.local/lib/libwm.so")`.

### Options

The following options can be set when generating the build system:

- `-DBETTER_FLOATING_BEHAVIOR=<ON|OFF>`: Hyprland hardcodes "floating windows
  always on top". This is strictly worse than having a dedicated property like
  `CWindow::m_alwaysOnTop` (it should be trivial to see why). This plugin hooks
  rendering and mouse input functions to treat `CCompositor::m_windows` as
  z-order. Disable hooks by turning off this option.

- `-DSWITCHER_MOD=<KEY>`: The modifier key to be held for using the app and
  window switchers [default for release build: `KEY_LEFTMETA`]. See
  [`/usr/include/linux/input-event-codes.h`](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h)
  for the list of keys.

  Use [<kbd>shift</kbd>]<kbd>tab</kbd> for switching between applications, and
  [<kbd>shift</kbd>]<kbd>\`</kbd> for switching between windows of an app. These
  are hardcoded in
  [`lib/WindowManager/WindowManager.cpp`](lib/WindowManager/WindowManager.cpp)`:WindowManager::on_key_press(IKeyboard::SKeyEvent e, Event::SCallbackInfo &info)`.
  It is essential to keep these values compile-time constants since this
  function is run on each keypress and release.

- `-DHASH_CHECK=<ON|OFF>`: Enable/disable hash check (useful when you apply non-ABI
  breaking patches to Hyprland and don't want to install headers again).

- `-DDEBUG_LOGS=<ON|OFF>`: Enable/disable debug logs.

## Configuration

Defaults:
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

### Dispatchers

- `wm.focus_or_exec({ class, command })`: Focus the last used window with
  `CWindow::m_initialClass` `class` or execute `command`.
  For example,
  `hl.bind("SUPER + 1", hl.plugin.wm.focus_or_exec({ class = "kitty", command = "runapp -o kitty" }))`.
- `wm.move_or_exec({ class, command })`: Focus the last used window with
  `CWindow::m_initialClass` `class` after moving it to the current workspace if
  it is not on the current workspace, or execute `command`.
  For example,
  `hl.bind("SUPER + SHIFT + 1", hl.plugin.wm.move_or_exec({ class = "kitty", command = "runapp -o kitty" }))`.
- `wm.fullscreen(mode, toggle?)`: `mode` can be `"maximized"` or `"fullscreen"`,
  and `toggle` is a boolean (`true` by default). `wm.fullscreen("disabled")` can
  be used to set the fullscreen state to `FSMODE_NONE`.
  For example, `hl.bind("SUPER + I", hl.plugin.wm.fullscreen("maximized"))`.

  It makes the window floating and resizes it to the size of a maximized window
  (before changing its fullscreen state). This allows maximizing multiple
  windows on the same workspace without having the last maximized window resize
  to its original size. When restoring a window, the previous size/tiling status
  is restored.

  Note that the tiled status of the window must not change if it was
  fullscreened using this plugin; use something like this from my config:
  ```lua
  if ENABLE_PLUGIN_WM then
      wm_bind("T", function()
          local w = hl.get_active_window()
          if not w then return end
          if w.fullscreen ~= 0 then
              hl.dispatch(hl.plugin.wm.fullscreen("disabled"))
              hl.dispatch(hl.dsp.window.float({ action = "unset" }))
              return
          end
          hl.dispatch(hl.dsp.window.float({ action = "toggle" }))
      end)
  else
      wm_bind("T", hl.dsp.window.float({ action = "toggle" }))
  end
  ```
  where `wm_bind` just prepends `SUPER +` to the keys and calls `hl.bind()`.
  (There is no practical reason to have a distinction between "floating and
  fullscreen" and "tiled and fullscreen" other than to work around limitations
  in the built-in layouts[^2].)

[Example binds](https://github.com/adityasz/dotfiles/blob/master/.config/hypr/keymap.lua).

[^1]: Incremental builds (modifying just a few `.cpp` files) can be several
times faster. Clean builds are also significantly sped up (even including the
time it takes to generate and build wrapper modules).

[^2]: When a window is untiled and tiled, due to a limitation in Hyprland's
built-in layouts, it may not go back to its previous spot. Also, Hyprland's
built-in layouts are rudimentary (a binary tree is the wrong design). There is
no way to tile a window in a certain direction, which is what is needed most of
the time. Ideally, one spends enough time on a fullscreen window to not care
about where it was tiled before it was fullscreened. Instead, one wants it to
appear next to a certain other window. I will write a tree at some point in the
future that fixes all of these issues (which only cost me one extra keypress a
few dozen times a day, but the frustration is enough that the tree in my head
will exist in code at some point).

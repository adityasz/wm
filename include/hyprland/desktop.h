#pragma once

#include <algorithm>
#include <any>
#include <chrono>
#include <queue>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#pragma GCC visibility push(default)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wkeyword-macro"

#define protected public
#define private   public
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/history/WindowHistoryTracker.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/desktop/state/ViewHitTester.hpp>
#include <hyprland/src/desktop/view/Group.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#undef private
#undef protected

#pragma GCC diagnostic pop

#pragma GCC visibility pop

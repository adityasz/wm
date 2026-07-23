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
#include <hyprland/src/event/EventBus.hpp>
#undef private
#undef protected

#pragma GCC diagnostic pop

#pragma GCC visibility pop

#pragma once

#include <algorithm>
#include <any>
#include <chrono>
#include <queue>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <hyprland/src/render/OpenGL.hpp>

#define private   public
#define protected public
#include <hyprland/src/render/Renderer.hpp>
#undef protected
#undef private

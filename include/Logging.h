#pragma once

#include "Hyprland.h"

import wm.Support.Logging;

#ifndef NDEBUG
#define LOG_TRACE(fmt, ...) log(LOG, "{}: " fmt, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...)                                                                       \
	do {                                                                                          \
	} while (0)
#endif

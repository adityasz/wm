#pragma once

// cannot add these things in global module fragment despite the preprocessor
// doing the exact same thing with the #include
// weird limitation; should probably be fixed in C++44
// and parser will somehow get even more black magic
typedef char gchar;
typedef int  gint;
typedef gint gboolean;
extern "C" {
#include "nkutils-xdg-theme.h"
}

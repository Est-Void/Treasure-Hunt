#ifndef TUI_H
#define TUI_H

#include <termbox2.h>

#include "game.h"

char map_symbol(const struct game *g, int y, int x);
void display_map(const struct game *g);
int run_tui(void);

#endif

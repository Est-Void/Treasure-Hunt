#define _DEFAULT_SOURCE 1
#define _XOPEN_SOURCE 700
#define TB_IMPL

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "tui.h"

#define MAP_X 2
#define MAP_Y 3
#define MAX_TEXT 80
#define LIST_PAGE 10

enum game_action { ACTION_MENU, ACTION_NEW, ACTION_QUIT };

static const char *status_text(enum game_status status) {
  if (status == STATUS_WIN)
    return "WIN";
  if (status == STATUS_LOSS)
    return "LOSS";
  return "PLAYING";
}

char map_symbol(const struct game *g, int y, int x) {
  if (x == g->x && y == g->y)
    return 'P';
  if (g->status == STATUS_PLAYING && !g->discovered[y][x])
    return '?';
  return cell_symbol(g->map[y][x]);
}

void display_map(const struct game *g) {
  for (int y = 0; y < MAP_SIZE; y++) {
    for (int x = 0; x < MAP_SIZE; x++) {
      tb_set_cell(MAP_X + x, MAP_Y + y, map_symbol(g, y, x), TB_DEFAULT,
                  TB_DEFAULT);
    }
  }
}

static void print_message(const char *message) {
  int line = 0;
  const char *part = message;

  while (*part != '\0') {
    const char *end = strchr(part, '\n');
    int len = end != NULL ? (int)(end - part) : (int)strlen(part);
    char buf[96];
    if (len > (int)sizeof(buf) - 1)
      len = (int)sizeof(buf) - 1;
    snprintf(buf, sizeof(buf), "%.*s", len, part);
    tb_print(2, MAP_Y + MAP_SIZE + 2 + line, TB_DEFAULT, TB_DEFAULT, buf);
    line++;
    if (end == NULL)
      break;
    part = end + 1;
  }
}

static void render_screen(const struct game *g, const char *message) {
  tb_clear();
  if (tb_width() < 80 || tb_height() < MAP_Y + MAP_SIZE + 6) {
    tb_print(1, 1, TB_DEFAULT, TB_DEFAULT, "Terminal is too small");
    tb_present();
    return;
  }

  tb_printf(2, 0, TB_DEFAULT, TB_DEFAULT, "Treasure Hunt - %s (id %u)", g->name,
            g->id);
  tb_printf(
      2, 1, TB_DEFAULT, TB_DEFAULT,
      "Lives: %d  Moves left: %d  Treasures: %d/%d  Status: %s  Pos: (%d,%d)",
      g->lives, g->moves, g->treasures, TREASURE_NUM, status_text(g->status),
      g->x, g->y);
  display_map(g);
  if (g->status == STATUS_PLAYING)
    tb_print(2, MAP_Y + MAP_SIZE + 1, TB_DEFAULT, TB_DEFAULT,
             "WASD - move, Q - save, Esc - menu");
  else
    tb_print(2, MAP_Y + MAP_SIZE + 1, TB_DEFAULT, TB_DEFAULT,
             "Game over - choose below");
  if (message != NULL)
    print_message(message);
  tb_present();
}

static void wait_enter_esc(void) {
  for (;;) {
    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY)
      continue;
    if (ev.key == TB_KEY_ENTER || ev.key == TB_KEY_ESC)
      return;
  }
}

static void show_message(const char *text) {
  tb_clear();
  tb_print(2, 3, TB_DEFAULT, TB_DEFAULT, text);
  tb_print(2, 5, TB_DEFAULT, TB_DEFAULT, "Enter/Esc - back");
  tb_present();
  wait_enter_esc();
}

static int read_text(const char *prompt, char *buf, size_t size,
                     int digits_only) {
  size_t len = 0;
  buf[0] = '\0';

  for (;;) {
    tb_clear();
    if (tb_width() < 40 || tb_height() < 8) {
      tb_print(1, 1, TB_DEFAULT, TB_DEFAULT, "Terminal is too small");
    } else {
      tb_printf(2, 2, TB_DEFAULT, TB_DEFAULT, "%s: %s_", prompt, buf);
      tb_print(2, 4, TB_DEFAULT, TB_DEFAULT, "Enter - confirm, Esc - back");
    }
    tb_present();

    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY)
      continue;
    if (ev.key == TB_KEY_ESC)
      return 0;
    if (ev.key == TB_KEY_ENTER)
      return 1;
    if ((ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) &&
        len > 0) {
      buf[--len] = '\0';
      continue;
    }
    if (len + 1 >= size)
      continue;
    if (digits_only) {
      if (ev.ch < '0' || ev.ch > '9')
        continue;
    } else {
      if (ev.ch < ' ' || ev.ch > '~' || ev.ch == '|')
        continue;
    }
    buf[len++] = (char)ev.ch;
    buf[len] = '\0';
  }
}

static int read_name(char *name, size_t size) {
  if (!read_text("Enter your name", name, size, 0))
    return 0;
  if (name[0] == '\0')
    snprintf(name, size, "Player");
  return 1;
}

static int read_id(unsigned int *id) {
  char buf[16];
  if (!read_text("Enter game ID", buf, sizeof(buf), 1))
    return 0;
  if (buf[0] == '\0')
    return 0;

  unsigned long value = strtoul(buf, NULL, 10);
  if (value == 0 || value > UINT_MAX)
    return 0;
  *id = (unsigned int)value;
  return 1;
}

static void show_list(const char *title, char lines[][MAX_TEXT], int count) {
  int pages = (count + LIST_PAGE - 1) / LIST_PAGE;
  if (pages < 1)
    pages = 1;
  int page = 0;

  for (;;) {
    tb_clear();
    tb_print(2, 1, TB_DEFAULT, TB_DEFAULT, title);
    if (count == 0)
      tb_print(2, 3, TB_DEFAULT, TB_DEFAULT, "No saved games.");
    for (int i = 0; i < LIST_PAGE && page * LIST_PAGE + i < count; i++)
      tb_print(2, 3 + i, TB_DEFAULT, TB_DEFAULT, lines[page * LIST_PAGE + i]);
    tb_printf(2, 14, TB_DEFAULT, TB_DEFAULT,
              "Page %d/%d  Up/Down - pages  Enter/Esc - back", page + 1, pages);
    tb_present();

    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY)
      continue;
    if (ev.key == TB_KEY_ENTER || ev.key == TB_KEY_ESC)
      return;
    if ((ev.key == TB_KEY_ARROW_DOWN || ev.ch == 's' || ev.ch == 'S') &&
        page + 1 < pages)
      page++;
    if ((ev.key == TB_KEY_ARROW_UP || ev.ch == 'w' || ev.ch == 'W') && page > 0)
      page--;
  }
}

static int show_main_menu(void) {
  static const char *items[] = {"Start new game", "Load saved game", "Exit"};
  int selected = 0;

  for (;;) {
    tb_clear();
    tb_print(2, 2, TB_DEFAULT, TB_DEFAULT, "TREASURE HUNT");
    for (int i = 0; i < 3; i++) {
      uintattr_t fg = i == selected ? TB_REVERSE : TB_DEFAULT;
      tb_printf(4, 5 + i * 2, fg, TB_DEFAULT, "%d. %s", i + 1, items[i]);
    }
    tb_print(2, 13, TB_DEFAULT, TB_DEFAULT,
             "Up/Down - select, Enter - choose, digits - quick choice");
    tb_present();

    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY)
      continue;
    if (ev.key == TB_KEY_ESC)
      return 3;
    if (ev.key == TB_KEY_ENTER)
      return selected + 1;
    if (ev.ch >= '1' && ev.ch <= '3')
      return ev.ch - '0';
    if (ev.key == TB_KEY_ARROW_DOWN || ev.ch == 's' || ev.ch == 'S')
      selected = (selected + 1) % 3;
    if (ev.key == TB_KEY_ARROW_UP || ev.ch == 'w' || ev.ch == 'W')
      selected = (selected + 2) % 3;
  }
}

static int load_menu(struct game *g) {
  static const char *items[] = {
      "List all saved games", "List saved games for player",
      "Show map of saved game", "Load a saved game", "Return to main menu"};
  int selected = 0;

  for (;;) {
    tb_clear();
    tb_print(2, 2, TB_DEFAULT, TB_DEFAULT, "Load game");
    for (int i = 0; i < 5; i++) {
      uintattr_t fg = i == selected ? TB_REVERSE : TB_DEFAULT;
      tb_printf(4, 4 + i * 2, fg, TB_DEFAULT, "%d. %s", i + 1, items[i]);
    }
    tb_print(2, 16, TB_DEFAULT, TB_DEFAULT,
             "Up/Down - select, Enter - choose, digits - quick choice");
    tb_present();

    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY)
      continue;
    if (ev.key == TB_KEY_ESC)
      return 0;

    int action;
    if (ev.key == TB_KEY_ENTER) {
      action = selected + 1;
    } else if (ev.ch >= '1' && ev.ch <= '5') {
      action = ev.ch - '0';
    } else {
      if (ev.key == TB_KEY_ARROW_DOWN || ev.ch == 's' || ev.ch == 'S')
        selected = (selected + 1) % 5;
      if (ev.key == TB_KEY_ARROW_UP || ev.ch == 'w' || ev.ch == 'W')
        selected = (selected + 4) % 5;
      continue;
    }

    if (action == 5)
      return 0;

    if (action == 1 || action == 2) {
      struct save_summary saves[MAX_SAVED_GAMES];
      char lines[MAX_SAVED_GAMES + 1][MAX_TEXT];
      int count = get_saved_games(saves, MAX_SAVED_GAMES);
      char player[32];

      if (action == 2) {
        if (!read_text("Player name", player, sizeof(player), 0))
          continue;
        int filtered = 0;
        for (int i = 0; i < count; i++) {
          if (strcmp(saves[i].name, player) != 0)
            continue;
          saves[filtered] = saves[i];
          filtered++;
        }
        count = filtered;
      }
      snprintf(lines[0], sizeof(lines[0]),
               "ID, Player name, Collected treasures, Remaining lives, "
               "Remaining moves");
      for (int i = 0; i < count; i++) {
        snprintf(lines[i + 1], sizeof(lines[i + 1]), "%u, %s, %d, %d, %d",
                 saves[i].id, saves[i].name, saves[i].treasures, saves[i].lives,
                 saves[i].moves);
      }
      show_list(action == 1 ? "All saved games" : "Saved games for player",
                lines, count + 1);
      continue;
    }

    unsigned int id;
    if (!read_id(&id))
      continue;
    struct game saved;
    if (!load_game(id, &saved)) {
      show_message("Game not found.");
      continue;
    }
    if (action == 4) {
      *g = saved;
      return 1;
    }

    render_screen(&saved, "Saved game. Enter/Esc - back");
    wait_enter_esc();
  }
}

static char move_from_event(const struct tb_event *ev) {
  // свичи и кейсы для движения
  switch (ev->ch) {
  case 'w':
  case 'W':
    return 'w';
  case 's':
  case 'S':
    return 's';
  case 'a':
  case 'A':
    return 'a';
  case 'd':
  case 'D':
    return 'd';
  default:
    break;
  }

  switch (ev->key) {
  case TB_KEY_ARROW_UP:
    return 'w';
  case TB_KEY_ARROW_DOWN:
    return 's';
  case TB_KEY_ARROW_LEFT:
    return 'a';
  case TB_KEY_ARROW_RIGHT:
    return 'd';
  default:
    return 0;
  }
}

// gameloop логика по факту, надо бы вынести в отдельный файл, но хз
static int play_game(struct game *g) {
  char notice[128];
  notice[0] = '\0';
  int result_saved = (g->status != STATUS_PLAYING);

  for (;;) {
    if (g->status != STATUS_PLAYING && !result_saved) {
      save_result(g);
      result_saved = 1;
    }

    char message[160];
    if (g->status == STATUS_PLAYING) {
      snprintf(message, sizeof(message), "%s", notice);
    } else {
      snprintf(message, sizeof(message),
               "Game over: %s. Moves used: %d, treasures: %d/%d.\n"
               "1 / N - start a new game\n"
               "2 / M / Esc - return to the main menu",
               status_text(g->status), MAX_MOVES - g->moves, g->treasures,
               TREASURE_NUM);
    }
    render_screen(g, message[0] != '\0' ? message : NULL);

    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK || ev.type != TB_EVENT_KEY)
      continue;

    if (ev.key == TB_KEY_ESC)
      return ACTION_MENU;
    if (g->status != STATUS_PLAYING) {
      if (ev.ch == '1' || ev.ch == 'n' || ev.ch == 'N')
        return ACTION_NEW;
      if (ev.ch == '2' || ev.ch == 'm' || ev.ch == 'M')
        return ACTION_MENU;
      continue;
    }
    if (ev.ch != 'q' && ev.ch != 'Q')
      notice[0] = '\0';

    if (ev.ch == 'q' || ev.ch == 'Q') {
      save_game(g);
      snprintf(notice, sizeof(notice), "Saved. Game ID: %u (Q - save again)",
               g->id);
      continue;
    }

    char dir = move_from_event(&ev);
    if (dir != 0)
      move_player(g, dir);
  }
}

static int start_new_game(void) {
  char name[32];
  if (!read_name(name, sizeof(name)))
    return ACTION_MENU;

  struct game g;
  init_game(&g, name, next_game_id());
  generate_map(&g);
  return play_game(&g);
}

static int run_loaded_game(void) {
  struct game g;
  if (!load_menu(&g))
    return ACTION_MENU;
  return play_game(&g);
}

// тут запускаем ТУЙ
int run_tui(void) {
  if (tb_init() != TB_OK)
    return 1;

  srand((unsigned)time(NULL));
  tb_hide_cursor();

  for (;;) {
    int choice = show_main_menu();
    if (choice == 3)
      break;

    int action = choice == 1 ? start_new_game() : run_loaded_game();
    while (action == ACTION_NEW)
      action = start_new_game();
    if (action == ACTION_QUIT)
      break;
  }

  tb_shutdown();
  return 0;
}

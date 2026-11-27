#ifndef GAME_H
#define GAME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// тута настройки карты
#define MAP_SIZE 10

#define OBSTACLE_NUM 8
#define TREASURE_NUM 6
#define TRAP_NUM 5
#define MAX_MOVES 40
#define MAX_SAVED_GAMES 50

#define SAVE_FILE "saved_games.txt"
#define RESULT_FILE "results.txt"

// тута всякие объекты на карте

enum cell { CELL_EMPTY, CELL_TREASURE, CELL_TRAP, CELL_OBSTACLE, CELL_EXIT };

enum game_status { STATUS_PLAYING, STATUS_WIN, STATUS_LOSS };

struct game {
  char name[32];
  unsigned int id;

  enum cell map[MAP_SIZE][MAP_SIZE];
  int discovered[MAP_SIZE][MAP_SIZE];

  int x, y;
  int lives, moves, treasures;

  enum game_status status;
};

struct save_summary {
  unsigned int id;
  char name[32];
  int treasures, lives, moves;
};

// тута функции что мы будем делать

void init_game(struct game *g, const char *name, unsigned int id);
void generate_map(struct game *g);
int is_path_available(const struct game *g);
int move_player(struct game *g, char dir);
void check_game_status(struct game *g);
char cell_symbol(enum cell cell);

unsigned int next_game_id(void);
void save_game(const struct game *g);
int load_game(unsigned int id, struct game *g);
int get_saved_games(struct save_summary *out, int max);
void save_result(const struct game *g);

#endif

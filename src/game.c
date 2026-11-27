
#include "game.h"

void init_game(struct game *g, const char *name, unsigned int id) {
  g->name[0] = '\0';
  if (name != NULL) {
    size_t i = 0;
    while (name[i] != '\0' && i + 1 < sizeof(g->name)) {
      g->name[i] = name[i];
      i++;
    }
    g->name[i] = '\0';
  }

  g->id = id;
  g->x = 0;
  g->y = 0;
  g->lives = 3;
  g->moves = MAX_MOVES;
  g->treasures = 0;
  g->status = STATUS_PLAYING;

  for (int y = 0; y < MAP_SIZE; y++) {
    for (int x = 0; x < MAP_SIZE; x++) {
      g->map[y][x] = CELL_EMPTY;
      g->discovered[y][x] = 0;
    }
  }
  g->discovered[0][0] = 1;
}

static void place_random(struct game *g, enum cell cell_type, int count) {
  while (count > 0) {
    // координата всегда в пределах мапы
    int x = rand() % MAP_SIZE;
    int y = rand() % MAP_SIZE;
    if (g->map[y][x] != CELL_EMPTY)
      continue;

    g->map[y][x] = cell_type;
    count--;
  }
}

int is_path_available(const struct game *g) {
  int seen[MAP_SIZE][MAP_SIZE] = {0};
  int qr[MAP_SIZE * MAP_SIZE], qc[MAP_SIZE * MAP_SIZE];
  int h = 0, t = 0;
  qr[t] = 0;
  qc[t] = 0;
  t++;
  seen[0][0] = 1;

  const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
  while (h < t) {
    int r = qr[h], c = qc[h];
    h++;
    for (int i = 0; i < 4; i++) {
      int nr = r + dr[i], nc = c + dc[i];
      if (nr < 0 || nr >= MAP_SIZE || nc < 0 || nc >= MAP_SIZE)
        continue;
      if (seen[nr][nc] || g->map[nr][nc] == CELL_OBSTACLE)
        continue;
      seen[nr][nc] = 1;
      qr[t] = nr;
      qc[t] = nc;
      t++;
    }
  }

  // до выхода и до каждого сокровища должен быть путь,
  // иначе игру невозможно пройти
  if (!seen[MAP_SIZE - 1][MAP_SIZE - 1])
    return 0;
  for (int y = 0; y < MAP_SIZE; y++) {
    for (int x = 0; x < MAP_SIZE; x++) {
      if (g->map[y][x] == CELL_TREASURE && !seen[y][x])
        return 0;
    }
  }
  return 1;
}

static void reveal_neighbor_obstacles(struct game *g) {
  // на препятствие наступить нельзя, поэтому показываем соседние # сразу
  const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
  for (int i = 0; i < 4; i++) {
    int ax = g->x + dc[i], ay = g->y + dr[i];
    if (ax >= 0 && ax < MAP_SIZE && ay >= 0 && ay < MAP_SIZE &&
        g->map[ay][ax] == CELL_OBSTACLE)
      g->discovered[ay][ax] = 1;
  }
}

void generate_map(struct game *g) {
  // инициируем пустую мапу и заполняет
  do {
    for (int y = 0; y < MAP_SIZE; y++) {
      for (int x = 0; x < MAP_SIZE; x++)
        g->map[y][x] = CELL_EMPTY;
    }
    g->map[MAP_SIZE - 1][MAP_SIZE - 1] = CELL_EXIT;

    place_random(g, CELL_OBSTACLE, OBSTACLE_NUM);
    place_random(g, CELL_TRAP, TRAP_NUM);
    place_random(g, CELL_TREASURE, TREASURE_NUM);
  } while (!is_path_available(g));

  reveal_neighbor_obstacles(g);
}

int move_player(struct game *g, char dir) {
  if (g->status != STATUS_PLAYING)
    return 0;
  int nx = g->x, ny = g->y;
  // смотрим путь, делаем перемещение координаты
  if (dir == 'w' || dir == 'W')
    ny--;
  else if (dir == 's' || dir == 'S')
    ny++;
  else if (dir == 'a' || dir == 'A')
    nx--;
  else if (dir == 'd' || dir == 'D')
    nx++;
  else
    return 0;

  // проверка на дурака
  if (nx < 0 || nx >= MAP_SIZE || ny < 0 || ny >= MAP_SIZE)
    return 0;
  // проверка на стенку
  if (g->map[ny][nx] == CELL_OBSTACLE)
    return 0;

  g->x = nx;
  g->y = ny;
  g->discovered[ny][nx] = 1;
  reveal_neighbor_obstacles(g);

  if (g->map[ny][nx] == CELL_TREASURE) {
    g->treasures++;
    g->map[ny][nx] = CELL_EMPTY;
  } else if (g->map[ny][nx] == CELL_TRAP) {
    g->lives--;
    g->map[ny][nx] = CELL_EMPTY;
  }

  g->moves--;
  check_game_status(g);
  return 1;
}

void check_game_status(struct game *g) {
  if (g->treasures == TREASURE_NUM && g->map[g->y][g->x] == CELL_EXIT) {
    g->status = STATUS_WIN;
    return;
  }
  if (g->lives <= 0 || g->moves <= 0)
    g->status = STATUS_LOSS;
}

char cell_symbol(enum cell cell) {
  switch (cell) {
  case CELL_EMPTY:
    return '.';
  case CELL_TREASURE:
    return 'T';
  case CELL_TRAP:
    return 'X';
  case CELL_OBSTACLE:
    return '#';
  case CELL_EXIT:
    return 'E';
  }
  return '.';
}

static int cell_from_symbol(char c, enum cell *cell) {
  switch (c) {
  case '.':
    *cell = CELL_EMPTY;
    return 1;
  case 'T':
    *cell = CELL_TREASURE;
    return 1;
  case 'X':
    *cell = CELL_TRAP;
    return 1;
  case '#':
    *cell = CELL_OBSTACLE;
    return 1;
  case 'E':
    *cell = CELL_EXIT;
    return 1;
  }
  return 0;
}

static int parse_game_line(const char *line, struct game *g) {
  char name[32], map[MAP_SIZE * MAP_SIZE + 1],
      discovered[MAP_SIZE * MAP_SIZE + 1];
  unsigned int id;
  int x, y, lives, moves, treasures, status;

  if (sscanf(line, "%u|%31[^|]|%d|%d|%d|%d|%d|%d|%100[^|]|%100s", &id, name, &x,
             &y, &lives, &moves, &treasures, &status, map, discovered) != 10)
    return 0;
  if (strlen(map) != MAP_SIZE * MAP_SIZE ||
      strlen(discovered) != MAP_SIZE * MAP_SIZE)
    return 0;
  if (x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE)
    return 0;
  if (status < STATUS_PLAYING || status > STATUS_LOSS)
    return 0;

  g->id = id;
  strcpy(g->name, name);
  g->x = x;
  g->y = y;
  g->lives = lives;
  g->moves = moves;
  g->treasures = treasures;
  g->status = (enum game_status)status;

  for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++) {
    int cell_y = i / MAP_SIZE, cell_x = i % MAP_SIZE;
    enum cell cell;
    if (!cell_from_symbol(map[i], &cell))
      return 0;
    g->map[cell_y][cell_x] = cell;
    if (discovered[i] == '0')
      g->discovered[cell_y][cell_x] = 0;
    else if (discovered[i] == '1')
      g->discovered[cell_y][cell_x] = 1;
    else
      return 0;
  }
  return 1;
}

static void write_game(FILE *f, const struct game *g) {
  fprintf(f, "%u|%s|%d|%d|%d|%d|%d|%d|", g->id, g->name, g->x, g->y, g->lives,
          g->moves, g->treasures, g->status);
  for (int y = 0; y < MAP_SIZE; y++) {
    for (int x = 0; x < MAP_SIZE; x++)
      fputc(cell_symbol(g->map[y][x]), f);
  }
  fputc('|', f);
  for (int y = 0; y < MAP_SIZE; y++) {
    for (int x = 0; x < MAP_SIZE; x++)
      fputc(g->discovered[y][x] ? '1' : '0', f);
  }
  fputc('\n', f);
}

static unsigned int max_id_in(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL)
    return 0;

  char line[512];
  unsigned int max_id = 0, id;
  while (fgets(line, sizeof(line), f) != NULL) {
    if (sscanf(line, "%u|", &id) == 1 && id > max_id)
      max_id = id;
  }
  fclose(f);
  return max_id;
}

unsigned int next_game_id(void) {
  unsigned int from_saves = max_id_in(SAVE_FILE);
  unsigned int from_results = max_id_in(RESULT_FILE);
  return (from_saves > from_results ? from_saves : from_results) + 1;
}

void save_game(const struct game *g) {
  struct game games[MAX_SAVED_GAMES];
  int count = 0;

  FILE *f = fopen(SAVE_FILE, "r");
  if (f != NULL) {
    char line[512];
    while (count < MAX_SAVED_GAMES && fgets(line, sizeof(line), f) != NULL) {
      struct game saved;
      if (parse_game_line(line, &saved))
        games[count++] = saved;
    }
    fclose(f);
  }

  int found = 0;
  for (int i = 0; i < count; i++) {
    if (games[i].id == g->id) {
      games[i] = *g;
      found = 1;
      break;
    }
  }
  if (!found && count < MAX_SAVED_GAMES)
    games[count++] = *g;

  f = fopen(SAVE_FILE, "w");
  if (f == NULL)
    return;
  for (int i = 0; i < count; i++)
    write_game(f, &games[i]);
  fclose(f);
}

int load_game(unsigned int id, struct game *g) {
  FILE *f = fopen(SAVE_FILE, "r");
  if (f == NULL)
    return 0;

  char line[512];
  int found = 0;
  while (fgets(line, sizeof(line), f) != NULL) {
    struct game saved;
    if (parse_game_line(line, &saved) && saved.id == id) {
      *g = saved;
      found = 1;
      break;
    }
  }
  fclose(f);
  return found;
}

int get_saved_games(struct save_summary *out, int max) {
  FILE *f = fopen(SAVE_FILE, "r");
  if (f == NULL)
    return 0;

  char line[512];
  int count = 0;
  while (count < max && fgets(line, sizeof(line), f) != NULL) {
    struct game saved;
    if (!parse_game_line(line, &saved))
      continue;
    out[count].id = saved.id;
    strcpy(out[count].name, saved.name);
    out[count].treasures = saved.treasures;
    out[count].lives = saved.lives;
    out[count].moves = saved.moves;
    count++;
  }
  fclose(f);
  return count;
}

void save_result(const struct game *g) {
  FILE *f = fopen(RESULT_FILE, "a");
  if (f == NULL)
    return;

  fprintf(f, "%u|%s|%s|%d|%d\n", g->id, g->name,
          g->status == STATUS_WIN ? "WIN" : "LOSS", MAX_MOVES - g->moves,
          g->treasures);
  fclose(f);
}

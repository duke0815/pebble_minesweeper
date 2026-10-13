/*
 * Minesweeper fuer Pebble Time 2 (emery) – laeuft auch auf basalt/diorite/flint.
 *
 * Tasten:
 *   UP                   Cursor nach rechts (horizontal, mit Umbruch in der Zeile)
 *   DOWN                 Cursor nach unten (vertikal, mit Umbruch in der Spalte)
 *   UP / DOWN halten     Cursor laeuft automatisch weiter
 *   SELECT               Feld aufdecken (auf Zahl: Nachbarn aufdecken)
 *   SELECT halten        Faehnchen setzen / entfernen
 *   BACK                 zurueck ins Menue
 *
 * Touch (Pebble Time 2, PBL_TOUCH):
 *   Feld tippen          aufdecken
 *   Feld halten          Faehnchen setzen / entfernen
 *   Kopfzeile tippen     Zoom an / aus
 *   Ziehen               im Zoom den Ausschnitt verschieben
 *   (Pinch-Zoom ist nicht moeglich: die Touch-API liefert nur einen Beruehrungspunkt.)
 *
 * Nach jedem Spiel: Animation -> Highscore-Liste -> End-Menue (Nochmal / Beenden = Hauptmenue).
 */
#include <pebble.h>

#define GRID_W 8
#define GRID_H 8
#define CELLS (GRID_W * GRID_H)
#define NUM_LEVELS 3
#define HS_COUNT 5
#define HS_EMPTY 0xFFFF

#define PKEY_BACKLIGHT 1
#define PKEY_HS_BASE 10

#define C_COUNT 0x0F
#define C_MINE 0x10
#define C_OPEN 0x20
#define C_FLAG 0x40

#define ANIM_MS 40
#define LOSE_FRAMES 50
#define WIN_FRAMES 60
#define RECORD_FRAMES 80
#define ANIM_SKIP_AFTER 12
#define CURSOR_REPEAT_MS 250
#define NUM_PARTICLES 40
#define LONG_PRESS_MS 450
#define TOUCH_SLOP 8

#define ABS(x) ((x) < 0 ? -(x) : (x))

static const uint8_t LEVEL_MINES[NUM_LEVELS] = {6, 10, 14};

typedef enum {
  ST_READY,       // Brett leer, Minen werden beim ersten Aufdecken gelegt
  ST_PLAYING,
  ST_ANIM_LOSE,
  ST_ANIM_WIN,
  ST_ANIM_RECORD,
  ST_DONE,        // End-Menue
} GameState;

// ---------------------------------------------------------------------------
// Texte: Deutsch bei deutscher Uhr-Sprache, sonst Englisch
// ---------------------------------------------------------------------------
typedef enum {
  S_EASY,
  S_MEDIUM,
  S_HARD,
  S_RETRY,
  S_QUIT,
  S_WON_BANNER,
  S_TIME_FMT,
  S_RECORD_BANNER,
  S_RECORD_LINE_FMT,
  S_LOST,
  S_NEW_RECORD,
  S_WON,
  S_LOST_AFTER_FMT,
  S_RANK_FMT,
  S_KEY_HOLD,
  S_ACT_RIGHT,
  S_ACT_DOWN,
  S_ACT_OPEN,
  S_ACT_FLAG,
  S_ACT_MENU,
  S_PRESS_BACK,
  S_HS_LOST,
  S_HS_TIME_FMT,
  S_BACKLIGHT,
  S_ON,
  S_OFF,
  S_HDR_NEW,
  S_HDR_MORE,
  S_LEVEL_SUB_FMT,
  S_HIGHSCORES,
  S_SETTINGS,
  S_CONTROLS,
  S_TOUCH_TAP,
  S_TOUCH_HOLD,
  S_TOUCH_TOPBAR,
  S_TOUCH_DRAG,
  S_ACT_ZOOM,
  S_ACT_PAN,
  S_COUNT,
} StrId;

static const char *const STR_DE[S_COUNT] = {
  "Leicht",
  "Mittel",
  "Schwer",
  "Nochmal",
  "Beenden",
  "GEWONNEN!",
  "Zeit: %s",
  "NEUER REKORD!",
  "%s  -  Platz %d  -  %s",
  "Verloren!",
  "Neuer Rekord!",
  "Gewonnen!",
  "%s - nach %s",
  "Platz %d - %s",
  "... halten",
  "nach rechts",
  "nach unten",
  "Feld öffnen",
  "Fähnchen",
  "Menü",
  "Taste drücken für zurück",
  "Verloren - Taste: weiter",
  "Zeit: %s - Taste: weiter",
  "Display immer an",
  "AN",
  "AUS",
  "Neues Spiel - Schwierigkeit",
  "Mehr",
  "%d Bomben | Rekord %s",
  "Highscores",
  "Einstellungen",
  "Steuerung",
  "Tippen",
  "Halten",
  "Kopfzeile",
  "Ziehen",
  "Zoom an/aus",
  "verschieben",
};

static const char *const STR_EN[S_COUNT] = {
  "Easy",
  "Medium",
  "Hard",
  "Play again",
  "Quit",
  "YOU WIN!",
  "Time: %s",
  "NEW RECORD!",
  "%s  -  Rank %d  -  %s",
  "Game over!",
  "New record!",
  "You win!",
  "%s - after %s",
  "Rank %d - %s",
  "... hold",
  "move right",
  "move down",
  "open cell",
  "flag",
  "menu",
  "Press any button",
  "Game over - press a key",
  "Time: %s - press a key",
  "Backlight always on",
  "ON",
  "OFF",
  "New game - difficulty",
  "More",
  "%d mines | Best %s",
  "Highscores",
  "Settings",
  "Controls",
  "Tap",
  "Hold",
  "Top bar",
  "Drag",
  "zoom in/out",
  "pan",
};

static bool s_german;

static const char *T(StrId id) {
  return s_german ? STR_DE[id] : STR_EN[id];
}

typedef enum { END_RETRY = 0, END_QUIT, END_COUNT } EndOption;
static const StrId END_LABELS[END_COUNT] = {S_RETRY, S_QUIT};

// ---------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------
static Window *s_menu_window, *s_intro_window, *s_game_window, *s_hs_window, *s_settings_window;
static MenuLayer *s_menu_layer, *s_settings_layer;
static Layer *s_intro_layer, *s_game_layer, *s_hs_layer;
static AppTimer *s_anim_timer;
static bool s_hs_from_game;  // Highscore-Liste nach Spielende

static bool s_backlight;
static uint16_t s_hs[NUM_LEVELS][HS_COUNT];

static uint8_t s_cells[CELLS];
static int s_level, s_cursor, s_opened, s_flags, s_exploded, s_rank, s_end_sel;
static GameState s_state;
static bool s_lost;
static int64_t s_start_ms;
static int s_seconds;
static int s_anim_frame;

// Layout
static int s_cs, s_ox, s_oy, s_hh;
static bool s_big;

// Zoom (nur mit Touchscreen): groessere Felder, Ausschnitt per Ziehen verschiebbar
static bool s_touch;          // Touchscreen vorhanden und aktiv
static bool s_zoom;
static int s_zs;              // Feldgroesse im Zoom
static int s_vx, s_vy;        // Ausschnitt-Versatz im gezoomten Brett (Pixel)

#ifdef PBL_TOUCH
// Touch-Gesten
static bool s_touch_down, s_touch_dragged, s_touch_long_fired;
static GPoint s_touch_start, s_touch_last;
static int s_touch_cell, s_cursor_before_touch;
static AppTimer *s_long_timer;
#endif

// Partikel (Explosion / Konfetti)
static int16_t s_px[NUM_PARTICLES], s_py[NUM_PARTICLES], s_pvx[NUM_PARTICLES], s_pvy[NUM_PARTICLES];
static uint8_t s_pcol[NUM_PARTICLES];

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------
static int64_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (int64_t)s * 1000 + ms;
}

static void fmt_time(char *buf, size_t size, int secs) {
  if (secs >= 60) {
    snprintf(buf, size, "%d:%02d", secs / 60, secs % 60);
  } else {
    snprintf(buf, size, "%ds", secs);
  }
}

static int current_seconds(void) {
  if (s_state == ST_READY) return 0;
  if (s_state == ST_PLAYING) return (int)((now_ms() - s_start_ms) / 1000);
  return s_seconds;
}

static GColor num_color(int n) {
#ifdef PBL_COLOR
  switch (n) {
    case 1: return GColorBlue;
    case 2: return GColorIslamicGreen;
    case 3: return GColorRed;
    case 4: return GColorOxfordBlue;
    case 5: return GColorBulgarianRose;
    case 6: return GColorTiffanyBlue;
    case 7: return GColorBlack;
    default: return GColorDarkGray;
  }
#else
  return GColorBlack;
#endif
}

static GColor particle_color(int i) {
#ifdef PBL_COLOR
  switch (i % 7) {
    case 0: return GColorRed;
    case 1: return GColorYellow;
    case 2: return GColorGreen;
    case 3: return GColorBlueMoon;
    case 4: return GColorMagenta;
    case 5: return GColorOrange;
    default: return GColorCyan;
  }
#else
  return GColorBlack;
#endif
}

// ---------------------------------------------------------------------------
// Persistenz: Einstellungen & Highscores
// ---------------------------------------------------------------------------
static void apply_backlight(void) {
  light_enable(s_backlight);
}

static void load_data(void) {
  s_backlight = persist_exists(PKEY_BACKLIGHT) ? persist_read_bool(PKEY_BACKLIGHT) : false;
  for (int l = 0; l < NUM_LEVELS; l++) {
    for (int i = 0; i < HS_COUNT; i++) s_hs[l][i] = HS_EMPTY;
    if (persist_exists(PKEY_HS_BASE + l)) {
      persist_read_data(PKEY_HS_BASE + l, s_hs[l], sizeof(s_hs[l]));
    }
  }
}

// Traegt eine Zeit ein, liefert den Platz (0-basiert) oder -1.
static int hs_insert(int level, int secs) {
  if (secs > 0xFFFE) secs = 0xFFFE;
  int rank = -1;
  for (int i = 0; i < HS_COUNT; i++) {
    if (secs < s_hs[level][i]) { rank = i; break; }
  }
  if (rank < 0) return -1;
  for (int i = HS_COUNT - 1; i > rank; i--) s_hs[level][i] = s_hs[level][i - 1];
  s_hs[level][rank] = (uint16_t)secs;
  persist_write_data(PKEY_HS_BASE + level, s_hs[level], sizeof(s_hs[level]));
  return rank;
}

// ---------------------------------------------------------------------------
// Zeichen-Primitive
// ---------------------------------------------------------------------------
static void draw_mine(GContext *ctx, GPoint c, int r) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  int s = r + r / 2 + 1;
  int d = r + 1;
  graphics_draw_line(ctx, GPoint(c.x - s, c.y), GPoint(c.x + s, c.y));
  graphics_draw_line(ctx, GPoint(c.x, c.y - s), GPoint(c.x, c.y + s));
  graphics_draw_line(ctx, GPoint(c.x - d, c.y - d), GPoint(c.x + d, c.y + d));
  graphics_draw_line(ctx, GPoint(c.x - d, c.y + d), GPoint(c.x + d, c.y - d));
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, c, r);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(c.x - r / 2 - 1, c.y - r / 2 - 1, 2, 2), 0, GCornerNone);
}

static void draw_flag(GContext *ctx, GRect r, GColor pole) {
  int fw = r.size.w * 2 / 5;
  int cx = r.origin.x + r.size.w / 2 + fw / 2 - 1;
  int top = r.origin.y + r.size.h / 5;
  int bot = r.origin.y + r.size.h * 4 / 5;
  int fh = (bot - top) * 3 / 5;
  graphics_context_set_fill_color(ctx, pole);
  graphics_fill_rect(ctx, GRect(cx, top, 2, bot - top), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(cx - fw + 1, bot - 2, fw + 3, 2), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, pole));
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i <= fw; i++) {
    int dy = i * fh / (2 * fw);
    graphics_draw_line(ctx, GPoint(cx - i, top + dy), GPoint(cx - i, top + fh - dy));
  }
}

static void draw_trophy(GContext *ctx, GPoint c, int u) {
  if (u < 3) return;
  GColor gold = PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite);
  GColor light = GColorWhite;
  GColor dark = PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorBlack);
  GColor plinth = PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorWhite);

  // Henkel
  graphics_context_set_stroke_color(ctx, gold);
  graphics_context_set_stroke_width(ctx, (u / 6) | 1);
  graphics_draw_circle(ctx, GPoint(c.x - u, c.y - u / 3), u / 2);
  graphics_draw_circle(ctx, GPoint(c.x + u, c.y - u / 3), u / 2);
  graphics_context_set_stroke_width(ctx, 1);

  // Kelch
  graphics_context_set_fill_color(ctx, gold);
  graphics_fill_rect(ctx, GRect(c.x - u, c.y - u, 2 * u, u), 0, GCornerNone);
  graphics_fill_circle(ctx, c, u);
  graphics_fill_rect(ctx, GRect(c.x - u - 2, c.y - u - 3, 2 * u + 4, 4), 1, GCornersAll);
  // Glanzlicht
  graphics_context_set_fill_color(ctx, light);
  graphics_fill_rect(ctx, GRect(c.x - u * 2 / 3, c.y - u + 2, u / 5 + 1, u), 0, GCornerNone);
  // Stiel, Knoten, Fuss
  graphics_context_set_fill_color(ctx, gold);
  graphics_fill_rect(ctx, GRect(c.x - u / 6, c.y + u - 2, u / 3 + 1, u / 2 + 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(c.x - u / 3, c.y + u + u / 4 - 2, u * 2 / 3 + 1, 3), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(c.x - u * 2 / 3, c.y + u + u / 2, u * 4 / 3 + 1, u / 3 + 1), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, plinth);
  graphics_fill_rect(ctx, GRect(c.x - u * 4 / 5, c.y + u + u / 2 + u / 3 + 1, u * 8 / 5 + 1, u / 4 + 2),
                     0, GCornerNone);
  // "1" auf dem Pokal
  if (u >= 12) {
    graphics_context_set_text_color(ctx, dark);
    GFont f = fonts_get_system_font(u >= 20 ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
    int fs = u >= 20 ? 24 : 18;
    graphics_draw_text(ctx, "1", f, GRect(c.x - u, c.y - u / 2 - fs / 2 - 2, 2 * u, fs + 4),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void draw_sparkle(GContext *ctx, GPoint p, int s, GColor col) {
  if (s <= 0) return;
  graphics_context_set_stroke_color(ctx, col);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(p.x - s, p.y), GPoint(p.x + s, p.y));
  graphics_draw_line(ctx, GPoint(p.x, p.y - s), GPoint(p.x, p.y + s));
  int d = s / 2;
  graphics_draw_line(ctx, GPoint(p.x - d, p.y - d), GPoint(p.x + d, p.y + d));
  graphics_draw_line(ctx, GPoint(p.x - d, p.y + d), GPoint(p.x + d, p.y - d));
}

// Text mit Schatten, vertikal um cy zentriert
static void draw_text_shadow(GContext *ctx, const char *txt, GFont f, int fs, GRect b, int cy,
                             int dx, GColor fg, GColor shadow) {
  GRect r = GRect(b.origin.x + dx, cy - fs / 2 - fs / 5, b.size.w, fs + 8);
  graphics_context_set_text_color(ctx, shadow);
  graphics_draw_text(ctx, txt, f, GRect(r.origin.x + 2, r.origin.y + 2, r.size.w, r.size.h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, txt, f, r, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// ---------------------------------------------------------------------------
// Spiellogik
// ---------------------------------------------------------------------------
static void stop_anim_timer(void) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  if (s_game_layer) layer_mark_dirty(s_game_layer);
}

static void game_reset(void) {
  stop_anim_timer();
  tick_timer_service_unsubscribe();
  memset(s_cells, 0, sizeof(s_cells));
  s_cursor = (GRID_H / 2) * GRID_W + GRID_W / 2 - 1;
  s_opened = 0;
  s_flags = 0;
  s_exploded = -1;
  s_rank = -1;
  s_end_sel = END_RETRY;
  s_lost = false;
  s_seconds = 0;
  s_state = ST_READY;
  if (s_game_layer) layer_mark_dirty(s_game_layer);
}

static bool is_neighbor(int a, int b) {
  int ax = a % GRID_W, ay = a / GRID_W, bx = b % GRID_W, by = b / GRID_W;
  return ABS(ax - bx) <= 1 && ABS(ay - by) <= 1;
}

static void place_mines(int safe) {
  int mines = LEVEL_MINES[s_level];
  int placed = 0;
  while (placed < mines) {
    int i = rand() % CELLS;
    if ((s_cells[i] & C_MINE) || is_neighbor(i, safe)) continue;
    s_cells[i] |= C_MINE;
    placed++;
  }
  for (int i = 0; i < CELLS; i++) {
    int x = i % GRID_W, y = i / GRID_W, n = 0;
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if ((dx || dy) && nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H &&
            (s_cells[ny * GRID_W + nx] & C_MINE)) {
          n++;
        }
      }
    }
    s_cells[i] = (s_cells[i] & ~C_COUNT) | n;
  }
}

static void anim_tick(void *data);
static GRect cell_rect(int i);

static void start_anim(GameState st) {
  stop_anim_timer();
  s_state = st;
  s_anim_frame = 0;
  s_anim_timer = app_timer_register(ANIM_MS, anim_tick, NULL);
  if (s_game_layer) layer_mark_dirty(s_game_layer);
}

static void init_debris(void) {
  GRect er = cell_rect(s_exploded);
  int cx = er.origin.x + er.size.w / 2;
  int cy = er.origin.y + er.size.h / 2;
  for (int i = 0; i < NUM_PARTICLES; i++) {
    int32_t a = i * TRIG_MAX_ANGLE / NUM_PARTICLES + rand() % (TRIG_MAX_ANGLE / 32);
    int speed = 40 + rand() % 60;  // 1/16 px pro Frame
    s_px[i] = cx;
    s_py[i] = cy;
    s_pvx[i] = cos_lookup(a) * speed / TRIG_MAX_RATIO;
    s_pvy[i] = sin_lookup(a) * speed / TRIG_MAX_RATIO - 24;
    s_pcol[i] = i;
  }
}

static void init_confetti(GRect b) {
  for (int i = 0; i < NUM_PARTICLES; i++) {
    s_px[i] = rand() % b.size.w;
    s_py[i] = -(rand() % (b.size.h * 2 / 3)) - 8;
    s_pvy[i] = 3 + rand() % 4;
    s_pvx[i] = rand() % TRIG_MAX_ANGLE;  // Phase fuer das Pendeln
    s_pcol[i] = rand() % 7;
  }
}

static void game_lose(int idx) {
  tick_timer_service_unsubscribe();
  s_seconds = current_seconds();
  s_lost = true;
  s_exploded = idx;
  s_cells[idx] |= C_OPEN;
  init_debris();
  vibes_long_pulse();
  start_anim(ST_ANIM_LOSE);
}

static void game_win(void) {
  tick_timer_service_unsubscribe();
  int64_t ms = now_ms() - s_start_ms;
  s_seconds = (int)(ms / 1000);
  if (s_seconds < 1) s_seconds = 1;
  for (int i = 0; i < CELLS; i++) {
    if ((s_cells[i] & C_MINE) && !(s_cells[i] & C_FLAG)) {
      s_cells[i] |= C_FLAG;
      s_flags++;
    }
  }
  s_rank = hs_insert(s_level, s_seconds);
  static const uint32_t pattern[] = {80, 60, 80, 60, 220};
  vibes_enqueue_custom_pattern((VibePattern){.durations = pattern, .num_segments = ARRAY_LENGTH(pattern)});
  init_confetti(layer_get_bounds(s_game_layer));
  start_anim(ST_ANIM_WIN);
}

// Deckt ein Feld auf (mit Flood-Fill). Liefert false, wenn eine Mine getroffen wurde.
static bool reveal(int idx) {
  if (s_cells[idx] & (C_OPEN | C_FLAG)) return true;
  if (s_cells[idx] & C_MINE) {
    game_lose(idx);
    return false;
  }
  uint8_t stack[CELLS];
  int sp = 0;
  stack[sp++] = idx;
  s_cells[idx] |= C_OPEN;
  s_opened++;
  while (sp > 0) {
    int i = stack[--sp];
    if ((s_cells[i] & C_COUNT) != 0) continue;
    int x = i % GRID_W, y = i / GRID_W;
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) continue;
        int n = ny * GRID_W + nx;
        if (s_cells[n] & (C_OPEN | C_FLAG | C_MINE)) continue;
        s_cells[n] |= C_OPEN;
        s_opened++;
        stack[sp++] = n;
      }
    }
  }
  return true;
}

static void check_win(void) {
  if (s_state == ST_PLAYING && s_opened == CELLS - LEVEL_MINES[s_level]) game_win();
}

static void open_at_cursor(void) {
  int idx = s_cursor;
  if (s_cells[idx] & C_FLAG) return;
  if (s_state == ST_READY) {
    place_mines(idx);
    s_start_ms = now_ms();
    s_state = ST_PLAYING;
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  }
  if (s_cells[idx] & C_OPEN) {
    // "Chording": stimmt die Zahl der Faehnchen, werden alle Nachbarn aufgedeckt
    int n = s_cells[idx] & C_COUNT;
    if (n == 0) return;
    int x = idx % GRID_W, y = idx / GRID_W, flags = 0;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && (s_cells[ny * GRID_W + nx] & C_FLAG)) flags++;
      }
    if (flags != n) return;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) continue;
        if (!reveal(ny * GRID_W + nx)) return;
      }
  } else if (!reveal(idx)) {
    return;
  }
  check_win();
}

static void toggle_flag(void) {
  uint8_t *c = &s_cells[s_cursor];
  if (*c & C_OPEN) return;
  *c ^= C_FLAG;
  s_flags += (*c & C_FLAG) ? 1 : -1;
  static const uint32_t pattern[] = {40};
  vibes_enqueue_custom_pattern((VibePattern){.durations = pattern, .num_segments = 1});
}

static void anim_finished(void) {
  s_anim_timer = NULL;
  if (s_state == ST_ANIM_WIN && s_rank >= 0) {
    static const uint32_t pattern[] = {100, 80, 100, 80, 100, 80, 400};
    vibes_enqueue_custom_pattern((VibePattern){.durations = pattern, .num_segments = ARRAY_LENGTH(pattern)});
    start_anim(ST_ANIM_RECORD);
    return;
  }
  s_state = ST_DONE;
  // Highscore-Liste anzeigen; danach erscheint das End-Menue
  s_hs_from_game = true;
  window_stack_push(s_hs_window, true);
}

static void anim_tick(void *data) {
  s_anim_timer = NULL;
  s_anim_frame++;
  int len = s_state == ST_ANIM_LOSE ? LOSE_FRAMES : s_state == ST_ANIM_WIN ? WIN_FRAMES : RECORD_FRAMES;
  if (s_anim_frame >= len) {
    anim_finished();
  } else {
    s_anim_timer = app_timer_register(ANIM_MS, anim_tick, NULL);
  }
  if (s_game_layer) layer_mark_dirty(s_game_layer);
}

static bool in_anim(void) {
  return s_state == ST_ANIM_LOSE || s_state == ST_ANIM_WIN || s_state == ST_ANIM_RECORD;
}

// Tastendruck waehrend einer Animation: nach kurzer Sperrzeit ueberspringen
static void skip_anim(void) {
  if (s_anim_frame < ANIM_SKIP_AFTER) return;
  stop_anim_timer();
  anim_finished();
  layer_mark_dirty(s_game_layer);
}

// ---------------------------------------------------------------------------
// Spiel-Zeichnung
// ---------------------------------------------------------------------------
static void compute_layout(GRect b) {
  s_big = b.size.w >= 180;
  s_hh = s_big ? 24 : 18;
  int cw = b.size.w / GRID_W;
  int ch = (b.size.h - s_hh - 2) / GRID_H;
  s_cs = cw < ch ? cw : ch;
  s_ox = (b.size.w - s_cs * GRID_W) / 2;
  s_oy = s_hh + (b.size.h - s_hh - s_cs * GRID_H) / 2;
  s_zs = s_cs * 8 / 5;  // Zoom 1,6x: auf der Time 2 sind dann 5x5 Felder sichtbar
}

static int cur_cs(void) {
  return s_zoom ? s_zs : s_cs;
}

static int view_size(void) {
  return s_cs * GRID_W;  // sichtbare Brettflaeche (quadratisch)
}

static void clamp_view(void) {
  int max = s_zs * GRID_W - view_size();
  if (s_vx < 0) s_vx = 0;
  if (s_vy < 0) s_vy = 0;
  if (s_vx > max) s_vx = max;
  if (s_vy > max) s_vy = max;
}

// Bildschirm-Rechteck eines Feldes (beruecksichtigt Zoom und Ausschnitt)
static GRect cell_rect(int i) {
  int cs = cur_cs();
  int vx = s_zoom ? s_vx : 0, vy = s_zoom ? s_vy : 0;
  return GRect(s_ox + (i % GRID_W) * cs - vx, s_oy + (i / GRID_W) * cs - vy, cs, cs);
}

#ifdef PBL_TOUCH
// Feld unter einem Bildschirmpunkt oder -1
static int cell_at(GPoint p) {
  int v = view_size();
  if (p.x < s_ox || p.y < s_oy || p.x >= s_ox + v || p.y >= s_oy + v) return -1;
  int cs = cur_cs();
  int x = (p.x - s_ox + (s_zoom ? s_vx : 0)) / cs;
  int y = (p.y - s_oy + (s_zoom ? s_vy : 0)) / cs;
  if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return -1;
  return y * GRID_W + x;
}

#endif

// Im Zoom den Ausschnitt so verschieben, dass der Cursor sichtbar ist
static void ensure_cursor_visible(void) {
  if (!s_zoom) return;
  int v = view_size();
  int cx = (s_cursor % GRID_W) * s_zs, cy = (s_cursor / GRID_W) * s_zs;
  if (cx < s_vx) s_vx = cx;
  if (cx + s_zs > s_vx + v) s_vx = cx + s_zs - v;
  if (cy < s_vy) s_vy = cy;
  if (cy + s_zs > s_vy + v) s_vy = cy + s_zs - v;
  clamp_view();
}

#ifdef PBL_TOUCH
static void toggle_zoom(void) {
  s_zoom = !s_zoom;
  if (s_zoom) {
    // auf den Cursor zentrieren
    int v = view_size();
    s_vx = (s_cursor % GRID_W) * s_zs + s_zs / 2 - v / 2;
    s_vy = (s_cursor / GRID_W) * s_zs + s_zs / 2 - v / 2;
    clamp_view();
  }
}
#endif

static void draw_magnifier(GContext *ctx, GPoint c, bool minus) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, c, 6);
  graphics_draw_line(ctx, GPoint(c.x + 5, c.y + 5), GPoint(c.x + 9, c.y + 9));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(c.x - 3, c.y), GPoint(c.x + 3, c.y));
  if (!minus) graphics_draw_line(ctx, GPoint(c.x, c.y - 3), GPoint(c.x, c.y + 3));
}

static void draw_header(GContext *ctx, GRect b) {
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, s_hh), 0, GCornerNone);
  GFont f = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD);
  int ty = s_big ? 0 : 0;
  int th = s_hh;

  // Minen-Zaehler mit kleinem Minen-Symbol
  int r = s_big ? 5 : 4;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(4 + r + 1, s_hh / 2), r + 1);
  draw_mine(ctx, GPoint(4 + r + 1, s_hh / 2), r - 1);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", LEVEL_MINES[s_level] - s_flags);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf, f, GRect(2 * r + 9, ty, 40, th), GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorIcterine, GColorWhite));
  graphics_draw_text(ctx, T(S_EASY + s_level), f, GRect(0, ty, b.size.w, th), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);

  fmt_time(buf, sizeof(buf), current_seconds());
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, buf, f, GRect(b.size.w - 60, ty, 56, th), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);

  // Lupe: Kopfzeile antippen = Zoom an/aus
  if (s_touch) draw_magnifier(ctx, GPoint(b.size.w - 62, s_hh / 2 - 1), s_zoom);
}

static void draw_closed(GContext *ctx, GRect r) {
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
  int x0 = r.origin.x, y0 = r.origin.y, x1 = x0 + r.size.w - 1, y1 = y0 + r.size.h - 1;
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorCeleste);
  graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x1 - 1, y0));
  graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x0, y1 - 1));
  graphics_draw_line(ctx, GPoint(x0 + 1, y0 + 1), GPoint(x1 - 2, y0 + 1));
  graphics_draw_line(ctx, GPoint(x0 + 1, y0 + 1), GPoint(x0 + 1, y1 - 2));
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_draw_line(ctx, GPoint(x1, y0), GPoint(x1, y1));
  graphics_draw_line(ctx, GPoint(x0, y1), GPoint(x1, y1));
  graphics_draw_line(ctx, GPoint(x1 - 1, y0 + 1), GPoint(x1 - 1, y1));
  graphics_draw_line(ctx, GPoint(x0 + 1, y1 - 1), GPoint(x1, y1 - 1));
#else
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, grect_inset(r, GEdgeInsets(1)), 1, GCornersAll);
#endif
}

static void draw_open(GContext *ctx, GRect r, GColor bg) {
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, r);
}

static void draw_board(GContext *ctx) {
  int cs = cur_cs();
  GFont nf = fonts_get_system_font(cs >= 36 ? FONT_KEY_GOTHIC_28_BOLD
                                   : cs >= 24 ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  int fs = cs >= 36 ? 28 : cs >= 24 ? 24 : 18;
  int v = view_size();
  GRect view = GRect(s_ox, s_oy, v, v);
  bool show_all = s_lost;
  GColor flag_pole = PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite);

  for (int i = 0; i < CELLS; i++) {
    GRect r = cell_rect(i);
    // im Zoom nur sichtbare Felder zeichnen
    if (r.origin.x >= view.origin.x + v || r.origin.y >= view.origin.y + v ||
        r.origin.x + cs <= view.origin.x || r.origin.y + cs <= view.origin.y) continue;
    GPoint c = GPoint(r.origin.x + cs / 2, r.origin.y + cs / 2);
    uint8_t cv = s_cells[i];
    int mr = cs / 4;

    if (i == s_exploded) {
      draw_open(ctx, r, PBL_IF_COLOR_ELSE(GColorRed, GColorWhite));
      draw_mine(ctx, c, mr);
    } else if (cv & C_OPEN) {
      draw_open(ctx, r, GColorWhite);
      int n = cv & C_COUNT;
      if (n > 0) {
        char t[2] = {'0' + n, 0};
        graphics_context_set_text_color(ctx, num_color(n));
        graphics_draw_text(ctx, t, nf, GRect(r.origin.x, r.origin.y + (cs - fs) / 2 - fs / 6, cs, fs + 4),
                           GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      }
    } else if (show_all && (cv & C_MINE) && !(cv & C_FLAG)) {
      draw_open(ctx, r, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
      draw_mine(ctx, c, mr);
    } else if (cv & C_FLAG) {
      draw_closed(ctx, r);
      draw_flag(ctx, r, flag_pole);
      if (show_all && !(cv & C_MINE)) {
        // falsch gesetztes Faehnchen
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
        graphics_context_set_stroke_width(ctx, 3);
        graphics_draw_line(ctx, GPoint(r.origin.x + 4, r.origin.y + 4),
                           GPoint(r.origin.x + cs - 5, r.origin.y + cs - 5));
        graphics_draw_line(ctx, GPoint(r.origin.x + cs - 5, r.origin.y + 4),
                           GPoint(r.origin.x + 4, r.origin.y + cs - 5));
        graphics_context_set_stroke_width(ctx, 1);
      }
    } else {
      draw_closed(ctx, r);
    }
  }

  // Cursor
  if (s_state == ST_READY || s_state == ST_PLAYING) {
    GRect r = cell_rect(s_cursor);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_rect(ctx, r);
    graphics_draw_rect(ctx, grect_inset(r, GEdgeInsets(3)));
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_draw_rect(ctx, grect_inset(r, GEdgeInsets(1)));
    graphics_draw_rect(ctx, grect_inset(r, GEdgeInsets(2)));
  }

  // Im Zoom: Positionsbalken unten und rechts
  if (s_zoom) {
    int full = s_zs * GRID_W;
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack));
    graphics_fill_rect(ctx, GRect(s_ox + s_vx * v / full, s_oy + v - 3, v * v / full, 3), 1, GCornersAll);
    graphics_fill_rect(ctx, GRect(s_ox + v - 3, s_oy + s_vy * v / full, 3, v * v / full), 1, GCornersAll);
  }
}

static void draw_lose_anim(GContext *ctx, GRect b) {
  int t = s_anim_frame - 4;
  GRect er = cell_rect(s_exploded);
  GPoint c = GPoint(er.origin.x + s_cs / 2, er.origin.y + s_cs / 2);

  // Truemmer mit Schwerkraft
  if (t < 34) {
    for (int i = 0; i < NUM_PARTICLES; i++) {
      int x = s_px[i] + s_pvx[i] * t / 16;
      int y = s_py[i] + (s_pvy[i] * t + 3 * t * t) / 16;
      graphics_context_set_fill_color(ctx, (i & 1) ? PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)
                                                   : PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack));
      int sz = (i % 3) + 3;
      graphics_fill_rect(ctx, GRect(x, y, sz, sz), 0, GCornerNone);
    }
  }
  // Feuerball
  if (t < 20) {
    int R = cur_cs() * 2;
    int r = t < 8 ? R * (t + 1) / 8 : R * (20 - t) / 12;
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack));
    graphics_fill_circle(ctx, c, r);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_fill_circle(ctx, c, r * 2 / 3);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
    graphics_fill_circle(ctx, c, r / 3);
  }
  // Druckwelle
  if (t < 18) {
    graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, GColorBlack));
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_circle(ctx, c, (t + 1) * cur_cs() * 2 / 3);
    graphics_context_set_stroke_width(ctx, 1);
  }
  // BOOM!
  if (t >= 6) {
    int shake = t < 22 ? ((t & 1) ? 4 : -4) : 0;
    GFont f = fonts_get_system_font(s_big ? FONT_KEY_BITHAM_42_BOLD : FONT_KEY_BITHAM_30_BLACK);
    int fs = s_big ? 42 : 30;
    draw_text_shadow(ctx, "BOOM!", f, fs, b, b.size.h / 2, shake, PBL_IF_COLOR_ELSE(GColorRed, GColorBlack),
                     PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  }
}

static void draw_win_anim(GContext *ctx, GRect b) {
  int f = s_anim_frame;
  // Konfetti
  for (int i = 0; i < NUM_PARTICLES; i++) {
    int y = s_py[i] + s_pvy[i] * f;
    if (y < -8 || y > b.size.h) continue;
    int x = s_px[i] + 5 * sin_lookup(s_pvx[i] + f * TRIG_MAX_ANGLE / 20) / TRIG_MAX_RATIO;
    graphics_context_set_fill_color(ctx, particle_color(s_pcol[i]));
    bool tall = ((f / 4 + i) & 1);
    graphics_fill_rect(ctx, GRect(x, y, tall ? 4 : 7, tall ? 7 : 4), 0, GCornerNone);
  }
  // Banner faehrt von links herein
  if (f >= 6) {
    int k = f - 6;
    int off = k < 10 ? (10 - k) * b.size.w / 10 : 0;
    int bh = s_big ? 64 : 50;
    GRect br = GRect(-off, b.size.h / 2 - bh / 2, b.size.w, bh);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack));
    graphics_fill_rect(ctx, br, 0, GCornerNone);
    graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_fill_rect(ctx, GRect(br.origin.x, br.origin.y, br.size.w, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(br.origin.x, br.origin.y + bh - 2, br.size.w, 2), 0, GCornerNone);

    GFont tf = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD);
    GFont sf = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, T(S_WON_BANNER), tf, GRect(br.origin.x, br.origin.y + 2, br.size.w, bh / 2 + 6),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    char tb[16], buf[32];
    fmt_time(tb, sizeof(tb), s_seconds);
    snprintf(buf, sizeof(buf), T(S_TIME_FMT), tb);
    graphics_draw_text(ctx, buf, sf, GRect(br.origin.x, br.origin.y + bh / 2 + (s_big ? 6 : 4), br.size.w, bh / 2),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void draw_record_anim(GContext *ctx, GRect b) {
  int f = s_anim_frame;
  GPoint c = GPoint(b.size.w / 2, b.size.h / 2 + (s_big ? 6 : 4));

  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorDukeBlue, GColorBlack));
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Rotierende Strahlen
  const int rays = 10;
  int32_t step = TRIG_MAX_ANGLE / rays;
  int32_t rot = f * TRIG_MAX_ANGLE / 160;
  int R = b.size.h;
  GPoint pts[3];
  GPathInfo info = {.num_points = 3, .points = pts};
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorBlueMoon, GColorWhite));
  graphics_context_set_stroke_color(ctx, GColorWhite);
  for (int i = 0; i < rays; i++) {
    int32_t a = rot + i * step;
    int32_t a1 = a - step / 4, a2 = a + step / 4;
    pts[0] = c;
    pts[1] = GPoint(c.x + R * cos_lookup(a1) / TRIG_MAX_RATIO, c.y + R * sin_lookup(a1) / TRIG_MAX_RATIO);
    pts[2] = GPoint(c.x + R * cos_lookup(a2) / TRIG_MAX_RATIO, c.y + R * sin_lookup(a2) / TRIG_MAX_RATIO);
    GPath *p = gpath_create(&info);
#ifdef PBL_COLOR
    gpath_draw_filled(ctx, p);
#else
    gpath_draw_outline(ctx, p);
#endif
    gpath_destroy(p);
  }

  // Pokal waechst mit Ueberschwinger herein und wippt danach
  int base = s_big ? 28 : 20;
  int scale;
  if (f < 12) scale = (f + 1) * 115 / 12;
  else if (f < 16) scale = 115 - (f - 12) * 15 / 4;
  else scale = 100;
  int bob = f >= 16 ? 3 * sin_lookup(f * TRIG_MAX_ANGLE / 24) / TRIG_MAX_RATIO : 0;
  GPoint tc = GPoint(c.x, c.y - base / 3 + bob);
  draw_trophy(ctx, tc, base * scale / 100);

  // Funkeln
  if (f >= 10) {
    for (int i = 0; i < 8; i++) {
      int32_t a = i * TRIG_MAX_ANGLE / 8 + f * TRIG_MAX_ANGLE / 300;
      int rad = base * 2 + (i & 1) * base / 2;
      GPoint p = GPoint(tc.x + rad * cos_lookup(a) / TRIG_MAX_RATIO, tc.y + rad * sin_lookup(a) / TRIG_MAX_RATIO);
      int s = 1 + ABS(sin_lookup(f * TRIG_MAX_ANGLE / 14 + i * TRIG_MAX_ANGLE / 5)) * 5 / TRIG_MAX_RATIO;
      draw_sparkle(ctx, p, s, (i & 1) ? GColorWhite : PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    }
  }

  // Texte
  GFont tf = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD);
  GFont sf = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  if ((f >= 4) && (f >= 20 || (f & 2))) {
    draw_text_shadow(ctx, T(S_RECORD_BANNER), tf, s_big ? 28 : 24, b, s_big ? 22 : 16, 0,
                     PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite), GColorBlack);
  }
  if (f >= 14) {
    char tb[16], buf[64];
    fmt_time(tb, sizeof(tb), s_seconds);
    snprintf(buf, sizeof(buf), T(S_RECORD_LINE_FMT), T(S_EASY + s_level), s_rank + 1, tb);
    draw_text_shadow(ctx, buf, s_big ? sf : fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), s_big ? 24 : 14, b,
                     b.size.h - (s_big ? 20 : 14), 0, GColorWhite, GColorBlack);
  }
}

static GRect end_panel_rect(GRect b) {
  int rh = s_big ? 30 : 24;
  int th = s_big ? 54 : 42;
  int pw = b.size.w - (s_big ? 24 : 12);
  int ph = th + END_COUNT * rh + 8;
  return GRect((b.size.w - pw) / 2, (b.size.h - ph) / 2 + s_hh / 2, pw, ph);
}

static GRect end_option_rect(GRect b, int i) {
  GRect p = end_panel_rect(b);
  int rh = s_big ? 30 : 24;
  int th = s_big ? 54 : 42;
  return GRect(p.origin.x + 4, p.origin.y + th + i * rh, p.size.w - 8, rh - 2);
}

static void draw_end_panel(GContext *ctx, GRect b) {
  int rh = s_big ? 30 : 24;
  int th = s_big ? 54 : 42;
  GRect p = end_panel_rect(b);
  int pw = p.size.w;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, grect_inset(p, GEdgeInsets(-2)), 6, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, p, 5, GCornersAll);

  GFont tf = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  GFont sf = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  GFont of = fonts_get_system_font(s_big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);

  const char *title = s_lost ? T(S_LOST) : (s_rank >= 0 ? T(S_NEW_RECORD) : T(S_WON));
  GColor tcol = s_lost ? PBL_IF_COLOR_ELSE(GColorRed, GColorBlack)
                       : PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack);
  int tx = p.origin.x;
  int tw = pw;
  if (!s_lost && s_rank >= 0) {
    // kleiner Pokal im End-Menue
#ifdef PBL_COLOR
    draw_trophy(ctx, GPoint(p.origin.x + 18, p.origin.y + th / 2 - 4), s_big ? 9 : 7);
#else
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(p.origin.x + 4, p.origin.y + 4, 28, th - 8), 3, GCornersAll);
    draw_trophy(ctx, GPoint(p.origin.x + 18, p.origin.y + th / 2 - 4), s_big ? 9 : 7);
#endif
    tx += 30;
    tw -= 34;
  }
  graphics_context_set_text_color(ctx, tcol);
  graphics_draw_text(ctx, title, tf, GRect(tx, p.origin.y, tw, th / 2 + 6), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  char tb[16], buf[48];
  fmt_time(tb, sizeof(tb), s_seconds);
  if (s_lost) {
    snprintf(buf, sizeof(buf), T(S_LOST_AFTER_FMT), T(S_EASY + s_level), tb);
  } else if (s_rank >= 0) {
    snprintf(buf, sizeof(buf), T(S_RANK_FMT), s_rank + 1, tb);
  } else {
    snprintf(buf, sizeof(buf), "%s - %s", T(S_EASY + s_level), tb);
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, buf, sf, GRect(tx, p.origin.y + th / 2 + (s_big ? 2 : 1), tw, th / 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  for (int i = 0; i < END_COUNT; i++) {
    GRect r = end_option_rect(b, i);
    bool sel = (i == s_end_sel);
    if (sel) {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack));
      graphics_fill_rect(ctx, r, 4, GCornersAll);
    }
    graphics_context_set_text_color(ctx, sel ? GColorWhite : GColorBlack);
    graphics_draw_text(ctx, T(END_LABELS[i]), of, GRect(r.origin.x, r.origin.y - (s_big ? 3 : 2), r.size.w, rh),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void game_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  if (s_state == ST_ANIM_RECORD) {
    draw_record_anim(ctx, b);
    return;
  }
  if (s_state == ST_ANIM_LOSE && s_anim_frame < 4) {
    // Blitz
    graphics_context_set_fill_color(ctx, (s_anim_frame & 1) ? PBL_IF_COLOR_ELSE(GColorRed, GColorBlack)
                                                             : PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
    graphics_fill_rect(ctx, b, 0, GCornerNone);
    return;
  }

  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  draw_board(ctx);
  // Ueberstand des gezoomten Bretts abdecken
  int v = view_size();
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, GRect(0, s_oy + v, b.size.w, b.size.h - s_oy - v), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(0, s_hh, b.size.w, s_oy - s_hh), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(0, s_oy, s_ox, v), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(s_ox + v, s_oy, b.size.w - s_ox - v, v), 0, GCornerNone);
  draw_header(ctx, b);

  if (s_state == ST_ANIM_LOSE) draw_lose_anim(ctx, b);
  else if (s_state == ST_ANIM_WIN) draw_win_anim(ctx, b);
  else if (s_state == ST_DONE) draw_end_panel(ctx, b);
}

// ---------------------------------------------------------------------------
// Spiel-Tasten
// ---------------------------------------------------------------------------
static void end_confirm(void) {
  switch (s_end_sel) {
    case END_RETRY:
      game_reset();
      break;
    default:
      window_stack_remove(s_game_window, true);  // zurueck ins Hauptmenue
      break;
  }
}

// UP = horizontal (nach rechts), DOWN = vertikal (nach unten), jeweils mit Umbruch
static void move_cursor(int dx, int dy) {
  int x = (s_cursor % GRID_W + dx) % GRID_W;
  int y = (s_cursor / GRID_W + dy) % GRID_H;
  s_cursor = y * GRID_W + x;
  ensure_cursor_visible();
  layer_mark_dirty(s_game_layer);
}

static void game_up_click(ClickRecognizerRef rec, void *ctx) {
  if (in_anim()) { skip_anim(); return; }
  if (s_state == ST_DONE) {
    s_end_sel = (s_end_sel + END_COUNT - 1) % END_COUNT;
    layer_mark_dirty(s_game_layer);
    return;
  }
  move_cursor(1, 0);
}

static void game_down_click(ClickRecognizerRef rec, void *ctx) {
  if (in_anim()) { skip_anim(); return; }
  if (s_state == ST_DONE) {
    s_end_sel = (s_end_sel + 1) % END_COUNT;
    layer_mark_dirty(s_game_layer);
    return;
  }
  move_cursor(0, 1);
}

static void game_select_click(ClickRecognizerRef rec, void *ctx) {
  if (in_anim()) { skip_anim(); return; }
  if (s_state == ST_DONE) { end_confirm(); return; }
  open_at_cursor();
  layer_mark_dirty(s_game_layer);
}

static void game_select_long(ClickRecognizerRef rec, void *ctx) {
  if (in_anim()) { skip_anim(); return; }
  if (s_state == ST_DONE) { end_confirm(); return; }
  toggle_flag();
  layer_mark_dirty(s_game_layer);
}

static void game_click_config(void *ctx) {
  // Halten = Cursor laeuft automatisch weiter
  window_single_repeating_click_subscribe(BUTTON_ID_UP, CURSOR_REPEAT_MS, game_up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, CURSOR_REPEAT_MS, game_down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, game_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 400, game_select_long, NULL);
}

#ifdef PBL_TOUCH
// ---------------------------------------------------------------------------
// Spiel-Touch (Pebble Time 2): Tippen = oeffnen, Halten = Faehnchen,
// Kopfzeile tippen = Zoom, Ziehen = Ausschnitt verschieben
// ---------------------------------------------------------------------------
static bool playing(void) {
  return s_state == ST_READY || s_state == ST_PLAYING;
}

static void cancel_long_timer(void) {
  if (s_long_timer) {
    app_timer_cancel(s_long_timer);
    s_long_timer = NULL;
  }
}

static void long_press_fired(void *data) {
  s_long_timer = NULL;
  if (!s_touch_down || s_touch_dragged || s_touch_cell < 0 || !playing()) return;
  s_cursor = s_touch_cell;
  toggle_flag();
  s_touch_long_fired = true;
  layer_mark_dirty(s_game_layer);
}

static void game_touch_tap(GPoint p) {
  if (in_anim()) { skip_anim(); return; }
  GRect b = layer_get_bounds(s_game_layer);
  if (s_state == ST_DONE) {
    for (int i = 0; i < END_COUNT; i++) {
      GRect r = end_option_rect(b, i);
      if (grect_contains_point(&r, &p)) {
        s_end_sel = i;
        end_confirm();
        return;
      }
    }
    return;
  }
  if (p.y < s_hh) {
    toggle_zoom();
  } else {
    int cell = cell_at(p);
    if (cell < 0) return;
    s_cursor = cell;
    open_at_cursor();
  }
  layer_mark_dirty(s_game_layer);
}

static void game_touch(const TouchEvent *e) {
  GPoint p = GPoint(e->x, e->y);
  switch (e->type) {
    case TouchEvent_Touchdown:
      s_touch_down = true;
      s_touch_dragged = false;
      s_touch_long_fired = false;
      s_touch_start = s_touch_last = p;
      s_touch_cell = playing() ? cell_at(p) : -1;
      cancel_long_timer();
      if (s_touch_cell >= 0) {
        s_cursor_before_touch = s_cursor;
        s_cursor = s_touch_cell;  // sofort zeigen, welches Feld getroffen ist
        s_long_timer = app_timer_register(LONG_PRESS_MS, long_press_fired, NULL);
        layer_mark_dirty(s_game_layer);
      }
      break;
    case TouchEvent_PositionUpdate:
      if (!s_touch_down) break;
      if (!s_touch_dragged && (ABS(p.x - s_touch_start.x) > TOUCH_SLOP || ABS(p.y - s_touch_start.y) > TOUCH_SLOP)) {
        s_touch_dragged = true;
        cancel_long_timer();
        if (s_touch_cell >= 0) s_cursor = s_cursor_before_touch;  // Ziehen bewegt den Cursor nicht
      }
      if (s_touch_dragged && s_zoom && playing()) {
        s_vx += s_touch_last.x - p.x;
        s_vy += s_touch_last.y - p.y;
        clamp_view();
        layer_mark_dirty(s_game_layer);
      }
      s_touch_last = p;
      break;
    case TouchEvent_Liftoff:
      if (!s_touch_down) break;
      s_touch_down = false;
      cancel_long_timer();
      if (!s_touch_dragged && !s_touch_long_fired) game_touch_tap(s_touch_last);
      break;
  }
}

#endif  // PBL_TOUCH

static void game_window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  compute_layout(b);
  s_game_layer = layer_create(b);
  layer_set_update_proc(s_game_layer, game_update_proc);
  layer_add_child(root, s_game_layer);
}

static void game_window_unload(Window *w) {
  stop_anim_timer();
#ifdef PBL_TOUCH
  cancel_long_timer();
  s_touch_down = false;
#endif
  tick_timer_service_unsubscribe();
  layer_destroy(s_game_layer);
  s_game_layer = NULL;
}

static void start_game(int level) {
  s_level = level;
  game_reset();
  window_stack_push(s_game_window, true);
}

// ---------------------------------------------------------------------------
// Intro / Steuerung
// ---------------------------------------------------------------------------
static void intro_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  bool big = b.size.w >= 180;
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Titel mit Mine
  int y = big ? 6 : 2;
  int r = big ? 9 : 7;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(b.size.w / 2, y + r + 4), r + 4);
  draw_mine(ctx, GPoint(b.size.w / 2, y + r + 4), r - 1);
  y += 2 * r + 8;

  GFont tf = fonts_get_system_font(big ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "MINESWEEPER", tf, GRect(0, y - 4, b.size.w, 32), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  y += big ? 30 : 24;

  // Mit Touchscreen: Tasten (ohne BACK) und darunter die Touch-Gesten, kompakter gesetzt
  const char *const keys[] = {"UP", "DOWN", "SELECT", T(S_KEY_HOLD), "BACK",
                              T(S_TOUCH_TAP), T(S_TOUCH_HOLD), T(S_TOUCH_TOPBAR), T(S_TOUCH_DRAG)};
  const char *const acts[] = {T(S_ACT_RIGHT), T(S_ACT_DOWN), T(S_ACT_OPEN), T(S_ACT_FLAG), T(S_ACT_MENU),
                              T(S_ACT_OPEN), T(S_ACT_FLAG), T(S_ACT_ZOOM), T(S_ACT_PAN)};
  bool compact = s_touch;
  GFont kf = fonts_get_system_font(big && !compact ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD);
  GFont af = fonts_get_system_font(big && !compact ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  int lh = compact ? 17 : (big ? 24 : 18);
  int half = b.size.w / 2;
  for (int i = 0; i < 9; i++) {
    if (compact && i == 4) continue;   // BACK weglassen, Platz fuer Touch-Zeilen
    if (!compact && i >= 5) break;
    if (i == 2 || i == 4 || i == 5) {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorWhite));
      graphics_fill_rect(ctx, GRect(8, y - 1, b.size.w - 16, 1), 0, GCornerNone);
    }
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite));
    graphics_draw_text(ctx, keys[i], kf, GRect(4, y - 2, half - 2, lh), GTextOverflowModeFill,
                       GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, acts[i], af, GRect(half - 4, y - 2, half, lh), GTextOverflowModeFill,
                       GTextAlignmentRight, NULL);
    y += lh;
  }

  GFont hf = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_draw_text(ctx, T(S_PRESS_BACK), hf,
                     GRect(0, b.size.h - 20, b.size.w, 18), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void intro_click(ClickRecognizerRef rec, void *ctx) {
  window_stack_remove(s_intro_window, true);
}

static void intro_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, intro_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, intro_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, intro_click);
}

static void intro_window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_intro_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_intro_layer, intro_update_proc);
  layer_add_child(root, s_intro_layer);
}

static void intro_window_unload(Window *w) {
  layer_destroy(s_intro_layer);
}

// ---------------------------------------------------------------------------
// Highscore-Fenster
// ---------------------------------------------------------------------------
static void hs_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  bool big = b.size.w >= 180;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  int th = big ? 40 : 32;
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, th), 0, GCornerNone);
  draw_trophy(ctx, GPoint(big ? 22 : 16, th / 2 - (big ? 4 : 3)), big ? 9 : 7);
  draw_trophy(ctx, GPoint(b.size.w - (big ? 22 : 16), th / 2 - (big ? 4 : 3)), big ? 9 : 7);
  GFont tf = fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, "HIGHSCORES", tf, GRect(0, (th - (big ? 30 : 24)) / 2, b.size.w, 30),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  GFont hf = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD);
  GFont vf = fonts_get_system_font(big ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  int rw = big ? 22 : 13;
  int cw = (b.size.w - rw) / NUM_LEVELS;
  int lh = big ? 26 : 18;
  int y = th + 4;

  for (int l = 0; l < NUM_LEVELS; l++) {
    bool played = s_hs_from_game && l == s_level;
    if (played) {
      // Spalte der gespielten Stufe markieren
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorCeleste, GColorBlack));
      graphics_fill_rect(ctx, GRect(rw + l * cw + 1, y, cw - 2, lh * (HS_COUNT + 1) + 3), 3, GCornersAll);
    }
    graphics_context_set_text_color(ctx, played ? PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite)
                                                 : PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack));
    // kleine Displays: normale statt fetter Schrift, damit "Medium" in die Spalte passt
    graphics_draw_text(ctx, T(S_EASY + l), big ? hf : vf, GRect(rw + l * cw, y, cw, lh), GTextOverflowModeFill,
                       GTextAlignmentCenter, NULL);
  }
  y += lh;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(4, y, b.size.w - 8, 1), 0, GCornerNone);
  y += 3;

  for (int i = 0; i < HS_COUNT; i++) {
    if (i == 0 && !s_hs_from_game) {
      graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorIcterine, GColorWhite));
      graphics_fill_rect(ctx, GRect(0, y, b.size.w, lh), 0, GCornerNone);
    }
    char rb[4];
    snprintf(rb, sizeof(rb), "%d.", i + 1);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, rb, hf, GRect(2, y - 2, rw, lh), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    for (int l = 0; l < NUM_LEVELS; l++) {
      char tb[16];
      if (s_hs[l][i] == HS_EMPTY) snprintf(tb, sizeof(tb), "--");
      else fmt_time(tb, sizeof(tb), s_hs[l][i]);
      bool played = s_hs_from_game && l == s_level;
      bool fresh = played && !s_lost && i == s_rank;
      if (fresh) {
        // gerade erreichter Eintrag
        graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorWhite));
        graphics_fill_rect(ctx, GRect(rw + l * cw + 4, y, cw - 8, lh - 1), 4, GCornersAll);
      }
      graphics_context_set_text_color(ctx, fresh ? PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)
                                                 : (played ? PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite) : GColorBlack));
      graphics_draw_text(ctx, tb, (i == 0 || fresh) ? hf : vf, GRect(rw + l * cw, y - 2, cw, lh),
                         GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
    graphics_context_set_text_color(ctx, GColorBlack);
    y += lh;
  }

  if (s_hs_from_game) {
    // Ergebnis der Runde + Hinweis
    char tb[16], buf[48];
    fmt_time(tb, sizeof(tb), s_seconds);
    if (s_lost) snprintf(buf, sizeof(buf), "%s", T(S_HS_LOST));
    else snprintf(buf, sizeof(buf), T(S_HS_TIME_FMT), tb);
    int fh = big ? 22 : 18;
    graphics_context_set_fill_color(ctx, s_lost ? PBL_IF_COLOR_ELSE(GColorRed, GColorBlack)
                                                : PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack));
    graphics_fill_rect(ctx, GRect(0, b.size.h - fh, b.size.w, fh), 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, buf, fonts_get_system_font(big ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
                       GRect(0, b.size.h - fh - (big ? 3 : 2), b.size.w, fh + 4), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void hs_click(ClickRecognizerRef rec, void *ctx) {
  window_stack_remove(s_hs_window, true);
}

static void hs_click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, hs_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, hs_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, hs_click);
}

static void hs_window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_hs_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_hs_layer, hs_update_proc);
  layer_add_child(root, s_hs_layer);
}

static void hs_window_unload(Window *w) {
  layer_destroy(s_hs_layer);
}

// ---------------------------------------------------------------------------
// Einstellungen
// ---------------------------------------------------------------------------
static uint16_t set_num_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return 1;
}

static void set_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  menu_cell_basic_draw(ctx, cell, T(S_BACKLIGHT), s_backlight ? T(S_ON) : T(S_OFF), NULL);
}

static void set_select(MenuLayer *m, MenuIndex *idx, void *data) {
  s_backlight = !s_backlight;
  persist_write_bool(PKEY_BACKLIGHT, s_backlight);
  apply_backlight();
  menu_layer_reload_data(m);
}

static void settings_window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_settings_layer = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_settings_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = set_num_rows,
    .draw_row = set_draw_row,
    .select_click = set_select,
  });
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_settings_layer, GColorCobaltBlue, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_settings_layer, w);
  layer_add_child(root, menu_layer_get_layer(s_settings_layer));
}

static void settings_window_unload(Window *w) {
  menu_layer_destroy(s_settings_layer);
}

// ---------------------------------------------------------------------------
// Hauptmenue (Schwierigkeitsauswahl)
// ---------------------------------------------------------------------------
enum { MM_HIGHSCORES = 0, MM_SETTINGS, MM_CONTROLS, MM_EXTRA_COUNT };

static uint16_t mm_num_sections(MenuLayer *m, void *ctx) {
  return 2;
}

static uint16_t mm_num_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return section == 0 ? NUM_LEVELS : MM_EXTRA_COUNT;
}

static int16_t mm_header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void mm_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, section == 0 ? T(S_HDR_NEW) : T(S_HDR_MORE));
}

static void mm_draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  if (idx->section == 0) {
    static char sub[48];
    char tb[16];
    int l = idx->row;
    if (s_hs[l][0] == HS_EMPTY) snprintf(tb, sizeof(tb), "--");
    else fmt_time(tb, sizeof(tb), s_hs[l][0]);
    snprintf(sub, sizeof(sub), T(S_LEVEL_SUB_FMT), LEVEL_MINES[l], tb);
    menu_cell_basic_draw(ctx, cell, T(S_EASY + l), sub, NULL);
  } else {
    menu_cell_basic_draw(ctx, cell, T(S_HIGHSCORES + idx->row), NULL, NULL);
  }
}

static void mm_select(MenuLayer *m, MenuIndex *idx, void *data) {
  if (idx->section == 0) {
    start_game(idx->row);
    return;
  }
  switch (idx->row) {
    case MM_HIGHSCORES:
      s_hs_from_game = false;
      window_stack_push(s_hs_window, true);
      break;
    case MM_SETTINGS: window_stack_push(s_settings_window, true); break;
    default: window_stack_push(s_intro_window, true); break;
  }
}

static void menu_window_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_menu_layer = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = mm_num_sections,
    .get_num_rows = mm_num_rows,
    .get_header_height = mm_header_height,
    .draw_header = mm_draw_header,
    .draw_row = mm_draw_row,
    .select_click = mm_select,
  });
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu_layer, GColorCobaltBlue, GColorWhite);
#endif
  menu_layer_set_click_config_onto_window(s_menu_layer, w);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void menu_window_appear(Window *w) {
  menu_layer_reload_data(s_menu_layer);  // Rekorde aktualisieren
}

static void menu_window_unload(Window *w) {
  menu_layer_destroy(s_menu_layer);
}

#ifdef PBL_TOUCH
// ---------------------------------------------------------------------------
// Touch-Verteilung: Spiel, Highscore-Liste und Steuerung reagieren selbst,
// die Menues uebernimmt die System-Touch-Navigation
// ---------------------------------------------------------------------------
static void touch_handler(const TouchEvent *e, void *ctx) {
  Window *top = window_stack_get_top_window();
  if (top == s_game_window) {
    game_touch(e);
  } else if ((top == s_hs_window || top == s_intro_window) && e->type == TouchEvent_Liftoff) {
    window_stack_remove(top, true);  // Antippen = weiter / zurueck
  }
}

#endif  // PBL_TOUCH

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------
static Window *make_window(WindowHandlers handlers, ClickConfigProvider ccp) {
  Window *w = window_create();
  window_set_window_handlers(w, handlers);
  if (ccp) window_set_click_config_provider(w, ccp);
  return w;
}

static void init(void) {
  srand(time(NULL));
  s_german = strncmp(i18n_get_system_locale(), "de", 2) == 0;
  load_data();
  apply_backlight();

  s_menu_window = make_window((WindowHandlers){
    .load = menu_window_load, .appear = menu_window_appear, .unload = menu_window_unload}, NULL);
  s_intro_window = make_window((WindowHandlers){
    .load = intro_window_load, .unload = intro_window_unload}, intro_click_config);
  s_game_window = make_window((WindowHandlers){
    .load = game_window_load, .unload = game_window_unload}, game_click_config);
  s_hs_window = make_window((WindowHandlers){.load = hs_window_load, .unload = hs_window_unload}, hs_click_config);
  s_settings_window = make_window((WindowHandlers){
    .load = settings_window_load, .unload = settings_window_unload}, NULL);

  // Touch nur auf Uhren mit Touchscreen (Pebble Time 2)
#ifdef PBL_TOUCH
  s_touch = touch_service_is_enabled();
  if (s_touch) {
    app_touch_navigation_enable(true);  // Menues per Finger scrollen/antippen
    window_set_touch_bridge_disabled(s_game_window, true);
    window_set_touch_bridge_disabled(s_hs_window, true);
    window_set_touch_bridge_disabled(s_intro_window, true);
    touch_service_subscribe(touch_handler, NULL);
  }
#endif

  window_stack_push(s_menu_window, false);
}

static void deinit(void) {
  light_enable(false);
#ifdef PBL_TOUCH
  if (s_touch) touch_service_unsubscribe();
#endif
  window_destroy(s_settings_window);
  window_destroy(s_hs_window);
  window_destroy(s_game_window);
  window_destroy(s_intro_window);
  window_destroy(s_menu_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

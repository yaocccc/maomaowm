#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc" 
#endif

typedef struct {
  const char *id;
  const char *title;
  unsigned int tags;
  int isfloating;
  int isfullscreen;
  float scroller_proportion;
  const char *animation_type;
  int isnoborder;
  int monitor;
  int width;
  int height;
} ConfigWinRule;

typedef struct {
  const char *name;   // 显示器名称
  float mfact;        // 主区域比例
  int nmaster;        // 主区域窗口数量
  const char *layout; // 布局名称（字符串）
  int rr;             // 旋转和翻转（假设为整数）
  float scale;        // 显示器缩放比例
  int x, y;           // 显示器位置
} ConfigMonitorRule;

typedef struct {
  uint32_t mod;
  xkb_keysym_t keysym;
  void (*func)(const Arg *);
  Arg arg;
} KeyBinding;

// 定义一个宏来简化默认按键绑定的添加
#define CHVT(n)                                                                \
  {                                                                            \
    WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_##n, chvt, {   \
      .ui = (n)                                                                \
    }                                                                          \
  }

// 默认的按键绑定数组
KeyBinding default_key_bindings[] = {CHVT(1), CHVT(2),  CHVT(3),  CHVT(4),
                                     CHVT(5), CHVT(6),  CHVT(7),  CHVT(8),
                                     CHVT(9), CHVT(10), CHVT(11), CHVT(12)};

typedef struct {
  unsigned int mod;
  unsigned int button;
  void (*func)(const Arg *);
  Arg arg;
} MouseBinding;

typedef struct {
  unsigned int mod;
  unsigned int dir;
  void (*func)(const Arg *);
  Arg arg;
} AxisBinding;

typedef struct {
  int animations;
  char animation_type[10];
  char animation_fade_in;
  float zoom_initial_ratio;
  float fadein_begin_opacity;
  float fadeout_begin_opacity;
  uint32_t animation_duration_move;
  uint32_t animation_duration_open;
  uint32_t animation_duration_tag;
  uint32_t animation_duration_close;
  double animation_curve[4];

  int scroller_structs;
  float scroller_default_proportion;
  int scoller_focus_center;
  float scroller_proportion_preset[3];

  unsigned int new_is_master;
  float default_mfact;
  unsigned int default_nmaster;

  unsigned int hotarea_size;
  unsigned int enable_hotarea;
  unsigned int ov_tab_mode;
  int overviewgappi;
  int overviewgappo;

  unsigned int axis_bind_apply_timeout;
  unsigned int focus_on_activate;
  int bypass_surface_visibility;
  int sloppyfocus;
  int warpcursor;

  /* keyboard */
  int repeat_rate;
  int repeat_delay;
  unsigned int numlockon;

  /* Trackpad */
  int tap_to_click;
  int tap_and_drag;
  int drag_lock;
  int natural_scrolling;
  int disable_while_typing;
  int left_handed;
  int middle_button_emulation;

  int smartgaps;
  unsigned int gappih;
  unsigned int gappiv;
  unsigned int gappoh;
  unsigned int gappov;
  unsigned int borderpx;
  float rootcolor[4];
  float bordercolor[4];
  float focuscolor[4];
  float maxmizescreencolor[4];
  float urgentcolor[4];
  float scratchpadcolor[4];
  float globalcolor[4];

  char autostart[3][256];

  struct {
    int id;
    char layout_name[256];
  } tags[9];

  ConfigWinRule *window_rules;
  int window_rules_count;

  ConfigMonitorRule *monitor_rules; // 动态数组
  int monitor_rules_count;          // 条数

  KeyBinding *key_bindings;
  int key_bindings_count;

  MouseBinding *mouse_bindings;
  int mouse_bindings_count;

  AxisBinding *axis_bindings;
  int axis_bindings_count;

} Config;

typedef void (*FuncType)(const Arg *);
Config config;

int parse_circle_direction(const char *str) {
  // 将输入字符串转换为小写
  char lowerStr[10];
  int i = 0;
  while (str[i] && i < 9) {
    lowerStr[i] = tolower(str[i]);
    i++;
  }
  lowerStr[i] = '\0';

  // 根据转换后的小写字符串返回对应的枚举值
  if (strcmp(lowerStr, "next") == 0) {
    return 1;
  } else {
    return -1;
  }
}

int parse_direction(const char *str) {
  // 将输入字符串转换为小写
  char lowerStr[10];
  int i = 0;
  while (str[i] && i < 9) {
    lowerStr[i] = tolower(str[i]);
    i++;
  }
  lowerStr[i] = '\0';

  // 根据转换后的小写字符串返回对应的枚举值
  if (strcmp(lowerStr, "up") == 0) {
    return UP;
  } else if (strcmp(lowerStr, "down") == 0) {
    return DOWN;
  } else if (strcmp(lowerStr, "left") == 0) {
    return LEFT;
  } else if (strcmp(lowerStr, "right") == 0) {
    return RIGHT;
  } else {
    return UNDIR;
  }
}

long int parse_color(const char *hex_str) {
  char *endptr;
  long int hex_num = strtol(hex_str, &endptr, 16);
  if (*endptr != '\0') {
    return -1;
  }
  return hex_num;
}

// 辅助函数：检查字符串是否以指定的前缀开头（忽略大小写）
static bool starts_with_ignore_case(const char *str, const char *prefix) {
  while (*prefix) {
    if (tolower(*str) != tolower(*prefix)) {
      return false;
    }
    str++;
    prefix++;
  }
  return true;
}

uint32_t parse_mod(const char *mod_str) {
  uint32_t mod = 0;
  char lower_str[64]; // 假设输入的字符串长度不超过 64
  int i = 0;

  // 将 mod_str 转换为全小写
  for (i = 0; mod_str[i] && i < sizeof(lower_str) - 1; i++) {
    lower_str[i] = tolower(mod_str[i]);
  }
  lower_str[i] = '\0'; // 确保字符串以 NULL 结尾

  // 检查修饰键，忽略左右键标识（如 "_l" 和 "_r"）
  if (strstr(lower_str, "super") || strstr(lower_str, "super_l") ||
      strstr(lower_str, "super_r")) {
    mod |= WLR_MODIFIER_LOGO;
  }
  if (strstr(lower_str, "ctrl") || strstr(lower_str, "ctrl_l") ||
      strstr(lower_str, "ctrl_r")) {
    mod |= WLR_MODIFIER_CTRL;
  }
  if (strstr(lower_str, "shift") || strstr(lower_str, "shift_l") ||
      strstr(lower_str, "shift_r")) {
    mod |= WLR_MODIFIER_SHIFT;
  }
  if (strstr(lower_str, "alt") || strstr(lower_str, "alt_l") ||
      strstr(lower_str, "alt_r")) {
    mod |= WLR_MODIFIER_ALT;
  }

  return mod;
}

xkb_keysym_t parse_keysym(const char *keysym_str) {
  if (strcmp(keysym_str, "F1") == 0 || strcmp(keysym_str, "f1") == 0)
    return XKB_KEY_XF86Switch_VT_1;
  else if (strcmp(keysym_str, "F2") == 0 || strcmp(keysym_str, "f2") == 0)
    return XKB_KEY_XF86Switch_VT_2;
  else if (strcmp(keysym_str, "F3") == 0 || strcmp(keysym_str, "f3") == 0)
    return XKB_KEY_XF86Switch_VT_3;
  else if (strcmp(keysym_str, "F4") == 0 || strcmp(keysym_str, "f4") == 0)
    return XKB_KEY_XF86Switch_VT_4;
  else if (strcmp(keysym_str, "F5") == 0 || strcmp(keysym_str, "f5") == 0)
    return XKB_KEY_XF86Switch_VT_5;
  else if (strcmp(keysym_str, "F6") == 0 || strcmp(keysym_str, "f6") == 0)
    return XKB_KEY_XF86Switch_VT_6;
  else if (strcmp(keysym_str, "F7") == 0 || strcmp(keysym_str, "f7") == 0)
    return XKB_KEY_XF86Switch_VT_7;
  else if (strcmp(keysym_str, "F8") == 0 || strcmp(keysym_str, "f8") == 0)
    return XKB_KEY_XF86Switch_VT_8;
  else if (strcmp(keysym_str, "F9") == 0 || strcmp(keysym_str, "f9") == 0)
    return XKB_KEY_XF86Switch_VT_9;
  else if (strcmp(keysym_str, "F10") == 0 || strcmp(keysym_str, "f10") == 0)
    return XKB_KEY_XF86Switch_VT_10;
  else if (strcmp(keysym_str, "F11") == 0 || strcmp(keysym_str, "f11") == 0)
    return XKB_KEY_XF86Switch_VT_11;
  else if (strcmp(keysym_str, "F12") == 0 || strcmp(keysym_str, "f12") == 0)
    return XKB_KEY_XF86Switch_VT_12;
  return xkb_keysym_from_name(keysym_str, XKB_KEYSYM_NO_FLAGS);
}

int parse_button(const char *str) {
  // 将输入字符串转换为小写
  char lowerStr[20];
  int i = 0;
  while (str[i] && i < 19) {
    lowerStr[i] = tolower(str[i]);
    i++;
  }
  lowerStr[i] = '\0'; // 确保字符串正确终止

  // 根据转换后的小写字符串返回对应的按钮编号
  if (strcmp(lowerStr, "btn_left") == 0) {
    return BTN_LEFT;
  } else if (strcmp(lowerStr, "btn_right") == 0) {
    return BTN_RIGHT;
  } else if (strcmp(lowerStr, "btn_middle") == 0) {
    return BTN_MIDDLE;
  } else if (strcmp(lowerStr, "btn_side") == 0) {
    return BTN_SIDE;
  } else if (strcmp(lowerStr, "btn_extra") == 0) {
    return BTN_EXTRA;
  } else if (strcmp(lowerStr, "btn_forward") == 0) {
    return BTN_FORWARD;
  } else if (strcmp(lowerStr, "btn_back") == 0) {
    return BTN_BACK;
  } else if (strcmp(lowerStr, "btn_task") == 0) {
    return BTN_TASK;
  } else {
    return 0;
  }
}

int parse_mouse_action(const char *str) {
  // 将输入字符串转换为小写
  char lowerStr[20];
  int i = 0;
  while (str[i] && i < 19) {
    lowerStr[i] = tolower(str[i]);
    i++;
  }
  lowerStr[i] = '\0'; // 确保字符串正确终止

  // 根据转换后的小写字符串返回对应的按钮编号
  if (strcmp(lowerStr, "curmove") == 0) {
    return CurMove;
  } else if (strcmp(lowerStr, "curresize") == 0) {
    return CurResize;
  } else if (strcmp(lowerStr, "curnormal") == 0) {
    return CurNormal;
  } else if (strcmp(lowerStr, "curpressed") == 0) {
    return CurPressed;
  } else {
    return 0;
  }
}

void convert_hex_to_rgba(float *color, unsigned long int hex) {
  color[0] = ((hex >> 24) & 0xFF) / 255.0f;
  color[1] = ((hex >> 16) & 0xFF) / 255.0f;
  color[2] = ((hex >> 8) & 0xFF) / 255.0f;
  color[3] = (hex & 0xFF) / 255.0f;
}

FuncType parse_func_name(char *func_name, Arg *arg, char *arg_value) {

  FuncType func = NULL;
  (*arg).v = NULL;

  if (strcmp(func_name, "focusstack") == 0) {
    func = focusstack;
    (*arg).i = parse_circle_direction(arg_value);
  } else if (strcmp(func_name, "focusdir") == 0) {
    func = focusdir;
    (*arg).i = parse_direction(arg_value);
  } else if (strcmp(func_name, "incnmaster") == 0) {
    func = incnmaster;
    (*arg).i = atoi(arg_value);
  } else if (strcmp(func_name, "setmfact") == 0) {
    func = setmfact;
    (*arg).f = atof(arg_value);
  } else if (strcmp(func_name, "zoom") == 0) {
    func = zoom;
  } else if (strcmp(func_name, "exchange_client") == 0) {
    func = exchange_client;
    (*arg).i = parse_direction(arg_value);
  } else if (strcmp(func_name, "toggleglobal") == 0) {
    func = toggleglobal;
  } else if (strcmp(func_name, "toggleoverview") == 0) {
    func = toggleoverview;
  } else if (strcmp(func_name, "set_proportion") == 0) {
    func = set_proportion;
    (*arg).f = atof(arg_value);
  } else if (strcmp(func_name, "switch_proportion_preset") == 0) {
    func = switch_proportion_preset;
  } else if (strcmp(func_name, "viewtoleft") == 0) {
    func = viewtoleft;
  } else if (strcmp(func_name, "viewtoright") == 0) {
    func = viewtoright;
  } else if (strcmp(func_name, "tagtoleft") == 0) {
    func = tagtoleft;
  } else if (strcmp(func_name, "tagtoright") == 0) {
    func = tagtoright;
  } else if (strcmp(func_name, "killclient") == 0) {
    func = killclient;
  } else if (strcmp(func_name, "setlayout") == 0) {
    func = setlayout;
    (*arg).v = strdup(arg_value);
  } else if (strcmp(func_name, "switch_layout") == 0) {
    func = switch_layout;
  } else if (strcmp(func_name, "togglefloating") == 0) {
    func = togglefloating;
  } else if (strcmp(func_name, "togglefullscreen") == 0) {
    func = togglefullscreen;
  } else if (strcmp(func_name, "minized") == 0) {
    func = minized;
  } else if (strcmp(func_name, "restore_minized") == 0) {
    func = restore_minized;
  } else if (strcmp(func_name, "toggle_scratchpad") == 0) {
    func = toggle_scratchpad;
  } else if (strcmp(func_name, "focusmon") == 0) {
    func = focusmon;
    (*arg).i = parse_direction(arg_value);
  } else if (strcmp(func_name, "tagmon") == 0) {
    func = tagmon;
    (*arg).i = parse_direction(arg_value);
  } else if (strcmp(func_name, "incgaps") == 0) {
    func = incgaps;
    (*arg).i = atoi(arg_value);
  } else if (strcmp(func_name, "togglegaps") == 0) {
    func = togglegaps;
  } else if (strcmp(func_name, "chvt") == 0) {
    func = chvt;
    (*arg).ui = atoi(arg_value);
  } else if (strcmp(func_name, "spawn") == 0) {
    func = spawn;
    (*arg).v = strdup(arg_value);
  } else if (strcmp(func_name, "quit") == 0) {
    func = quit;
  } else if (strcmp(func_name, "moveresize") == 0) {
    func = moveresize;
    (*arg).ui = parse_mouse_action(arg_value);
  } else if (strcmp(func_name, "togglemaxmizescreen") == 0) {
    func = togglemaxmizescreen;
  } else if (strcmp(func_name, "viewtoleft_have_client") == 0) {
    func = viewtoleft_have_client;
  } else if (strcmp(func_name, "viewtoright_have_client") == 0) {
    func = viewtoright_have_client;
  } else if (strcmp(func_name, "reload_config") == 0) {
    func = reload_config;
  } else if (strcmp(func_name, "tag") == 0) {
    func = tag;
    (*arg).ui = 1 << (atoi(arg_value) - 1);
  } else if (strcmp(func_name, "view") == 0) {
    func = bind_to_view;
    (*arg).ui = 1 << (atoi(arg_value) - 1);
  } else {
    return NULL;
  }
  return func;
}

void parse_config_line(Config *config, const char *line) {
  char key[256], value[256];
  if (sscanf(line, "%[^=]=%[^\n]", key, value) != 2) {
    fprintf(stderr, "Error: Invalid line format: %s\n", line);
    return;
  }

  if (strcmp(key, "animations") == 0) {
    config->animations = atoi(value);
  } else if (strcmp(key, "animation_type") == 0) {
    strncpy(config->animation_type, value, sizeof(config->animation_type));
  } else if (strcmp(key, "animation_fade_in") == 0) {
    config->animation_fade_in = atoi(value);
  } else if (strcmp(key, "zoom_initial_ratio") == 0) {
    config->zoom_initial_ratio = atof(value);
  } else if (strcmp(key, "fadein_begin_opacity") == 0) {
    config->fadein_begin_opacity = atof(value);
  } else if (strcmp(key, "fadeout_begin_opacity") == 0) {
    config->fadeout_begin_opacity = atof(value);
  } else if (strcmp(key, "animation_duration_move") == 0) {
    config->animation_duration_move = atoi(value);
  } else if (strcmp(key, "animation_duration_open") == 0) {
    config->animation_duration_open = atoi(value);
  } else if (strcmp(key, "animation_duration_tag") == 0) {
    config->animation_duration_tag = atoi(value);
  } else if (strcmp(key, "animation_duration_close") == 0) {
    config->animation_duration_close = atoi(value);
  } else if (strcmp(key, "animation_curve") == 0) {
    if (sscanf(value, "%lf,%lf,%lf,%lf", &config->animation_curve[0],
               &config->animation_curve[1], &config->animation_curve[2],
               &config->animation_curve[3]) != 4) {
      fprintf(stderr, "Error: Invalid animation_curve format: %s\n", value);
    }
  } else if (strcmp(key, "scroller_structs") == 0) {
    config->scroller_structs = atoi(value);
  } else if (strcmp(key, "scroller_default_proportion") == 0) {
    config->scroller_default_proportion = atof(value);
  } else if (strcmp(key, "scoller_focus_center") == 0) {
    config->scoller_focus_center = atoi(value);
  } else if (strcmp(key, "scroller_proportion_preset") == 0) {
    if (sscanf(value, "%f,%f,%f", &config->scroller_proportion_preset[0],
               &config->scroller_proportion_preset[1],
               &config->scroller_proportion_preset[2]) != 3) {
      fprintf(stderr, "Error: Invalid scroller_proportion_preset format: %s\n",
              value);
    }
  } else if (strcmp(key, "new_is_master") == 0) {
    config->new_is_master = atoi(value);
  } else if (strcmp(key, "default_mfact") == 0) {
    config->default_mfact = atof(value);
  } else if (strcmp(key, "default_nmaster") == 0) {
    config->default_nmaster = atoi(value);
  } else if (strcmp(key, "hotarea_size") == 0) {
    config->hotarea_size = atoi(value);
  } else if (strcmp(key, "enable_hotarea") == 0) {
    config->enable_hotarea = atoi(value);
  } else if (strcmp(key, "ov_tab_mode") == 0) {
    config->ov_tab_mode = atoi(value);
  } else if (strcmp(key, "overviewgappi") == 0) {
    config->overviewgappi = atoi(value);
  } else if (strcmp(key, "overviewgappo") == 0) {
    config->overviewgappo = atoi(value);
  } else if (strcmp(key, "axis_bind_apply_timeout") == 0) {
    config->axis_bind_apply_timeout = atoi(value);
  } else if (strcmp(key, "focus_on_activate") == 0) {
    config->focus_on_activate = atoi(value);
  } else if (strcmp(key, "numlockon") == 0) {
    config->numlockon = atoi(value);
  } else if (strcmp(key, "bypass_surface_visibility") == 0) {
    config->bypass_surface_visibility = atoi(value);
  } else if (strcmp(key, "sloppyfocus") == 0) {
    config->sloppyfocus = atoi(value);
  } else if (strcmp(key, "warpcursor") == 0) {
    config->warpcursor = atoi(value);
  } else if (strcmp(key, "smartgaps") == 0) {
    config->smartgaps = atoi(value);
  } else if (strcmp(key, "repeat_rate") == 0) {
    config->repeat_rate = atoi(value);
  } else if (strcmp(key, "repeat_delay") == 0) {
    config->repeat_delay = atoi(value);
  } else if (strcmp(key, "tap_to_click") == 0) {
    config->tap_to_click = atoi(value);
  } else if (strcmp(key, "tap_and_drag") == 0) {
    config->tap_and_drag = atoi(value);
  } else if (strcmp(key, "drag_lock") == 0) {
    config->drag_lock = atoi(value);
  } else if (strcmp(key, "natural_scrolling") == 0) {
    config->natural_scrolling = atoi(value);
  } else if (strcmp(key, "disable_while_typing") == 0) {
    config->disable_while_typing = atoi(value);
  } else if (strcmp(key, "left_handed") == 0) {
    config->left_handed = atoi(value);
  } else if (strcmp(key, "middle_button_emulation") == 0) {
    config->middle_button_emulation = atoi(value);
  } else if (strcmp(key, "gappih") == 0) {
    config->gappih = atoi(value);
  } else if (strcmp(key, "gappiv") == 0) {
    config->gappiv = atoi(value);
  } else if (strcmp(key, "gappoh") == 0) {
    config->gappoh = atoi(value);
  } else if (strcmp(key, "gappov") == 0) {
    config->gappov = atoi(value);
  } else if (strcmp(key, "borderpx") == 0) {
    config->borderpx = atoi(value);
  } else if (strcmp(key, "rootcolor") == 0) {
    long int color = parse_color(value);
    if (color == -1) {
      fprintf(stderr, "Error: Invalid rootcolor format: %s\n", value);
    } else {
      convert_hex_to_rgba(config->rootcolor, color);
    }
  } else if (strcmp(key, "bordercolor") == 0) {
    long int color = parse_color(value);
    if (color == -1) {
      fprintf(stderr, "Error: Invalid bordercolor format: %s\n", value);
    } else {
      convert_hex_to_rgba(config->bordercolor, color);
    }
  } else if (strcmp(key, "focuscolor") == 0) {
    long int color = parse_color(value);
    if (color == -1) {
      fprintf(stderr, "Error: Invalid focuscolor format: %s\n", value);
    } else {
      convert_hex_to_rgba(config->focuscolor, color);
    }
  } else if (strcmp(key, "maxmizescreencolor") == 0) {
    long int color = parse_color(value);
    if (color == -1) {
      fprintf(stderr, "Error: Invalid maxmizescreencolor format: %s\n", value);
    } else {
      convert_hex_to_rgba(config->maxmizescreencolor, color);
    }
  } else if (strcmp(key, "urgentcolor") == 0) {
    long int color = parse_color(value);
    if (color == -1) {
      fprintf(stderr, "Error: Invalid urgentcolor format: %s\n", value);
    } else {
      convert_hex_to_rgba(config->urgentcolor, color);
    }
  } else if (strcmp(key, "scratchpadcolor") == 0) {
    long int color = parse_color(value);
    if (color == -1) {
      fprintf(stderr, "Error: Invalid scratchpadcolor format: %s\n", value);
    } else {
      convert_hex_to_rgba(config->scratchpadcolor, color);
    }
  } else if (strcmp(key, "globalcolor") == 0) {
    long int color = parse_color(value);
    if (color == -1) {
      fprintf(stderr, "Error: Invalid globalcolor format: %s\n", value);
    } else {
      convert_hex_to_rgba(config->globalcolor, color);
    }
  } else if (strcmp(key, "autostart") == 0) {
    if (sscanf(value, "%[^,],%[^,],%[^,]", config->autostart[0],
               config->autostart[1], config->autostart[2]) != 3) {
      fprintf(stderr, "Error: Invalid autostart format: %s\n", value);
    }
  } else if (strcmp(key, "tags") == 0) {
    int id;
    char layout_name[256];
    if (sscanf(value, "id:%d,layout_name:%255[^\n]", &id, layout_name) == 2) {
      if (id >= 1 && id <= 9) {
        config->tags[id - 1].id = id;
        strncpy(config->tags[id - 1].layout_name, layout_name,
                sizeof(config->tags[id - 1].layout_name));
      } else {
        fprintf(stderr, "Error: Invalid tag id: %d\n", id);
      }
    } else {
      fprintf(stderr, "Error: Invalid tags format: %s\n", value);
    }
  } else if (strcmp(key, "windowrule") == 0) {
    config->window_rules =
        realloc(config->window_rules,
                (config->window_rules_count + 1) * sizeof(ConfigWinRule));
    if (!config->window_rules) {
      fprintf(stderr, "Error: Failed to allocate memory for window rules\n");
      return;
    }

    ConfigWinRule *rule = &config->window_rules[config->window_rules_count];
    memset(rule, 0, sizeof(ConfigWinRule));

    rule->isfloating = -1;
    rule->isfullscreen = -1;
    rule->isnoborder = -1;
    rule->monitor = -1;
    rule->width = -1;
    rule->height = -1;
    rule->animation_type = NULL;
    rule->scroller_proportion = -1;
    rule->id = NULL;
    rule->title = NULL;
    rule->tags = 0;

    char *token = strtok(value, ",");
    while (token != NULL) {
      char *colon = strchr(token, ':');
      if (colon != NULL) {
        *colon = '\0';
        char *key = token;
        char *val = colon + 1;

        if (strcmp(key, "isfloating") == 0) {
          rule->isfloating = atoi(val);
        } else if (strcmp(key, "title") == 0) {
          rule->title = strdup(val);
        } else if (strcmp(key, "appid") == 0) {
          rule->id = strdup(val);
        } else if (strcmp(key, "animation_type") == 0) {
          rule->animation_type = strdup(val);
        } else if (strcmp(key, "tags") == 0) {
          rule->tags = 1 << (atoi(val) - 1);
        } else if (strcmp(key, "monitor") == 0) {
          rule->monitor = atoi(val);
        } else if (strcmp(key, "width") == 0) {
          rule->width = atoi(val);
        } else if (strcmp(key, "height") == 0) {
          rule->height = atoi(val);
        } else if (strcmp(key, "isnoborder") == 0) {
          rule->isnoborder = atoi(val);
        } else if (strcmp(key, "scroller_proportion") == 0) {
          rule->scroller_proportion = atof(val);
        } else if (strcmp(key, "isfullscreen") == 0) {
          rule->isfullscreen = atoi(val);
        }
      }
      token = strtok(NULL, ",");
    }
    config->window_rules_count++;
  } else if (strcmp(key, "monitorrule") == 0) {
    config->monitor_rules =
        realloc(config->monitor_rules,
                (config->monitor_rules_count + 1) * sizeof(ConfigMonitorRule));
    if (!config->monitor_rules) {
      fprintf(stderr, "Error: Failed to allocate memory for monitor rules\n");
      return;
    }

    ConfigMonitorRule *rule =
        &config->monitor_rules[config->monitor_rules_count];
    memset(rule, 0, sizeof(ConfigMonitorRule));

    char layout[256], name[256];
    int parsed = sscanf(value, "%255[^,],%f,%d,%255[^,],%d,%f,%d,%d", name,
                        &rule->mfact, &rule->nmaster, layout, &rule->rr,
                        &rule->scale, &rule->x, &rule->y);

    if (parsed == 8) {
      rule->name = strdup(name);
      rule->layout = strdup(layout);

      if (!rule->name || !rule->layout) {
        if (rule->name)
          free((void *)rule->name);
        if (rule->layout)
          free((void *)rule->layout);
        fprintf(stderr, "Error: Failed to allocate memory for monitor rule\n");
        return;
      }

      config->monitor_rules_count++;
    } else {
      fprintf(stderr, "Error: Invalid monitorrule format: %s\n", value);
    }
  } else if (strncmp(key, "bind", 4) == 0) {
    config->key_bindings =
        realloc(config->key_bindings,
                (config->key_bindings_count + 1) * sizeof(KeyBinding));
    if (!config->key_bindings) {
      fprintf(stderr, "Error: Failed to allocate memory for key bindings\n");
      return;
    }

    KeyBinding *binding = &config->key_bindings[config->key_bindings_count];
    memset(binding, 0, sizeof(KeyBinding));

    char mod_str[256], keysym_str[256], func_name[256], arg_value[256] = "none";
    if (sscanf(value, "%[^,],%[^,],%[^,],%[^\n]", mod_str, keysym_str,
               func_name, arg_value) < 3) {
      fprintf(stderr, "Error: Invalid bind format: %s\n", value);
      return;
    }

    binding->mod = parse_mod(mod_str);
    binding->keysym = parse_keysym(keysym_str);
    binding->arg.v = NULL;
    binding->func = parse_func_name(func_name, &binding->arg, arg_value);
    if (!binding->func) {
      fprintf(stderr, "Error: Unknown function in bind: %s\n", func_name);
    } else {
      config->key_bindings_count++;
    }

    // 计算默认按键绑定的数量
    size_t default_key_bindings_count =
        sizeof(default_key_bindings) / sizeof(KeyBinding);

    // 重新分配内存以容纳新的默认按键绑定
    config->key_bindings =
        realloc(config->key_bindings,
                (config->key_bindings_count + default_key_bindings_count) *
                    sizeof(KeyBinding));
    if (!config->key_bindings) {
      return;
    }

    // 将默认按键绑定复制到配置的按键绑定数组中
    for (size_t i = 0; i < default_key_bindings_count; i++) {
      config->key_bindings[config->key_bindings_count + i] =
          default_key_bindings[i];
    }

    // 更新按键绑定的总数
    config->key_bindings_count += default_key_bindings_count;

  } else if (strncmp(key, "mousebind", 9) == 0) {
    config->mouse_bindings =
        realloc(config->mouse_bindings,
                (config->mouse_bindings_count + 1) * sizeof(MouseBinding));
    if (!config->mouse_bindings) {
      fprintf(stderr, "Error: Failed to allocate memory for mouse bindings\n");
      return;
    }

    MouseBinding *binding =
        &config->mouse_bindings[config->mouse_bindings_count];
    memset(binding, 0, sizeof(MouseBinding));

    char mod_str[256], button_str[256], func_name[256], arg_value[256] = "none";
    if (sscanf(value, "%[^,],%[^,],%[^,],%[^\n]", mod_str, button_str,
               func_name, arg_value) < 3) {
      fprintf(stderr, "Error: Invalid mousebind format: %s\n", value);
      return;
    }

    binding->mod = parse_mod(mod_str);
    binding->button = parse_button(button_str);
    binding->arg.v = NULL;
    binding->func = parse_func_name(func_name, &binding->arg, arg_value);
    if (!binding->func) {
      fprintf(stderr, "Error: Unknown function in mousebind: %s\n", func_name);
    } else {
      config->mouse_bindings_count++;
    }
  } else if (strncmp(key, "axisbind", 8) == 0) {
    config->axis_bindings =
        realloc(config->axis_bindings,
                (config->axis_bindings_count + 1) * sizeof(AxisBinding));
    if (!config->axis_bindings) {
      fprintf(stderr, "Error: Failed to allocate memory for axis bindings\n");
      return;
    }

    AxisBinding *binding = &config->axis_bindings[config->axis_bindings_count];
    memset(binding, 0, sizeof(AxisBinding));

    char mod_str[256], dir_str[256], func_name[256], arg_value[256] = "none";
    if (sscanf(value, "%[^,],%[^,],%[^,],%[^\n]", mod_str, dir_str, func_name,
               arg_value) < 3) {
      fprintf(stderr, "Error: Invalid axisbind format: %s\n", value);
      return;
    }

    binding->mod = parse_mod(mod_str);
    binding->dir = parse_direction(dir_str);
    binding->arg.v = NULL;
    binding->func = parse_func_name(func_name, &binding->arg, arg_value);

    if (!binding->func) {
      fprintf(stderr, "Error: Unknown function in axisbind: %s\n", func_name);
    } else {
      config->axis_bindings_count++;
    }

  } else {
    fprintf(stderr, "Error: Unknown key: %s\n", key);
  }
}

void parse_config_file(Config *config, const char *file_path) {
  FILE *file = fopen(file_path, "r");
  if (!file) {
    perror("Error opening file");
    return;
  }

  char line[512];
  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#' || line[0] == '\n')
      continue;
    parse_config_line(config, line);
  }

  fclose(file);
}

void free_config(void) {
  // 释放内存
  int i;
  for (i = 0; i < config.window_rules_count; i++) {
    ConfigWinRule *rule = &config.window_rules[i];
    if (rule->id)
      free((void *)rule->id);
    if (rule->title)
      free((void *)rule->title);
    if (rule->animation_type)
      free((void *)rule->animation_type);
  }
  free(config.window_rules);

  for (i = 0; i < config.monitor_rules_count; i++) {
    ConfigMonitorRule *rule = &config.monitor_rules[i];
    if (rule->name)
      free((void *)rule->name);
    if (rule->layout)
      free((void *)rule->layout);
  }
  free(config.monitor_rules);

  for (i = 0; i < config.key_bindings_count; i++) {
    if (config.key_bindings[i].arg.v) {
      free((void *)config.key_bindings[i].arg.v);
      config.key_bindings[i].arg.v = NULL; // 避免重复释放
    }
  }
  free(config.key_bindings);

  for (i = 0; i < config.mouse_bindings_count; i++) {
    if (config.mouse_bindings[i].arg.v) {
      free((void *)config.mouse_bindings[i].arg.v);
      config.mouse_bindings[i].arg.v = NULL; // 避免重复释放
    }
  }
  free(config.mouse_bindings);

  for (i = 0; i < config.axis_bindings_count; i++) {
    if (config.axis_bindings[i].arg.v) {
      free((void *)config.axis_bindings[i].arg.v);
      config.axis_bindings[i].arg.v = NULL; // 避免重复释放
    }
  }
  free(config.axis_bindings);
}

void override_config(void) {
  animations = config.animations;
  animation_type = config.animation_type;
  animation_fade_in = config.animation_fade_in;
  zoom_initial_ratio = config.zoom_initial_ratio;
  fadein_begin_opacity = config.fadein_begin_opacity;
  fadeout_begin_opacity = config.fadeout_begin_opacity;
  animation_duration_move = config.animation_duration_move;
  animation_duration_open = config.animation_duration_open;
  animation_duration_tag = config.animation_duration_tag;
  animation_duration_close = config.animation_duration_close;

  // 复制数组类型的变量
  memcpy(animation_curve, config.animation_curve, sizeof(animation_curve));
  memcpy(scroller_proportion_preset, config.scroller_proportion_preset,
         sizeof(scroller_proportion_preset));

  scroller_structs = config.scroller_structs;
  scroller_default_proportion = config.scroller_default_proportion;
  scoller_focus_center = config.scoller_focus_center;

  new_is_master = config.new_is_master;
  default_mfact = config.default_mfact;
  default_nmaster = config.default_nmaster;
  hotarea_size = config.hotarea_size;
  enable_hotarea = config.enable_hotarea;
  ov_tab_mode = config.ov_tab_mode;
  overviewgappi = config.overviewgappi;
  overviewgappo = config.overviewgappo;
  axis_bind_apply_timeout = config.axis_bind_apply_timeout;
  focus_on_activate = config.focus_on_activate;
  numlockon = config.numlockon;
  bypass_surface_visibility = config.bypass_surface_visibility;
  sloppyfocus = config.sloppyfocus;
  warpcursor = config.warpcursor;
  smartgaps = config.smartgaps;
  gappih = config.gappih;
  gappiv = config.gappiv;
  gappoh = config.gappoh;
  gappov = config.gappov;
  borderpx = config.borderpx;

  // 复制颜色数组
  memcpy(rootcolor, config.rootcolor, sizeof(rootcolor));
  memcpy(bordercolor, config.bordercolor, sizeof(bordercolor));
  memcpy(focuscolor, config.focuscolor, sizeof(focuscolor));
  memcpy(maxmizescreencolor, config.maxmizescreencolor,
         sizeof(maxmizescreencolor));
  memcpy(urgentcolor, config.urgentcolor, sizeof(urgentcolor));
  memcpy(scratchpadcolor, config.scratchpadcolor, sizeof(scratchpadcolor));
  memcpy(globalcolor, config.globalcolor, sizeof(globalcolor));
}

void set_value_default() {
  /* animaion */
  config.animations = 1;             // 是否启用动画
  config.animation_fade_in = 1;      // Enable animation fade in
  config.zoom_initial_ratio = 0.5;   // 动画起始窗口比例
  config.fadein_begin_opacity = 0.5; // Begin opac window ratio for animations
  config.fadeout_begin_opacity = 0.5;
  config.animation_duration_move = 500;  // Animation move speed
  config.animation_duration_open = 400;  // Animation open speed
  config.animation_duration_tag = 300;   // Animation tag speed
  config.animation_duration_close = 300; // Animation tag speed

  /* appearance */
  config.axis_bind_apply_timeout = 100; // 滚轮绑定动作的触发的时间间隔
  config.focus_on_activate = 1;         // 收到窗口激活请求是否自动跳转聚焦
  config.new_is_master = 1;             // 新窗口是否插在头部
  config.default_mfact = 0.55f;         // master 窗口比例
  config.default_nmaster = 1;           // 默认master数量

  config.numlockon = 1; // 是否打开右边小键盘

  config.ov_tab_mode = 0;    // alt tab切换模式
  config.hotarea_size = 10;  // 热区大小,10x10
  config.enable_hotarea = 1; // 是否启用鼠标热区
  config.smartgaps = 0; /* 1 means no outer gap when there is only one window */
  config.sloppyfocus = 1; /* focus follows mouse */
  config.gappih = 5;      /* horiz inner gap between windows */
  config.gappiv = 5;      /* vert inner gap between windows */
  config.gappoh = 10;     /* horiz outer gap between windows and screen edge */
  config.gappov = 10;     /* vert outer gap between windows and screen edge */

  config.scroller_structs = 20;
  config.scroller_default_proportion = 0.9;
  config.scoller_focus_center = 0;

  config.bypass_surface_visibility =
      0; /* 1 means idle inhibitors will disable idle tracking even if it's
            surface isn't visible  */

  config.overviewgappi = 5;  /* overview时 窗口与边缘 缝隙大小 */
  config.overviewgappo = 30; /* overview时 窗口与窗口 缝隙大小 */

  config.warpcursor = 1; /* Warp cursor to focused client */

  config.repeat_rate = 25;
  config.repeat_delay = 600;

  /* Trackpad */
  config.tap_to_click = 1;
  config.tap_and_drag = 1;
  config.drag_lock = 1;
  config.natural_scrolling = 0;
  config.disable_while_typing = 1;
  config.left_handed = 0;
  config.middle_button_emulation = 0;
}

void parse_config(void) {

  char filename[1024];

  memset(&config, 0, sizeof(config));
  config.window_rules = NULL;
  config.window_rules_count = 0;
  config.monitor_rules = NULL;
  config.monitor_rules_count = 0;
  config.key_bindings = NULL;
  config.key_bindings_count = 0;
  config.mouse_bindings = NULL;
  config.mouse_bindings_count = 0;
  config.axis_bindings = NULL;
  config.axis_bindings_count = 0;

  // 获取 MAOMAOCONFIG 环境变量
  const char *maomaoconfig = getenv("MAOMAOCONFIG");

  // 如果 MAOMAOCONFIG 环境变量不存在或为空，则使用 HOME 环境变量
  if (!maomaoconfig || maomaoconfig[0] == '\0') {
    // 获取当前用户家目录
    const char *homedir = getenv("HOME");
    if (!homedir) {
      // 如果获取失败，则无法继续
      return;
    }
    // 构建日志文件路径
    snprintf(filename, sizeof(filename), "%s/.config/maomao/config.conf",
             homedir);

    // 检查文件是否存在
    if (access(filename, F_OK) != 0) {
      // 如果文件不存在，则使用 /etc/maomao/config.conf
      snprintf(filename, sizeof(filename), "%s/maomao/config.conf",SYSCONFDIR);
    }
  } else {
    // 使用 MAOMAOCONFIG 环境变量作为配置文件夹路径
    snprintf(filename, sizeof(filename), "%s/config.conf", maomaoconfig);
  }

  set_value_default();
  parse_config_file(&config, filename);
  override_config();
}

void reload_config(const Arg *arg) {
  Client *c;
  free_config();
  parse_config();
  init_baked_points();
  wl_list_for_each(c, &clients, link) {
    if (c && !c->iskilling) {
      if (c->bw) {
        c->bw = borderpx;
      }
    }
  }
  arrange(selmon, false);
}
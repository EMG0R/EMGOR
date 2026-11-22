#include "watch_state.h"
#include <string.h>

WatchState g_ws;
static SemaphoreHandle_t s_mutex;

void ws_init() {
  memset(&g_ws, 0, sizeof(g_ws));
  g_ws.battery_pct = -1;
  g_ws.phone_batt_pct = -1;
  g_ws.mac_batt_pct = -1;
  g_ws.mu_active_src = WS_SRC_NONE;
  s_mutex = xSemaphoreCreateMutex();
}

void ws_lock()   { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void ws_unlock() { xSemaphoreGive(s_mutex); }

void ws_mark_dirty(uint32_t bits) { g_ws.dirty |= bits; }

uint32_t ws_take_dirty() {
  ws_lock();
  uint32_t d = g_ws.dirty;
  g_ws.dirty = 0;
  ws_unlock();
  return d;
}

static void copy_str(char *dst, const char *src, size_t cap) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

void ws_push_notification(uint32_t id, const char *src, const char *title, const char *body) {
  // Advance ring head.
  g_ws.notif_head = (g_ws.notif_head + 1) % WS_NOTIF_MAX;
  WsNotification &n = g_ws.notif[g_ws.notif_head];
  n.id = id;
  copy_str(n.src,   src,   WS_STR);
  copy_str(n.title, title, WS_STR);
  copy_str(n.body,  body,  WS_BODY);
  n.used = true;
  if (g_ws.notif_count < WS_NOTIF_MAX) g_ws.notif_count++;
}

void ws_remove_notification(uint32_t id) {
  for (int i = 0; i < WS_NOTIF_MAX; i++) {
    if (g_ws.notif[i].used && g_ws.notif[i].id == id) {
      g_ws.notif[i].used = false;
    }
  }
}

// Set a KWin output to an arbitrary resolution+refresh via the kde-output-management-v2 Wayland protocol directly.
//
// libkscreen (kscreen-doctor's backend) has a bug: its setWlConfig gates the set_custom_modes request on a
// change-comparison that wrongly evaluates false, so adding a custom mode never reaches the wire (verified with
// WAYLAND_DEBUG: only wl_registry.bind is sent). KWin's server side is correct, so we speak the protocol
// ourselves: create_mode_list -> set_resolution/set_refresh_rate/add_mode -> set_custom_modes -> apply (adds the
// mode; KWin then advertises it), then a second configuration selecting that mode (resizes the output). The mode
// carries refresh, so this sets resolution AND refresh. Usage: kde-set-mode <output-name> <width> <height> <fps>.

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

#include "kde-output-device-v2-client-protocol.h"
#include "kde-output-management-v2-client-protocol.h"

static void noop(void) {}
#define STUB ((void*) noop)

#define MAX_DEV 16
#define MAX_MODES 128

struct mode_rec {
  struct kde_output_device_mode_v2* obj;
  int32_t w, h, refresh;  // refresh in mHz
};

struct dev_rec {
  struct kde_output_device_v2* dev;
  char name[128];
  struct mode_rec modes[MAX_MODES];
  int n_modes;
};

static struct dev_rec g_devs[MAX_DEV];
static int g_ndev;
static struct kde_output_management_v2* g_mgmt;
static int g_result = -1;  // -1 pending, 1 applied, 0 failed

// ---- device mode object listener ----
static void mode_size(void* data, struct kde_output_device_mode_v2* m, int32_t w, int32_t h) {
  (void) m;
  struct mode_rec* r = data;
  r->w = w;
  r->h = h;
}
static void mode_refresh(void* data, struct kde_output_device_mode_v2* m, int32_t refresh) {
  (void) m;
  ((struct mode_rec*) data)->refresh = refresh;
}
static const struct kde_output_device_mode_v2_listener mode_listener = {
    .size = mode_size,
    .refresh = mode_refresh,
    .preferred = STUB,
    .removed = STUB,
    .flags = STUB,
};

// ---- device listener (only name/mode/done used; rest stubbed) ----
static void dev_name(void* data, struct kde_output_device_v2* d, const char* name) {
  (void) d;
  struct dev_rec* dr = data;
  snprintf(dr->name, sizeof(dr->name), "%s", name);
}
static void dev_mode(void* data, struct kde_output_device_v2* d, struct kde_output_device_mode_v2* mode) {
  (void) d;
  struct dev_rec* dr = data;
  if (dr->n_modes < MAX_MODES) {
    struct mode_rec* r = &dr->modes[dr->n_modes++];
    r->obj = mode;
    r->w = r->h = r->refresh = 0;
    kde_output_device_mode_v2_add_listener(mode, &mode_listener, r);
  }
}
static const struct kde_output_device_v2_listener dev_listener = {
    .geometry = STUB,
    .current_mode = STUB,
    .mode = dev_mode,
    .done = STUB,
    .scale = STUB,
    .edid = STUB,
    .enabled = STUB,
    .uuid = STUB,
    .serial_number = STUB,
    .eisa_id = STUB,
    .capabilities = STUB,
    .overscan = STUB,
    .vrr_policy = STUB,
    .rgb_range = STUB,
    .name = dev_name,
    .high_dynamic_range = STUB,
    .sdr_brightness = STUB,
    .wide_color_gamut = STUB,
    .auto_rotate_policy = STUB,
    .icc_profile_path = STUB,
    .brightness_metadata = STUB,
    .brightness_overrides = STUB,
    .sdr_gamut_wideness = STUB,
    .color_profile_source = STUB,
    .brightness = STUB,
    .color_power_tradeoff = STUB,
    .dimming = STUB,
    .replication_source = STUB,
    .ddc_ci_allowed = STUB,
    .max_bits_per_color = STUB,
    .max_bits_per_color_range = STUB,
    .automatic_max_bits_per_color_limit = STUB,
    .edr_policy = STUB,
    .sharpness = STUB,
    .priority = STUB,
    .auto_brightness = STUB,
    .removed = STUB,
    .hdr_icc_profile_path = STUB,
    .hdr_color_profile_source = STUB,
    .abm_level = STUB,
};

// ---- configuration result listener ----
static void cfg_applied(void* data, struct kde_output_configuration_v2* c) {
  (void) data;
  (void) c;
  g_result = 1;
}
static void cfg_failed(void* data, struct kde_output_configuration_v2* c) {
  (void) data;
  (void) c;
  g_result = 0;
}
static const struct kde_output_configuration_v2_listener cfg_listener = {
    .applied = cfg_applied,
    .failed = cfg_failed,
};

// ---- registry ----
static void reg_global(void* data, struct wl_registry* reg, uint32_t name, const char* iface, uint32_t version) {
  (void) data;
  if (strcmp(iface, kde_output_management_v2_interface.name) == 0) {
    uint32_t v = version < (uint32_t) kde_output_management_v2_interface.version
                     ? version
                     : (uint32_t) kde_output_management_v2_interface.version;
    g_mgmt = wl_registry_bind(reg, name, &kde_output_management_v2_interface, v);
  } else if (strcmp(iface, kde_output_device_v2_interface.name) == 0 && g_ndev < MAX_DEV) {
    uint32_t v = version < (uint32_t) kde_output_device_v2_interface.version
                     ? version
                     : (uint32_t) kde_output_device_v2_interface.version;
    struct dev_rec* dr = &g_devs[g_ndev++];
    dr->dev = wl_registry_bind(reg, name, &kde_output_device_v2_interface, v);
    dr->name[0] = '\0';
    dr->n_modes = 0;
    kde_output_device_v2_add_listener(dr->dev, &dev_listener, dr);
  }
}
static const struct wl_registry_listener reg_listener = {
    .global = reg_global,
    .global_remove = STUB,
};

static struct mode_rec* find_mode(struct dev_rec* d, int w, int h, int refresh_mhz) {
  for (int i = 0; i < d->n_modes; i++) {
    struct mode_rec* m = &d->modes[i];
    if (m->w == w && m->h == h && abs(m->refresh - refresh_mhz) < 1000) {
      return m;
    }
  }
  return NULL;
}

static int wait_result(struct wl_display* dpy) {
  g_result = -1;
  while (g_result == -1) {
    if (wl_display_dispatch(dpy) < 0) {
      return -1;
    }
  }
  return g_result;
}

int main(int argc, char** argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: kde-set-mode <output> <width> <height> <fps>\n");
    return 2;
  }
  const char* target = argv[1];
  const int w = atoi(argv[2]);
  const int h = atoi(argv[3]);
  const int refresh_mhz = atoi(argv[4]) * 1000;
  if (w <= 0 || h <= 0 || refresh_mhz <= 0) {
    fprintf(stderr, "invalid geometry\n");
    return 2;
  }

  struct wl_display* dpy = wl_display_connect(NULL);
  if (!dpy) {
    fprintf(stderr, "cannot connect to wayland display\n");
    return 1;
  }
  struct wl_registry* reg = wl_display_get_registry(dpy);
  wl_registry_add_listener(reg, &reg_listener, NULL);
  wl_display_roundtrip(dpy);  // globals
  if (!g_mgmt) {
    fprintf(stderr, "kde_output_management_v2 not available\n");
    return 1;
  }
  wl_display_roundtrip(dpy);  // device events (name, modes)

  struct dev_rec* dev = NULL;
  for (int i = 0; i < g_ndev; i++) {
    if (strcmp(g_devs[i].name, target) == 0) {
      dev = &g_devs[i];
      break;
    }
  }
  if (!dev) {
    fprintf(stderr, "output '%s' not found\n", target);
    return 1;
  }

  struct mode_rec* m = find_mode(dev, w, h, refresh_mhz);
  if (!m) {
    // Apply #1: add the custom mode. KWin then advertises it as a real mode object.
    struct kde_mode_list_v2* list = kde_output_management_v2_create_mode_list(g_mgmt);
    kde_mode_list_v2_set_resolution(list, w, h);
    kde_mode_list_v2_set_refresh_rate(list, refresh_mhz);
    kde_mode_list_v2_set_reduced_blanking(list, 0);
    kde_mode_list_v2_add_mode(list);
    struct kde_output_configuration_v2* cfg = kde_output_management_v2_create_configuration(g_mgmt);
    kde_output_configuration_v2_add_listener(cfg, &cfg_listener, NULL);
    kde_output_configuration_v2_set_custom_modes(cfg, dev->dev, list);
    kde_output_configuration_v2_apply(cfg);
    kde_mode_list_v2_destroy(list);
    if (wait_result(dpy) != 1) {
      fprintf(stderr, "add custom mode: apply failed\n");
      return 1;
    }
    kde_output_configuration_v2_destroy(cfg);
    wl_display_roundtrip(dpy);  // receive the re-advertised mode
    m = find_mode(dev, w, h, refresh_mhz);
    if (!m) {
      fprintf(stderr, "custom mode not advertised after add\n");
      return 1;
    }
  }

  // Apply #2: select the mode (resizes the output).
  struct kde_output_configuration_v2* cfg = kde_output_management_v2_create_configuration(g_mgmt);
  kde_output_configuration_v2_add_listener(cfg, &cfg_listener, NULL);
  kde_output_configuration_v2_mode(cfg, dev->dev, m->obj);
  kde_output_configuration_v2_apply(cfg);
  if (wait_result(dpy) != 1) {
    fprintf(stderr, "select mode: apply failed\n");
    return 1;
  }
  printf("ok: %s -> %dx%d@%d\n", target, w, h, refresh_mhz / 1000);
  return 0;
}

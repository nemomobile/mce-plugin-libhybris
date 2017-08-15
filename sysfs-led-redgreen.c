/** @file sysfs-led-redgreen.c
 *
 * mce-plugin-libhybris - Libhybris plugin for Mode Control Entity
 * <p>
 * Copyright (C) 2017 Jolla Ltd.
 * <p>
 * @author Simo Piiroinen <simo.piiroinen@jollamobile.com>
 *
 * mce-plugin-libhybris is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * mce-plugin-libhybris is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with mce-plugin-libhybris; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* ========================================================================= *
 * Red + green led control
 *
 * Two channels (red and green), both of which:
 * - must have 'brightness' control file
 * - must have 'max_brightness' control file
 *
 * Notes:
 * - despite having wide max_brightness range, the brightness control
 *   is binary: zero for off and any non-zero value for on.
 * ========================================================================= */

#include "sysfs-led-redgreen.h"

#include "sysfs-led-util.h"

#include <stdio.h>

#include <glib.h>

/* ========================================================================= *
 * PROTOTYPES
 * ========================================================================= */

typedef struct
{
  const char *max;    // R
  const char *val;    // W
} led_paths_redgreen_t;

typedef struct
{
  int maxval;
  int fd_val;
} led_channel_redgreen_t;

/* ------------------------------------------------------------------------- *
 * ONE_CHANNEL
 * ------------------------------------------------------------------------- */

static void        led_channel_redgreen_init        (led_channel_redgreen_t *self);
static void        led_channel_redgreen_close       (led_channel_redgreen_t *self);
static bool        led_channel_redgreen_probe       (led_channel_redgreen_t *self, const led_paths_redgreen_t *path);
static void        led_channel_redgreen_set_value   (const led_channel_redgreen_t *self, int value);

/* ------------------------------------------------------------------------- *
 * ALL_CHANNELS
 * ------------------------------------------------------------------------- */

static void        led_control_redgreen_map_color   (int r, int g, int b, int *red, int *green);
static void        led_control_redgreen_value_cb    (void *data, int r, int g, int b);
static void        led_control_redgreen_close_cb    (void *data);

bool               led_control_redgreen_probe       (led_control_t *self);

/* ========================================================================= *
 * ONE_CHANNEL
 * ========================================================================= */

static void
led_channel_redgreen_init(led_channel_redgreen_t *self)
{
  self->maxval   = -1;
  self->fd_val   = -1;
}

static void
led_channel_redgreen_close(led_channel_redgreen_t *self)
{
  led_util_close_file(&self->fd_val);
}

static bool
led_channel_redgreen_probe(led_channel_redgreen_t *self,
                            const led_paths_redgreen_t *path)
{
  bool res = false;

  led_channel_redgreen_close(self);

  if( (self->maxval = led_util_read_number(path->max)) <= 0 )
  {
    goto cleanup;
  }

  if( !led_util_open_file(&self->fd_val,   path->val) )
  {
    goto cleanup;
  }

  res = true;

cleanup:

  if( !res ) led_channel_redgreen_close(self);

  return res;
}

static void
led_channel_redgreen_set_value(const led_channel_redgreen_t *self,
                                int value)
{
  if( self->fd_val != -1 )
  {
    dprintf(self->fd_val, "%d", led_util_scale_value(value, self->maxval));
  }
}

/* ========================================================================= *
 * ALL_CHANNELS
 * ========================================================================= */

static void
led_control_redgreen_map_color(int r, int g, int b, int *red, int *green)
{
  /* If the pattern defines red and/or green intensities, those should
   * be used. Otherwise make sure that requesting for blue only colour
   * does not result in the led being turned off. */
  if( r || g )
  {
    *red   = r;
    *green = g;
  }
  else
  {
    *red   = b;
    *green = b;
  }
}

static void
led_control_redgreen_value_cb(void *data, int r, int g, int b)
{
  const led_channel_redgreen_t *channel = data;

  int red   = 0;
  int green = 0;
  led_control_redgreen_map_color(r, g, b, &red, &green);

  led_channel_redgreen_set_value(channel + 0, red);
  led_channel_redgreen_set_value(channel + 1, green);
}

static void
led_control_redgreen_close_cb(void *data)
{
  led_channel_redgreen_t *channel = data;
  led_channel_redgreen_close(channel + 0);
  led_channel_redgreen_close(channel + 1);
}

bool
led_control_redgreen_probe(led_control_t *self)
{
  /** Sysfs control paths for Red + Green leds */
  static const led_paths_redgreen_t paths[][2] =
  {
    // "standard" paths
    {
      {
        .max   = "/sys/class/leds/red/max_brightness",
        .val   = "/sys/class/leds/red/brightness",
      },
      {
        .max   = "/sys/class/leds/green/max_brightness",
        .val   = "/sys/class/leds/green/brightness",
      },
    },
  };

  static led_channel_redgreen_t channel[2];

  bool res = false;

  led_channel_redgreen_init(channel + 0);
  led_channel_redgreen_init(channel + 1);

  self->name   = "redgreen";
  self->data   = channel;
  self->enable = 0;
  self->value  = led_control_redgreen_value_cb;
  self->close  = led_control_redgreen_close_cb;

  /* We can use sw breathing logic to simulate hw blinking */
  self->can_breathe = true;
  self->breath_type = LED_RAMP_HARD_STEP;

  for( size_t i = 0; i < G_N_ELEMENTS(paths) ; ++i )
  {
    if( led_channel_redgreen_probe(&channel[0], &paths[i][0]) &&
        led_channel_redgreen_probe(&channel[1], &paths[i][1]) )
    {
      res = true;
      break;
    }
  }

  if( !res )
  {
    led_control_close(self);
  }

  return res;
}
/**
 * This file is part of budgie-monitor-brightness-applet
 *
 * Copyright © 2016 Ikey Doherty <ikey@solus-project.com>
 * Copyright © 2019 Dominik Schütz <do.sch.dev@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program;  if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _MonitorBrightnessPlugin MonitorBrightnessPlugin;
typedef struct _MonitorBrightnessPluginClass MonitorBrightnessPluginClass;

#define TYPE_MONITOR_BRIGHTNESS_PLUGIN monitor_brightness_plugin_get_type()
#define MONITOR_BRIGHTNESS_PLUGIN(o)                                                                   \
        (G_TYPE_CHECK_INSTANCE_CAST((o), TYPE_MONITOR_BRIGHTNESS_PLUGIN, MonitorBrightnessPlugin))
#define IS_MONITOR_BRIGHTNESS_PLUGIN(o) (G_TYPE_CHECK_INSTANCE_TYPE((o), TYPE_MONITOR_BRIGHTNESS_PLUGIN))
#define MONITOR_BRIGHTNESS_PLUGIN_CLASS(o)                                                             \
        (G_TYPE_CHECK_CLASS_CAST((o), TYPE_MONITOR_BRIGHTNESS_PLUGIN, MonitorBrightnessPluginClass))
#define IS_MONITOR_BRIGHTNESS_PLUGIN_CLASS(o) (G_TYPE_CHECK_CLASS_TYPE((o), TYPE_MONITOR_BRIGHTNESS_PLUGIN))
#define MONITOR_BRIGHTNESS_PLUGIN_GET_CLASS(o)                                                         \
        (G_TYPE_INSTANCE_GET_CLASS((o), TYPE_MONITOR_BRIGHTNESS_PLUGIN, MonitorBrightnessPluginClass))

struct _MonitorBrightnessPluginClass {
        GObjectClass parent_class;
};

struct _MonitorBrightnessPlugin {
        GObject parent;
};

GType monitor_brightness_plugin_get_type(void);

G_END_DECLS

/**
 * This file is part of budgie-monitor-brightness-applet
 *
 * Copyright (C) 2019 Dominik Schütz <do.sch.dev@gmail.com>
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

#include <budgie-desktop/applet.h>
#include <budgie-desktop/popover.h>
#include <budgie-desktop/popover-manager.h>
#include <gtk/gtk.h>

#define __budgie_unused__ __attribute__((unused))

G_BEGIN_DECLS

typedef struct _MonitorBrightnessApplet MonitorBrightnessApplet;
typedef struct _MonitorBrightnessAppletClass MonitorBrightnessAppletClass;

#define TYPE_MONITOR_BRIGHTNESS_APPLET monitor_brightness_applet_get_type()
#define MONITOR_BRIGHTNESS_APPLET(o)                                                                   \
        (G_TYPE_CHECK_INSTANCE_CAST((o), TYPE_MONITOR_BRIGHTNESS_APPLET, MonitorBrightnessApplet))
#define IS_MONITOR_BRIGHTNESS_APPLET(o) (G_TYPE_CHECK_INSTANCE_TYPE((o), TYPE_MONITOR_BRIGHTNESS_APPLET))
#define MONITOR_BRIGHTNESS_APPLET_CLASS(o)                                                             \
        (G_TYPE_CHECK_CLASS_CAST((o), TYPE_MONITOR_BRIGHTNESS_APPLET, MonitorBrightnessAppletClass))
#define IS_MONITOR_BRIGHTNESS_APPLET_CLASS(o) (G_TYPE_CHECK_CLASS_TYPE((o), TYPE_MONITOR_BRIGHTNESS_APPLET))
#define MONITOR_BRIGHTNESS_APPLET_GET_CLASS(o)                                                         \
        (G_TYPE_INSTANCE_GET_CLASS((o), TYPE_MONITOR_BRIGHTNESS_APPLET, MonitorBrightnessAppletClass))

struct _MonitorBrightnessAppletClass {
        BudgieAppletClass parent_class;
};

struct _MonitorBrightnessApplet {
        BudgieApplet parent;
};

GType monitor_brightness_applet_get_type(void);

/**
 * Public for the plugin to allow registration of types
 */
void monitor_brightness_applet_init_gtype(GTypeModule *module);

/**
 * Construct a new MonitorBrightnessApplet
 */
BudgieApplet *monitor_brightness_applet_new(void);

G_END_DECLS


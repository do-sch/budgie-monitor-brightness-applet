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

#define _GNU_SOURCE

#include <budgie-desktop/plugin.h>

#include "applet.h"
#include "plugin.h"

static void monitor_brightness_plugin_iface_init(BudgiePluginIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(MonitorBrightnessPlugin, monitor_brightness_plugin, G_TYPE_OBJECT, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(BUDGIE_TYPE_PLUGIN,
                                                             monitor_brightness_plugin_iface_init))

/**
 * Return a new panel widget
 */
static BudgieApplet *monitor_brightness_applet_get_panel_widget(__budgie_unused__ BudgiePlugin *self,
                                                    __budgie_unused__ gchar *uuid)
{
        BudgieApplet *result = monitor_brightness_applet_new();
        
        /* Don't know why this is needed, but without budgie_panel would raise warnings and criticals on Solus. On Arch this is not needed */
        g_object_ref_sink(result);
        return result;
}

/**
 * Handle cleanup
 */
static void monitor_brightness_plugin_dispose(GObject *object)
{
        G_OBJECT_CLASS(monitor_brightness_plugin_parent_class)->dispose(object);
}

/**
 * Class initialisation
 */
static void monitor_brightness_plugin_class_init(MonitorBrightnessPluginClass *klazz)
{
        GObjectClass *obj_class = G_OBJECT_CLASS(klazz);

        /* gobject vtable hookup */
        obj_class->dispose = monitor_brightness_plugin_dispose;
}

/**
 * Implement the BudgiePlugin interface, i.e the factory method get_panel_widget
 */
static void monitor_brightness_plugin_iface_init(BudgiePluginIface *iface)
{
        iface->get_panel_widget = monitor_brightness_applet_get_panel_widget;
}

/**
 * No-op, just skips compiler errors
 */
static void monitor_brightness_plugin_init(__budgie_unused__ MonitorBrightnessPlugin *self)
{
}

/**
 * Nothing to free
 */
static void monitor_brightness_plugin_class_finalize(__budgie_unused__ MonitorBrightnessPluginClass *klazz)
{
}

/**
 * Export the types to the gobject type system
 */
G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module)
{
        monitor_brightness_plugin_register_type(G_TYPE_MODULE(module));

        /* Register the actual dynamic types contained in the resulting plugin */
        monitor_brightness_applet_init_gtype(G_TYPE_MODULE(module));

        peas_object_module_register_extension_type(module,
                                                   BUDGIE_TYPE_PLUGIN,
                                                   TYPE_MONITOR_BRIGHTNESS_PLUGIN);
}


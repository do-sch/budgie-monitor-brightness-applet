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

#include "internaldisplayhandler.h"

#define PROPERTYNAME "Brightness"

static int wished_brightness = -1;
/* if this thread is false, all threads end, as soon as they wake up */
static char cont = 1;
static pthread_mutex_t lock;
static pthread_cond_t cond;
static GDBusProxy *proxy = NULL;

/**
 * signal, if brightness is changed
 */
static void proxy_signal (GDBusProxy *proxy,
		    GVariant   *changed_properties,
		    GStrv       invalidated_properties,
		    gpointer    user_data)
{
    g_print("proxy_signal\n");
    void (*callback)(int) = user_data;
    
    GVariant *v;
    GVariantDict dict;
    
    g_variant_dict_init (&dict, changed_properties);
    
    if (g_variant_dict_contains (&dict, PROPERTYNAME)) {
    
        int val;
        v = g_dbus_proxy_get_cached_property(proxy, PROPERTYNAME);
        
        if (v != NULL) {
            val = g_variant_get_int32(v);
            g_variant_unref(v);
            callback(val);
        }
        
    }
    
    g_variant_dict_clear(&dict);
}

/**
 * sets brightness of internal display
 */
static void set_brightness_thread(void *val)
{
    int last_brightness = wished_brightness;
    GError *error = NULL;
        
    while (cont) {
        
        /* fall asleep when wished brightness is already set */
        if (wished_brightness == last_brightness) {
            pthread_mutex_lock(&lock);
            pthread_cond_wait(&cond, &lock);
            pthread_mutex_unlock(&lock);
            
            if (!cont)
                break;
        }
        
        last_brightness = wished_brightness;
        /* sets birghtness */
        if (proxy != NULL) {
            g_dbus_proxy_call_sync(proxy,
                              "org.freedesktop.DBus.Properties.Set",
                              g_variant_new("(ssv)",
                                            "org.gnome.SettingsDaemon.Power.Screen",
                                            "Brightness",
                                            g_variant_new_int32(last_brightness)),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL,
                              &error);
            if (error != NULL) {
                g_print("Proxy call error: %s\n", error -> message);
                g_error_free(error);
            }
        } else {
            break;
        }
    }
    
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    
}

/**
 * function, that gets called when dbus is connected. It calls property brightness.
 * if brightness is -1, there is no internal display
 */
static void dbus_connected(GObject *source_object, GAsyncResult *res, gpointer user_data) 
{
    GError *error;
    int value = -1;
    void (*callback)(int) = user_data;
    
    /* persists proxy for other functions */
    proxy = g_dbus_proxy_new_for_bus_finish(res, &error);
    
    if (proxy == NULL) {
        g_printerr("Error getting proxy client: %s\n", error -> message);
        g_error_free(error);
    } else {
        GVariant *var;
        
        var = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(proxy), PROPERTYNAME);
        if (var != NULL) {
            value = g_variant_get_int32(var);
            g_variant_unref(var);
        }
    }
    
    char has_internal = value != -1;
    
    /* this initialize thread, mutex and condition for wakeup, if there is a internal display */
    if (has_internal) {
        pthread_mutex_init(&lock, NULL);
        pthread_cond_init(&cond, NULL);
        
        pthread_t id;
        pthread_create(&id, NULL, (void*) set_brightness_thread, NULL);
    } else {
        g_object_unref(proxy);
        proxy = NULL;
    }
    
    callback(has_internal);
    
}

/**
 * tells callback function, if there is an internal display
 */
void internal_init(void (*callback)(int)) 
{
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.gnome.SettingsDaemon.Power",
                                  "/org/gnome/SettingsDaemon/Power",
                                  "org.gnome.SettingsDaemon.Power.Screen",
                                  NULL,
                                  dbus_connected,
                                  callback);
}

/**
 * returns internal brightness to callback function
 */
int internal_get_brightness() 
{
    GVariant *var;
    int value = -1;
        
    var = g_dbus_proxy_get_cached_property(G_DBUS_PROXY(proxy), PROPERTYNAME);
    if (var != NULL) {
        value = g_variant_get_int32(var);
        g_variant_unref(var);
    }
    
    return value;
}


/**
 * sets internal brightness
 */
void internal_set_brightness(int percentage) 
{
    if (proxy != NULL) {
        wished_brightness = percentage;
        pthread_cond_signal(&cond);
    }  
}

/**
 * sets function to call, if brightness changes
 */
void internal_set_on_change_callback(void (*callback)(int)) 
{
    g_signal_connect(proxy, "g-properties-changed", G_CALLBACK(proxy_signal), (void*) callback);
}

/**
 * destroys the proxy
 */
void internal_destroy() 
{
    if (proxy != NULL) {
        g_object_unref(proxy);
        cont = 0;
        pthread_cond_signal(&cond);
    }
}

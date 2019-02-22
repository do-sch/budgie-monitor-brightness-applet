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

#include "applet.h"
#include "displaymanager.h"
#include <stdlib.h>

static char tooltip_text[5];
static int displaycount = 0;
static GtkWidget *ebox, *popover, *sliderbox;
static BudgiePopoverManager *managerref;

typedef struct Brightness_Store{
	gpointer range;
	int value;
} Brightness_Store;

//static GDBusProxy *mutter_proxy;

G_DEFINE_DYNAMIC_TYPE_EXTENDED(MonitorBrightnessApplet, monitor_brightness_applet, BUDGIE_TYPE_APPLET, 0, )

static gboolean refresh_range(gpointer user_data)
{
	Brightness_Store *store = user_data;
	
	GtkRange *range = store -> range;
	int value = store -> value;
	
	if (!gtk_widget_get_visible(popover))
    	gtk_range_set_value(GTK_RANGE(range), value);
	
	/* frees Brightness_Store */
	g_free(store);
	
	return G_SOURCE_REMOVE;
}

static void update_brightness(int brightness, void* range){
	
	/* store values for main-loop-thread */
	Brightness_Store *store = g_malloc(sizeof(Brightness_Store));
	store -> range = range;
	store -> value = brightness;
	
	/* run in main thread */
	gdk_threads_add_idle(refresh_range, store);
}

/**
 * changes brightness of single monitor
 */
static void change_brightness(GtkWidget *scale, GtkScrollType scroll, double value, void *v) 
{
	/* convert pointer to long and int to store without need of heap */
	intptr_t i = (intptr_t)v;
	
	/* set brightness of scale */
	int val = gtk_range_get_value(GTK_RANGE(scale));
	set_brightness_percentage(i, val);
}


/**
 * creates and adds GtkWidgets that depend on the thread
 */
static gboolean create_sliders(gpointer user_data)
{
	if (displaycount > 0) {
		/* add sliderbox for every display */	
		GtkWidget *sliderboxes[displaycount];

		for (int i = 0; i < displaycount; i++) {
		
			/* Add Separator between Sliders, if more than one monitor avaliable */
			if (i != 0) {
				GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
				gtk_box_pack_start(GTK_BOX(sliderbox), sep, FALSE, FALSE, 4);
			}
		
			/* create sliderbox */
			sliderboxes[i] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
			
			/* get name of display */
			char *dspname = get_display_name(i);
			GtkWidget *label = gtk_label_new(dspname);
			
			/* create scale */
			GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 1);
			gtk_range_set_inverted(GTK_RANGE(scale), TRUE);
			
			/* get value for range (this is handled in an thread to avoid lag) */
			get_brightness_percentage(i, scale, update_brightness);
			
			/* make scale look prettier */
			gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
			gtk_widget_set_size_request(scale, 25, 120);
			
		    /* dirty, but fast + prevents mistakes with memory management */
		    g_signal_connect(scale, "change-value", G_CALLBACK(change_brightness), (void*) ((intptr_t)i));
			
			/* add label and scale to sliderbox */
			gtk_box_pack_start(GTK_BOX(sliderboxes[i]), label, FALSE, FALSE, 5);
			gtk_box_pack_start(GTK_BOX(sliderboxes[i]), scale, FALSE, FALSE, 0);
			
			/* add sliderbox to outer sliderbox */
			gtk_box_pack_start(GTK_BOX(sliderbox), sliderboxes[i], TRUE, FALSE, 5);
			
			/* tell displaymanager scale, so value can be connected */
			register_scale(scale, i, update_brightness);

		}
	} else {
		GtkWidget *no_display_label = gtk_label_new("No supported monitors found");
		gtk_box_pack_start(GTK_BOX(sliderbox), no_display_label, FALSE, FALSE, 5);
	}
	
	/* Show all of our things. */
        gtk_widget_show_all(GTK_WIDGET(sliderbox));
        
        return G_SOURCE_REMOVE;
}

/** 
 * this function is called when ddca is initialized, it calls ui to create sliders
 */
static void update_displaycount(int count) 
{	
	/* get displaycount */
	displaycount = count;
	
	/* run in main thread */
	gdk_threads_add_idle(create_sliders, NULL);
}

/**
 * Create Budgie Popover
 */
static gboolean create_brightness_popover(gpointer userdata) 
{
	/* if userdata != NULL -> recreate, this can be used later, when ddcutil is able to reinitialize */
	if (userdata == NULL) {
	
		GtkWidget *mainbox, *sep1, *nightlightbox, *nightlightlabel, *nightlightswitch;
	
		/* create budgie popover if it does not exist */
		popover = budgie_popover_new(ebox);
		
		/* create box inside of popover */
		mainbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 6);
		
		/* create box, that contains all sliderboxes */
		sliderbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(mainbox), sliderbox, FALSE, FALSE, 0);
		
		/* add manbox to popover */
		gtk_container_add(GTK_CONTAINER(popover), mainbox);
		
		/* Separator before nightlight Toggle */
		sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(mainbox), sep1, FALSE, FALSE, 6);
		
		/* Night Light */
		nightlightbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		
		nightlightlabel = gtk_label_new("Night Light");
		gtk_widget_set_hexpand_set(nightlightlabel, TRUE);
		
		nightlightswitch = gtk_switch_new();
		GSettings *s = g_settings_new("org.gnome.settings-daemon.plugins.color");
		g_settings_bind(s, "night-light-enabled", nightlightswitch, "active", G_SETTINGS_BIND_DEFAULT);
		
		gtk_box_pack_start(GTK_BOX(nightlightbox), nightlightlabel, FALSE, FALSE, 6);
		gtk_box_pack_end(GTK_BOX(nightlightbox), nightlightswitch, FALSE, FALSE, 6);
		
		gtk_box_pack_start(GTK_BOX(mainbox), nightlightbox, FALSE, FALSE, 0);
	
		/* Show all of our things. */
		gtk_widget_show_all(GTK_WIDGET(mainbox));
	
	} else {
		/* remove all inner containers */
		/*GList *children, *iter;
		children = gtk_container_get_children(GTK_CONTAINER(sliderbox));
		//for(iter = children; iter != NULL; iter = g_list_next(iter))
		//	gtk_widget_destroy(GTK_WIDGET(iter->data));
		g_list_free_full(children, gtk_widget_destroy);*/
		
		gtk_container_foreach (GTK_CONTAINER (sliderbox), (GtkCallback) gtk_widget_destroy, NULL);
	}
		
	count_displays_and_init(update_displaycount);
	
	///* Display Settings */
	//GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	//GtkWidget *dispsettings = gtk_button_new_with_label("Display Settings");
	//gtk_button_set_relief(GTK_BUTTON(dispsettings), GTK_RELIEF_NONE);
	//gtk_widget_set_halign(dispsettings, GTK_ALIGN_START);
	//GtkStyleContext *context = gtk_widget_get_style_context(dispsettings);
	//gtk_style_context_add_class(context, "flat");
	
	//gtk_box_pack_start(GTK_BOX(mainbox), sep2, FALSE, FALSE, 6);
	//gtk_box_pack_start(GTK_BOX(mainbox), dispsettings, FALSE, FALSE, 0);
       
        
        return G_SOURCE_REMOVE;
}


// proxy works but ddca_c_api won't allow to reinitialize
/**
 * Signal from DBus-Proxy, when new Displays where connected or disconnected
 */
//static gboolean on_monitors_changed(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, 
//		GVariant *parameters, gpointer user_data)
//{
//
//	if (strcmp(signal_name, "MonitorsChanged") == 0 && displaycount != -1) {
//		/* free old stuff, it will be newly created */
//		displaycount = -1;
//        	free_ddca();
//	
//		/* run in main thread */
//		gdk_threads_add_idle(create_brightness_popover, (gpointer)1);
//	}
//	
//	return G_SOURCE_REMOVE;
//}

/**
 * Scroll events
 */
static void on_scroll_event(GtkWidget *image, GdkEventScroll *scroll)
{
	/* return if there is no monitor here */
	if (displaycount == 0)
		return;
		
	
	/* Collect ranges */
	GtkRange *ranges[displaycount];
	int countnum = 0;
	GList *sliderboxes = gtk_container_get_children(GTK_CONTAINER(sliderbox));
	for (GList *item = sliderboxes; item != NULL; item = item -> next) {
		/* skip separator */
		if (GTK_IS_CONTAINER(item -> data)) {
			GList *inner = gtk_container_get_children(GTK_CONTAINER(item -> data));
			for (GList *inneritem = inner; inneritem != NULL; inneritem = inneritem -> next) {
				/* skip label */
				if (GTK_IS_RANGE(inneritem -> data))
					ranges[countnum++] = GTK_RANGE(inneritem -> data);
			}
			
			g_list_free(inner);
		}
	}
	
	g_list_free(sliderboxes);
	
	int value = gtk_range_get_value(ranges[0]);
	
	/* raise or lower birghtness */
	if (scroll -> direction == GDK_SCROLL_UP) {
		value += 7;
	} else if (scroll -> direction == GDK_SCROLL_DOWN) {
		value -= 7;
	}
	
	/* limit value */
	if (value > 100)
		value = 100;
	else if (value < 0)
		value = 0;
	
	/* set new brightness for every scale */
	for (int i = 0; i < displaycount; i++) {
	
    	if (!is_self_updated(i)) {
    		gtk_range_set_value(ranges[i], value);	
		}
	}
	
	/* set value to all screens */
	set_brightness_percentage_for_all(value);	

	/* store value of first scrollbar in tooltip_text-array */
	sprintf(tooltip_text, "%d%%", value);
	gtk_widget_set_tooltip_text(image, tooltip_text);
	
}

/**
 * Button press event
 */
static void on_press_event(GtkWidget *image, GdkEventButton *buttonevent)
{
	
	/* Only allow primary mouse button */
	if (buttonevent -> button != 1) {
		return;
	}
	
	/* Hide if already showing */
	if (gtk_widget_get_visible(popover)) {
		gtk_widget_hide(popover);
	} else {
		/* It is invisible, so show it */
		budgie_popover_manager_show_popover(managerref, ebox);
	}
	
}

/**
 * connect to Proxy-Signal
 */
//static void on_proxy_created(GObject *source_object, GAsyncResult *res, gpointer user_data)
//{
//	GError *error;
//	
//	/* get proxy */
//	mutter_proxy = g_dbus_proxy_new_for_bus_finish(res, &error);
//	
//	if (mutter_proxy == NULL) {
//		/* print error if one occurs */ 
//		g_printerr("Error getting proxy client:%s\n", error -> message);
//		g_error_free(error);
//	} else {
//		/* connect to signal if no error occurs */
//		g_signal_connect(mutter_proxy, "g-signal", G_CALLBACK(on_monitors_changed), NULL);
//	}
//}

/**
 * Initialisation of basic UI layout and such
 */
static void monitor_brightness_applet_init(MonitorBrightnessApplet *self)
{
	/* Create EventBox with Brightness-Icon */
    GtkWidget *image = gtk_image_new_from_icon_name("display-brightness-symbolic", GTK_ICON_SIZE_MENU);
    ebox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(ebox), image);
            
    /* Connect EventBox to its signals */
	gtk_widget_set_events(ebox, GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK);
	g_signal_connect(ebox, "scroll_event", G_CALLBACK(on_scroll_event), NULL);
	g_signal_connect(ebox, "button_press_event", G_CALLBACK(on_press_event), NULL);
	
	/* Create Popover */
	create_brightness_popover(NULL);
        
	/* Add ebox to applet */
    gtk_container_add(GTK_CONTAINER(self), ebox);

    /* Show all of our things. */
    gtk_widget_show_all(GTK_WIDGET(self));
}

/**
 * overwritten update_popovers
 */
static void update_popovers(BudgieApplet *self, BudgiePopoverManager *manager)
{
	budgie_popover_manager_register_popover(manager, ebox, BUDGIE_POPOVER(popover));
	managerref = manager;
}

/**
 * Handle cleanup
 */
static void monitor_brightness_applet_dispose(GObject *object)
{
    G_OBJECT_CLASS(monitor_brightness_applet_parent_class)->dispose(object);
    //g_object_unref(mutter_proxy);
}


/**
 * Class initialisation
 */
static void monitor_brightness_applet_class_init(MonitorBrightnessAppletClass *klazz)
{
        
    GObjectClass *obj_class = G_OBJECT_CLASS(klazz);

    /* gobject vtable hookup */
    obj_class->dispose = monitor_brightness_applet_dispose;
    
    /* override update_popoveres */
    klazz -> parent_class.update_popovers = update_popovers;
    
    //monitor_brightness_applet_parent_class = g_type_class_peek_parent(klazz);
        
}


/**
 * clean up ddca
 */
static void monitor_brightness_applet_class_finalize(__budgie_unused__ MonitorBrightnessAppletClass *klazz)
{
    clear_all();
}
 
void monitor_brightness_applet_init_gtype(GTypeModule *module)
{
    monitor_brightness_applet_register_type(module);
}

BudgieApplet *monitor_brightness_applet_new(void)
{
    return g_object_new(TYPE_MONITOR_BRIGHTNESS_APPLET, NULL);
}


/**
 * This file is part of budgie-monitor-brightness-applet
 *
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

#include "ddcwrapper.h"
#include "internaldisplayhandler.h"


/**
 * initializes everything and gives back the number of compatible displays to callback function
 */
void count_displays_and_init(void (*callback)(int));

/**
 * returns the monitorname of selected display
 */
char *get_display_name(int dispnum);

/**
 * returns brightness of selected display to callback function
 */
void get_brightness_percentage(int dispnum, void *userdata, void (*callback)(int, void*));

/**
 * register a scale, so its value can be changed, if brightness gets changed from another place
 */
void register_scale(void *scale, int index, void (*callback)(int, void*));

/**
 * tells, if the scale is updated by dbus signal
 */
int is_self_updated(int dispnum);

/**
 * sets brightness of selected display
 */
void set_brightness_percentage(int dispnum, int value);

/**
 * set brightness for all displays
 */
void set_brightness_percentage_for_all(int value);

/**
 * everything
 */
void clear_all();

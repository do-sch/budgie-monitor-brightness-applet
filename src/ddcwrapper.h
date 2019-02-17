/**
 * This file is part of budgie-monitor-brightness-applet
 *
 * Copyright (C) 2019 Dominik Schütz <do.sch.dev@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DDCWRAPPER__
#define DDCWRAPPER

#include <ddcutil_c_api.h>
#include <pthread.h>
#include <stdlib.h>

/**
 * initializes ddcci stuff and gives back the number of compatible displays
 */
void count_displays_and_init(void (*callback)(int));

/**
 * returns the monitorname of selected display
 */
char *get_display_name(int dispnum);

/**
 * returns brightness of selected display
 */
void get_brightness_percentage(int dispnum, void* userdata, void (*callback)(int, void*));

/**
 * sets brightness of selected display
 */
void set_brightness_percentage(int dispnum, int value);

/**
 * set brightness for all displays
 */
void set_brightness_percentage_for_all(int value);

/**
 * cleans the heap up
 */
void free_ddca();

#endif

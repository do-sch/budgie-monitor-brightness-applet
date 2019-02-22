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

#include <gio/gio.h>
#include <pthread.h>
#include <stdlib.h>

/**
 * tells callback function, if there is an internal display
 */
void internal_init(void (*callback)(int));

/**
 * returns internal brightness to callback function
 */
int internal_get_brightness();

/**
 * sets internal brightness
 */
void internal_set_brightness(int percentage);

/**
 * sets function to call, if brightness changes
 */
void internal_register_scale(gpointer userdata, void (*callback)(int, void*));

/**
 * destroys the proxy
 */
void internal_destroy();

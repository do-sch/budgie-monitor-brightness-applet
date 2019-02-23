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

/**
 * initializes ddcci stuff and gives back the number of compatible displays to callback function
 */
int ddc_count_displays_and_init();

/**
 * returns the monitorname of selected display
 */
char *ddc_get_display_name(int dispnum);

/**
 * returns brightness of selected display to callback function
 */
int ddc_get_brightness_percentage(int dispnum);

/**
 * sets brightness of selected display
 */
void ddc_set_brightness_percentage(int dispnum, int value);

/**
 * set brightness for all displays
 */
void ddc_set_brightness_percentage_for_all(int value);

/**
 * cleans the heap up
 */
void ddc_free();


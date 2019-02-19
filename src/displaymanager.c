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
 
#include "displaymanager.h"

typedef struct Brightness_Userdata {
    int dispnum;
    void *old_userdata;
    void (*callback)(int, void*);
} Brightness_Userdata;


/**
 * async part of initializing
 */
static void count_displays_and_init_thread(void (*callback)(int)){
    int displaycount = 0;
    
    displaycount += ddc_count_displays_and_init();
    
    callback(displaycount);
}
 
/**
 * initializes everything and gives back the number of compatible displays to callback function
 */
void count_displays_and_init(void (*callback)(int)){
    pthread_t id;
    int status;
    
    status = pthread_create(&id, NULL, (void*) count_displays_and_init_thread, callback);
    if (status != 0) {
        fprintf(stderr, "Error creating thread: %d\n", status);
    }
}

/**
 * returns the monitorname of selected display
 */
char *get_display_name(int dispnum){
    return ddc_get_display_name(dispnum);
}

/**
 * async part of getting brightness
 */
static void get_brightness_percentage_thread(void *userdata){
    Brightness_Userdata *data = userdata;
    
    int dispnum = data -> dispnum;
    void (*callback)(int, void*) = data -> callback;
    void *old_userdata = data -> old_userdata;
    
    free(data);
    
    int percentage;
    
    percentage = ddc_get_brightness_percentage(dispnum);
    
    callback(percentage, old_userdata);
    
}

/**
 * returns brightness of selected display to callback function
 */
void get_brightness_percentage(int dispnum, void* userdata, void (*callback)(int, void*)){
    Brightness_Userdata *data = malloc(sizeof(Brightness_Userdata));
    int status;
    pthread_t id;
    
    data -> dispnum = dispnum;
    data -> old_userdata = userdata;
    data -> callback = callback;
    
    status = pthread_create(&id, NULL, (void*) get_brightness_percentage_thread, data);
    if (status != 0) {
        fprintf(stderr, "Error creating thread: %d\n", status);
    }
}

/**
 * sets brightness of selected display
 */
void set_brightness_percentage(int dispnum, int value){
    ddc_set_brightness_percentage(dispnum, value);
}

/**
 * set brightness for all displays
 */
void set_brightness_percentage_for_all(int value){
    ddc_set_brightness_percentage_for_all(value);
}

/**
 * everything
 */
void clear_all(){
    free_ddca();
}

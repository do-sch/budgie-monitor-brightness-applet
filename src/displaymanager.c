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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "displaymanager.h"

typedef struct Brightness_Userdata {
    int dispnum;
    void *old_userdata;
    void (*callback)(int, void*);
} Brightness_Userdata;

static int has_internal = -1;
static pthread_mutex_t internal_ready_mutex;
static pthread_cond_t internal_ready_cond;

/**
 * sets has_internal variable and wakes up thread, that tells ui thread displycount
 */
static void has_internal_callback(int has) 
{
    pthread_mutex_lock(&internal_ready_mutex);
    has_internal = (has == 1 ? 1 : 0);
    pthread_cond_signal(&internal_ready_cond);
    pthread_mutex_unlock(&internal_ready_mutex);        
}

/**
 * async part of initializing
 */
static void count_displays_and_init_thread(void (*callback)(int))
{
    /* initialize stuff */
    int displaycount = 0;
    if (pthread_mutex_init(&internal_ready_mutex, NULL) != 0) {
        fprintf(stderr, "Error initializing mutex\n");
    }
    if (pthread_cond_init(&internal_ready_cond, NULL) != 0) {
        fprintf(stderr, "Error initializing conditional wait\n");
    }
       
    internal_init(has_internal_callback);
    displaycount += ddc_count_displays_and_init();
    
    /* waits for proxy callback to figure out, if there is an internal display */
    pthread_mutex_lock(&internal_ready_mutex);
    while(has_internal == -1) {
        pthread_cond_wait(&internal_ready_cond, &internal_ready_mutex);
    }
    pthread_mutex_unlock(&internal_ready_mutex);
    
    if (has_internal == -1) {
        fprintf(stderr, "Error with synchronization\n");
    }
    
    displaycount += has_internal;
    
    callback(displaycount);
}
 
/**
 * initializes everything and gives back the number of compatible displays to callback function
 */
void count_displays_and_init(void (*callback)(int))
{
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
char *get_display_name(int dispnum)
{
    if (has_internal == 1) {
        /* return "Internal" if there is an internal display */
        if (dispnum == 0)
            return "Internal";
        /* the ddcwrapper indizies have to be reduced by one */
        return ddc_get_display_name(dispnum - 1);
    }
    
    /* this is the default case, if there is no internal display */
    return ddc_get_display_name(dispnum);
}

/**
 * async part of getting brightness
 */
static void get_brightness_percentage_thread(void *userdata)
{
    /* unpack userdata for getting percentage */
    Brightness_Userdata *data = userdata;
    
    int dispnum = data -> dispnum;
    void (*callback)(int, void*) = data -> callback;
    void *old_userdata = data -> old_userdata;
    
    free(data);
    
    /* call ddc function and call its return value back to callback */
    int percentage;
    
    percentage = ddc_get_brightness_percentage(dispnum);
    
    callback(percentage, old_userdata);
    
}

/**
 * returns brightness of selected display to callback function
 */
void get_brightness_percentage(int dispnum, void* userdata, void (*callback)(int, void*))
{
    /* change internal brightness and sub dispnum to fit ddc dispnum */
    if (has_internal) {
        if (dispnum == 0) {
            callback(internal_get_brightness(), userdata);
            return;
        } else {
            dispnum -= 1;
        }
    }

    /* user data for brightness threads */
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
 * register a scale, so its value can be changed, if brightness gets changed from another place
 * returns 1 if scale can be registered
 */
void register_scale(void *scale, int index, void (*callback)(int, void*))
{
    if (has_internal == 1) {
        if (index == 0) {
            internal_register_scale(scale, callback);
        } else
            index--;
    }
    
    /* TODO: also detect brightness change at ddc interface */
}

/**
 * tells, if the scale is updated by dbus signal
 */
int is_self_updated(int dispnum) 
{
    if (has_internal && dispnum == 0)
        return 1;
    return 0;
}

/**
 * sets brightness of selected display
 */
void set_brightness_percentage(int dispnum, int value)
{
    if (has_internal == 1) {
        if (dispnum == 0) {
            internal_set_brightness(value);
        } else {
            dispnum--;
        }
    }
    ddc_set_brightness_percentage(dispnum, value);
   
}

/**
 * set brightness for all displays
 */
void set_brightness_percentage_for_all(int value)
{
    if (has_internal == 1)
        internal_set_brightness(value);
    ddc_set_brightness_percentage_for_all(value);
}

/**
 * everything
 */
void clear_all()
{
    if (has_internal == 1)
        internal_destroy();
    ddc_free();
}

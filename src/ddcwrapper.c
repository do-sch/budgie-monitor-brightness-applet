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

#include <ddcutil_c_api.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "ddcwrapper.h"

#define BRIGHTNESS_VCP_CODE 0x10

//#include <stdio.h>
//static FILE *debug;


/* information and references to a monitor */
typedef struct Display_Info {
	int dispno;
	DDCA_Display_Ref *ref;
	char *name;
	int wanted_brightness;
} Display_Info;

/* parameters for a brightness-change-thread */
typedef struct Brightness_Thread {
	int dispnum;
	pthread_t id;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	bool cont; /* thread will end itself, when this is set to false */
} Brightness_Thread;

/* parameters for a ask-for-brightness-thread */
typedef struct Brightness_Store{
	int dispnum;
	void *userdata;
	void (*callback)(int, void*);
} Brightness_Store;

/* array of all displays, supporting brightness change */
static Display_Info **info = NULL;

/* array with all threads */
static Brightness_Thread **brightness_change_threads = NULL;

/* mutex for Display_Info-array prevents race condition and memory leak when building up the whole array  */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/* adds thread safety for creating and destroying the whole stuff */
static pthread_mutex_t freemutex = PTHREAD_MUTEX_INITIALIZER;

static DDCA_Display_Info_List *zlist = NULL;

/* number of displays supporting brightness change */
static int displaycount = -1;

/* compare-function for quicksort */
static int cmp(const void* a, const void* b) 
{
	return ((*(Display_Info**) a) -> dispno) - ((*(Display_Info**) b) -> dispno);
}

static void error(DDCA_Status code) 
{
    fprintf(stderr, "%s: %s\n",
        ddca_rc_name(code),
        ddca_rc_desc(code)
    );
}

/**
 * adds a display to info array thread save and sorts it
 */
static void add_display(Display_Info *new) 
{
	/* ensure, that only one thread enters this area */
	pthread_mutex_lock(&lock);
	
	int index = displaycount;
	displaycount++;
	
	/* resize array by one */
	info = (Display_Info**) realloc(info, sizeof(Display_Info*) * sizeof(displaycount));
	
	info[index] = new;
	
	/* sort displays by displaycount */
	qsort(info, displaycount, sizeof(Display_Info*), cmp);
	
	pthread_mutex_unlock(&lock);
}

/**
 * takes a look at every display and adds it to info array if it is able to change brightness
 */
static void init_threaded(void *voidref) 
{
	DDCA_Status rc = 0;
	
	/* convert parameters for thread */
	Display_Info *parms = voidref;

	/* open display */
	DDCA_Display_Handle handle;
	rc = ddca_open_display2(*parms -> ref, true, &handle);
	if (rc != 0) {
	    free(parms);
	    error(rc);
	    return;
	}
	
	/* read current brightness value */
	DDCA_Non_Table_Vcp_Value val;
	rc = ddca_get_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, &val);
	if (rc == 0) {
	    /* permanently add display to infolist */
	    add_display(parms);
	    parms -> wanted_brightness = val.sl;
	} else {
	    /* forget thata display, if requesting brightness fails */
	    free(parms);
	    error(rc);
	}
	
	/* close display */
	rc = ddca_close_display(handle);
	if (rc != 0) {
	    error(rc);
	}
	   
}


/**
 * thread to set Brightness for one Monitor
 */
static void set_brightness_thread(void* val)
{
    
    DDCA_Status rc = 0;

	Brightness_Thread *myinfo = val;
	Display_Info *dinfo = info[myinfo -> dispnum];

	int last_brightness = dinfo -> wanted_brightness;
	pthread_mutex_t *lock = &myinfo -> lock;
	pthread_cond_t *cond = &myinfo -> cond;
	bool *cont = &myinfo -> cont;
	
	DDCA_Display_Handle handle = NULL;
	
	/* set brightness in a loop */
	while(*cont && rc == 0) {
	
		/* fall asleep when whished brightness is already set */
		if (dinfo -> wanted_brightness == last_brightness) {
			/* close display before sleeping */
			if (handle != NULL) {
				rc = ddca_close_display(handle);
				if (rc != 0) {
				    error(rc);
				}
			}
				
			/* sleep until another thread wakes you up */
			pthread_mutex_lock(lock);
			pthread_cond_wait(cond, lock);
			pthread_mutex_unlock(lock);

			if (!*cont)
				break;

			/* open display again, when need to change brightness */
			rc = ddca_open_display2(*(dinfo -> ref), true, &handle);
			if (rc!= 0) {
			    error(rc);
			}
			
		}
		
		/* set brightness Value */
		last_brightness = dinfo -> wanted_brightness;
		rc = ddca_set_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, 0, last_brightness);
		if (rc != 0) {
		    error(rc);
		    rc = ddca_close_display(handle);
		    if (rc != 0)
		        error (rc);
		}
		
	}
}

/**
 * prints error message
 */
static int error_initialization(char *message, int status)
{
    pthread_mutex_unlock(&freemutex);
    ddc_free();
    fprintf(stderr, message, status);
    return 0;
}

/**
 * initializes ddcci stuff and gives back the number of compatible displays
 */
int ddc_count_displays_and_init() 
{
	/* you can not free and create stuff at the same time */
	pthread_mutex_lock(&freemutex);
	
	if (displaycount != -1) {
		/* if already initialized return the displaycount you know */
		pthread_mutex_unlock(&freemutex);
		return displaycount;
	
	} else {
		
		/* initialize */
		int status;
		displaycount = 0;

		/* free old stuff, if any */
		//ddc_free();
		if (info != NULL) {
		    return error_initialization("info is NULL", 0);
		}
		
		
		//debug = fopen("/tmp/applet_debug.txt", "a+");
		//ddca_get_display_info_list2(true, &zlist);
		//fprintf(debug, "count(false): %d\n", zlist -> ct);
		//for (int i = 0; i < zlist -> ct; i++) {
		//    DDCA_Display_Info *info = &zlist -> info[i];
		//    
		//    fprintf(debug, "name %d: %s\n", i, info -> model_name);
		//}
		//ddca_free_display_info_list(zlist);
		//fprintf(debug, "\n");
		
		/* higher up ddc retries to ensure every monitor will be found */
		if ((status = ddca_set_max_tries(DDCA_MULTI_PART_TRIES, 15)) < 0) {
			return error_initialization("Error setting retries: %d\n", status);
		}

		/* count number of supported displays */
		if ((status = ddca_get_display_info_list2(true, &zlist)) < 0) {
			return error_initialization("Error asking for displaylist: %d\n", status);
		}
				
		
		int count = zlist -> ct;
		
		//fprintf(debug, "count: %d\n", count);
		
		/* Start threads. This makes the whole thing faster when using multiple monitors */
		pthread_t threads[count];	
		for (int i = 0; i < count; i++) {
			
			/* Store model name */
			DDCA_Display_Info *info = &(zlist -> info[i]);
		
			/* Parameters for Thread */
			Display_Info *dinfo = malloc(sizeof(Display_Info));
			dinfo -> dispno = info -> dispno;
			dinfo -> name = info -> model_name;
			dinfo -> ref = &(info -> dref);
			
			/* init monitors in separate threads to speed the whole thing up */
			if ((status = pthread_create(&threads[i], NULL, (void*)init_threaded, dinfo)) != 0) {
				return error_initialization("Error creating thread: %d\n", status);
			}
			
		}
		
		/* wait for all threads */
		for (int i = 0; i < count; i++) {
			if ((status = pthread_join(threads[i], NULL)) != 0) {		
				return error_initialization("Error joining threads: %d\n", status);
			}
		}
		
		/* create threads, that will change brightness later */
		brightness_change_threads = calloc(displaycount, sizeof(Brightness_Thread*));
		for (int i = 0; i < displaycount; i++) {
			brightness_change_threads[i] = malloc(sizeof(Brightness_Thread));
			brightness_change_threads[i] -> dispnum = i;
			if ((pthread_mutex_init(&(brightness_change_threads[i] -> lock), NULL) || 
				pthread_cond_init(&(brightness_change_threads[i] -> cond), NULL)) != 0) {		
				return error_initialization("Error creating synchronisation puffers: \n", 0);
			}
			brightness_change_threads[i] -> cont = true;
			if ((status = pthread_create(&(brightness_change_threads[i] -> id), NULL, (void*)set_brightness_thread, brightness_change_threads[i])) != 0) {
				return error_initialization("Error creating thread: %d\n", status);	
			}
		}

    	pthread_mutex_unlock(&freemutex);
		return displaycount;
		
	}
}


/**
 * returns the monitorname of selected display
 */
char *ddc_get_display_name(int dispnum)
{
	return info[dispnum] -> name;
}

/**
 * returns brightness of selected display
 */
int ddc_get_brightness_percentage(int dispnum)
{

    DDCA_Status rc;

	/* Open Display */
	DDCA_Display_Handle handle;
	rc = ddca_open_display2(*info[dispnum] -> ref, true, &handle);
	if (rc!= 0) {
	    error(rc);
	} else {
	
	    /* read out Value */
	    DDCA_Non_Table_Vcp_Value val;
	    rc = ddca_get_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, &val);
	    if (rc != 0) {
	        error(rc);
	        val.sl = 0;
	    }
	    
	    /* Close Display */
	    rc = ddca_close_display(handle);
	    if (rc!= 0) {
	        error(rc);
	    }
	    
	    return val.sl;
	}
	
	return -1;
	
}

/**
 * sets brightness of selected display
 */
void ddc_set_brightness_percentage(int dispnum, int value)
{
	/* everything has to be initialized first */
	if (dispnum >= displaycount)
		return;
	
	info[dispnum] -> wanted_brightness = value;
	
	/* wake up the thread, that handles brightness for this monitor */
	pthread_cond_signal(&brightness_change_threads[dispnum] -> cond);

}


/**
 * set brightness for all displays
 */
void ddc_set_brightness_percentage_for_all(int value)
{
	/* be save, everything is initialized */
	if(displaycount < 1)
		return;
		
	for (int i = 0; i < displaycount; i++) {
		info[i] -> wanted_brightness = value;
		
		/* wake up thread, that handles brightness for this monitor */
		pthread_cond_signal(&brightness_change_threads[i] -> cond);
	}
}

/**
 * cleans the heap up
 */
void ddc_free()
{
	pthread_mutex_lock(&freemutex);
	
	/* end all threads */
	for (int i = 0; i < displaycount; i++) {
		brightness_change_threads[i] -> cont = false;
		pthread_cond_signal(&brightness_change_threads[i] -> cond);
		pthread_join(brightness_change_threads[i] -> id, NULL);

		/* destroy mutex and conditional */	
	    pthread_mutex_destroy(&brightness_change_threads[i] -> lock);
    	pthread_cond_destroy(&brightness_change_threads[i] -> cond);
    	free(brightness_change_threads[i]);
    	
	}
	
	/* free thread-array, the single structures are freed in the thread itself */
	free(brightness_change_threads);
	brightness_change_threads = NULL;
	
	/* free all display-infos */
	for (int i = 0; i < displaycount; i++) {
		free(info[i]);
	}
	
	/* free display-info-array */
	free(info);
	info = NULL;
	
	/* free ddc_display_info_list */
	ddca_free_display_info_list(zlist);
	displaycount = -1;
	
	pthread_mutex_unlock(&freemutex);
}

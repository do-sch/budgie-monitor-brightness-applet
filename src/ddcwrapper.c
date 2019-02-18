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

#include "ddcwrapper.h"

#define BRIGHTNESS_VCP_CODE 0x10

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

/* number of displays supporting brightness change */
static int displaycount = -1;

/* compare-function for quicksort */
static int cmp(const void* a, const void* b) {
	return ((*(Display_Info**) a) -> dispno) - ((*(Display_Info**) b) -> dispno);
}

static void error(DDCA_Status code) {
    fprintf(stderr, "%s: %s\n",
        ddca_rc_name(code),
        ddca_rc_desc(code)
    );
}

/**
 * adds a display to info array thread save and sorts it
 */
static void add_display(Display_Info *new) {
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

static DDCA_Display_Info_List *zlist = NULL;

/**
 * takes a look at every display and adds it to info array if it is able to change brightness
 */
static void init_threaded(void *voidref) {
	DDCA_Status rc = 0;
	
	/* convert parameters for thread */
	Display_Info *parms = voidref;

	/* open display */
	DDCA_Display_Handle handle;
	rc = ddca_open_display2(*(parms -> ref), true, &handle);
	if (rc != 0) {
	    free(parms);
	    error(rc);
	    return;
	}
	
	/* read capabilities */
	char *capabilities_string;
	rc = ddca_get_capabilities_string(handle, &capabilities_string);
	if (rc != 0) {
	    free(parms);
	    error(rc);
	    rc = ddca_close_display(handle);
	    if (rc != 0) {
	        error(rc);
	    }
	    return;
	}
	
	/* parse capabilities */
	DDCA_Capabilities* capabilities;
	rc = ddca_parse_capabilities_string(capabilities_string, &capabilities);
	if (rc != 0) {
	    free(parms);
	    error(rc);
	    return;
	}
	
	/* check, if Monitor is able to change Brightness */
	DDCA_Feature_List list = ddca_feature_list_from_capabilities(capabilities);
	ddca_free_parsed_capabilities(capabilities);
	if (ddca_feature_list_contains(&list, BRIGHTNESS_VCP_CODE)) {
	
		/* read current brightness value and permanently store Display_Info */
		DDCA_Non_Table_Vcp_Value val;
		
		rc = ddca_get_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, &val);
		if (rc != 0) {
		    free(parms);
		    error(rc);
		    rc = ddca_close_display(handle);
		    if (rc != 0) {
		        error(rc);
		        free(parms);
		    }
		    return;
		}
		
		parms -> wanted_brightness = val.sl;
		
		add_display(parms);
	} else {
		/* delete parms, if it is not able, to change Brightness */
		free(parms);
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
static void set_brightness_thread(void* val){
    
    DDCA_Status rc = 0;

	Brightness_Thread *myinfo = val;
	Display_Info *dinfo = info[myinfo -> dispnum];

	int last_brightness = dinfo -> wanted_brightness;
	pthread_mutex_t *lock = &myinfo -> lock;
	pthread_cond_t *cond = &myinfo -> cond;
	bool *cont = &myinfo -> cont;
	
	DDCA_Display_Handle handle = NULL;
	
	/* Set brightness in a loop */
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
			    break;
			}
			
		}
		
		/* Set Brightness Value */
		last_brightness = dinfo -> wanted_brightness;
		rc = ddca_set_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, 0, last_brightness);
		if (rc != 0) {
		    error(rc);
		    rc = ddca_close_display(handle);
		    if (rc != 0)
		        error (rc);
		}
		
	}
	
	/* destroy mutex and conditional */	
	pthread_mutex_destroy(lock);
	pthread_cond_destroy(cond);
	
	/* free thread struct */
	free(myinfo);
}

/**
 * initializes ddcci stuff in its own thread and gives back number of displays to callback function
 */
static void count_displays_and_init_thread(void (*callback)(int)) 
{
	/* you can not free and create stuff at the same time */
	pthread_mutex_lock(&freemutex);
	
	if (displaycount != -1) {
		/* if already initialized return the displaycount you know */
		callback(displaycount);
	
	} else {
		
		/* initialize */
		int status;
		displaycount = 0;

		/* free old stuff, if any */
		//free_ddca();
		if (info != NULL) {
			fprintf(stderr, "WTF\n");
			return;
		}

		/* count number of supported displays */
		if ((status = ddca_get_display_info_list2(false, &zlist)) < 0) {
			fprintf(stderr, "Error asking for displaylist: %d\n", status);
			callback(0);
			return;
		}
		
		int count = zlist -> ct;
		
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
				fprintf(stderr, "Error creating thread: %d\n", status);
				free_ddca();
				callback(0);
			}
			
		}
		
		/* wait for all threads */
		for (int i = 0; i < count; i++) {
			if ((status = pthread_join(threads[i], NULL)) != 0) {		
				fprintf(stderr, "Error joining threads: %d\n", status);
				free_ddca();
				callback(0);
			}
		}
		
		/* create threads, that will change brightness later */
		brightness_change_threads = calloc(displaycount, sizeof(Brightness_Thread*));
		for (int i = 0; i < displaycount; i++) {
			brightness_change_threads[i] = malloc(sizeof(Brightness_Thread));
			brightness_change_threads[i] -> dispnum = i;
			if ((pthread_mutex_init(&(brightness_change_threads[i] -> lock), NULL) || 
				pthread_cond_init(&(brightness_change_threads[i] -> cond), NULL)) != 0) {		
				fprintf(stderr, "Error creating synchronisation puffers: \n");
				free_ddca();
				callback(0);
			}
			brightness_change_threads[i] -> cont = true;
			if ((status = pthread_create(&(brightness_change_threads[i] -> id), NULL, (void*)set_brightness_thread, brightness_change_threads[i])) != 0) {
				fprintf(stderr, "Error creating thread: %d\n", status);	
				free_ddca();
				callback(0);
			}
		}

		callback(displaycount);
		
	}
	pthread_mutex_unlock(&freemutex);
}

/**
 * initializes ddcci stuff and gives back the number of compatible displays
 */
void count_displays_and_init(void (*callback)(int)) {
	pthread_t id;
	pthread_create(&id, NULL, (void*) count_displays_and_init_thread, callback);	
}

/**
 * returns the monitorname of selected display
 */
char *get_display_name(int dispnum){
	return info[dispnum] -> name;
}

/**
 * async part of telling brightness
 */
void get_brightness_percentage_thread(void *storevoid) {

    DDCA_Status rc;

	Brightness_Store *store = storevoid;

	/* Open Display */
	DDCA_Display_Handle handle;
	rc = ddca_open_display2(*(info[store -> dispnum] -> ref), true, &handle);
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
	    
	    /* calls function in applet.c */
	    store -> callback(val.sl, store -> userdata);
	}
	
	/* frees store */
	free(store);
}

/**
 * returns brightness of selected display
 */
void get_brightness_percentage(int dispnum, void *userdata, void (*callback)(int, void*)){

	/* create store that holds parameters for thread */
	Brightness_Store *store = malloc(sizeof(Brightness_Store));
	store -> dispnum = dispnum;
	store -> userdata = userdata;
	store -> callback = callback;
	
	/* create thread */
	pthread_t id;
	pthread_create(&id, NULL, (void *) get_brightness_percentage_thread, store);
	
}

/**
 * sets brightness of selected display
 */
void set_brightness_percentage(int dispnum, int value){
	/* everything has to be initialized first */
	if (dispnum >= displaycount)
		return;
	
	info[dispnum] -> wanted_brightness = value;
	
	/* wake up the thread, that handles brightness for this monitor */
	pthread_cond_signal(&(brightness_change_threads[dispnum] -> cond));

}


/**
 * set brightness for all displays
 */
void set_brightness_percentage_for_all(int value){
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
void free_ddca(){
	pthread_mutex_lock(&freemutex);
	/* end all threads */
	for (int i = 0; i < displaycount; i++) {
		brightness_change_threads[i] -> cont = false;
		pthread_cond_signal(&brightness_change_threads[i] -> cond);
		pthread_join(brightness_change_threads[i] -> id, NULL);
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
	displaycount = 0;
	pthread_mutex_unlock(&freemutex);
}

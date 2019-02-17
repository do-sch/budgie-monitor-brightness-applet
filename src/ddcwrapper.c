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

static void add_display(Display_Info *new) {
	/* ensure, that only one thread enters this area*/
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

static void init_threaded(void *voidref) {
	
	/* convert parameters for thread) */
	Display_Info *parms = voidref;

	/* open display */
	DDCA_Display_Handle handle;
	ddca_open_display2(*(parms -> ref), true, &handle);
	
	/* read capabilities */
	char *capabilities_string;
	ddca_get_capabilities_string(handle, &capabilities_string);
	
	/* parse capabilities */
	DDCA_Capabilities* capabilities;
	ddca_parse_capabilities_string(capabilities_string, &capabilities);
	
	/* check, if Monitor is able to change Brightness exists */
	DDCA_Feature_List list = ddca_feature_list_from_capabilities(capabilities);
	if (ddca_feature_list_contains(&list, BRIGHTNESS_VCP_CODE)) {
		/* read current Brightness-Value and permanently store Display_Info */
		DDCA_Non_Table_Vcp_Value val;
		ddca_get_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, &val);
		
		parms -> wanted_brightness = val.sl;
		
		add_display(parms);
	} else {
		/* delete parms, if it is not able, to change Brightness */
		free(parms);
	}
	
	/* close display */
	ddca_close_display(handle);
}



/**
 * thread to set Brightness for one Monitor
 */
static void set_brightness_thread(void* val){
	Brightness_Thread *myinfo = val;
	Display_Info *dinfo = info[myinfo -> dispnum];

	int last_brightness = dinfo -> wanted_brightness;
	pthread_mutex_t *lock = &myinfo -> lock;
	pthread_cond_t *cond = &myinfo -> cond;
	bool *cont = &myinfo -> cont;
	
	DDCA_Display_Handle handle = NULL;
	
	/* Set brightness in a loop */
	while(*cont) {
	
		/* fall asleep when whished brightness is already set */
		if (dinfo -> wanted_brightness == last_brightness) {
			/* close display before sleeping */
			if (handle != NULL)
				ddca_close_display(handle);
				
			/* sleep until another thread wakes you up */
			pthread_mutex_lock(lock);
			pthread_cond_wait(cond, lock);
			pthread_mutex_unlock(lock);

			if (!*cont)
				break;

			/* open display again, when need to change brightness */
			ddca_open_display2(*(dinfo -> ref), true, &handle);
			
		}
		
		/* Set Brightness Value */
		last_brightness = dinfo -> wanted_brightness;
		ddca_set_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, 0, last_brightness);
		
	}
	
	/* destroy mutex and conditional */	
	pthread_mutex_destroy(lock);
	pthread_cond_destroy(cond);
	
	/* free thread struct */
	free(myinfo);
}

/**
 * 
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
		if (info != NULL)
			fprintf(stderr, "WTF\n");

		/* count number of supported displays */
		if ((status = ddca_get_display_info_list2(false, &zlist)) < 0) {
			fprintf(stderr, "Error asking for displaylist: %d\n", status);
			callback(0);
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

	Brightness_Store *store = storevoid;

	/* Open Display */
	DDCA_Display_Handle handle;
	ddca_open_display2(*(info[store -> dispnum] -> ref), true, &handle);
	
	/* read out Value */
	DDCA_Non_Table_Vcp_Value val;
	ddca_get_non_table_vcp_value(handle, BRIGHTNESS_VCP_CODE, &val);
	
	/* Close Display */
	ddca_close_display(handle);
	
	/* calls function in applet.c */
	store -> callback(val.sl, store -> userdata);
	
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

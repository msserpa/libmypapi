#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <papi.h>
#include <time.h>

#define PAPI_MAX_EVENTS 10
#define BUFFER_SIZE 1024

/* Use a positive value of retval to simply print an error message */
void error_handler(int line, const char *call, int retval){
	if(retval >= 0 || retval == PAPI_ESYS)
		fprintf(stderr, "libmypapi - Error: %s on line %d\n", call, line);
	else
		fprintf(stderr, "libmypapi - Error: %s : %s on line %d\n\n", call, PAPI_strerror(retval), line);

	_exit(1);
}

static void __attribute__ ((constructor)) constructor();

static void constructor(){
	char short_exec[BUFFER_SIZE], exec[BUFFER_SIZE], file[BUFFER_SIZE], log_dir[BUFFER_SIZE];
	char **list_events = NULL;
	int i, retval, length, sample_delay_ms, EventSet1 = PAPI_NULL, num_events = 0;
	long long int *values;
	pid_t pid = 0;
	FILE *f;

	/* Get full path */
	length = readlink("/proc/self/exe", exec, sizeof(exec) - 1);
	if(length == -1)
        error_handler(__LINE__, "readlink", 1);

    /* Get only application name */
    strcpy(short_exec,  strrchr(exec, '/') + 1);

    /* Only programs whose name ends with ".x" are accepted */
	if(exec[length - 2] == '.' && exec[length - 1] == 'x'){
		/* Get sample delay from environment variable */
		if(getenv("PAPI_DELAY") == NULL)
			sample_delay_ms = 250;
		else
			sample_delay_ms = atoi(getenv("PAPI_DELAY"));

		/* Init the PAPI library */
		retval = PAPI_library_init(PAPI_VER_CURRENT);
		if(retval != PAPI_VER_CURRENT){
			error_handler(__LINE__, "PAPI_library_init", retval);
		}

		/* Initialize the EventSet */
		retval = PAPI_create_eventset(&EventSet1);
		if(retval != PAPI_OK)
			error_handler(__LINE__, "PAPI_create_eventset", retval);

		/* Force addition of component */
		retval = PAPI_assign_eventset_component(EventSet1, 0);
		if(retval != PAPI_OK){
			error_handler(__LINE__, "PAPI_assign_eventset_component", retval);
		}

		/* Add inherit support */
		PAPI_option_t opt;
		memset(&opt, 0x0, sizeof(PAPI_option_t));

		opt.inherit.inherit = PAPI_INHERIT_ALL;
		opt.inherit.eventset = EventSet1;
		if ( ( retval = PAPI_set_opt( PAPI_INHERIT, &opt ) ) != PAPI_OK ) {
			if ( retval == PAPI_ECMP) {
				error_handler( __LINE__, "Inherit not supported by current component.\n", retval );
			} else if (retval == PAPI_EPERM) {
				error_handler( __LINE__, "Inherit not supported by current component.\n", retval );
			} else {
				error_handler(__LINE__, "PAPI_set_opt", retval );
			}
		}

		/* Get events from environment variable */
		if(getenv("PAPI_EVENT") == NULL)
			error_handler(__LINE__, "PAPI_EVENT is not defined!", 1);
		else{
			list_events = (char **) calloc(PAPI_MAX_EVENTS, sizeof(char *));
			char *str;
			str = strtok(getenv("PAPI_EVENT"),",");
			while(str != NULL){
				list_events[num_events] = (char *) calloc(strlen(str) + 1, sizeof(char));
				strcpy(list_events[num_events++], str);
				str = strtok(NULL, ",");
				if(num_events > PAPI_MAX_EVENTS)
					error_handler(__LINE__, "Maximum number of PAPI events reached", 1);
			}
			list_events = (char **) realloc(list_events, num_events * sizeof(char *));
		}
		values = (long long int *) calloc(num_events, sizeof(long long));

		/* Add events */
		for(i = 0; i < num_events; i++){
			retval = PAPI_add_named_event(EventSet1, list_events[i]);
			if(retval != PAPI_OK)
				error_handler(__LINE__, "Trouble adding event", retval);
		}

		/* Remove underlines from events' name */
		for(i = 0; i < num_events; i++){
			char *ptr = list_events[i];
			while(*ptr != '\0'){
				if(*ptr == '_')
					*ptr='-';
				ptr++;
			}
		}

		/* Output directories */
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);

		struct stat st = {0};

		sprintf(log_dir, "./log");
		if(stat(log_dir, &st) == -1)
	    	mkdir(log_dir, 0700);

		sprintf(log_dir, "./log/%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
		if(stat(log_dir, &st) == -1)
	    	mkdir(log_dir, 0700);

		sprintf(log_dir, "%s/%02d-%02d-%02d/",log_dir, tm.tm_hour, tm.tm_min, tm.tm_sec);
		if(stat(log_dir, &st) == -1)
	    	mkdir(log_dir, 0700);

	    /* Output file */
	    sprintf(file, "%s/%s.freq", log_dir, short_exec);
		f = fopen(file, "w");
		if(f == NULL)
			error_handler(__LINE__, "Trouble creating the output file", retval);

		/* Register the timestamp and the file header */
		fprintf(f, "%lu\n", (unsigned long) time(NULL));
		fprintf(f, "event,value\n");

		/* Start PAPI */
		retval = PAPI_start(EventSet1);
		if(retval != PAPI_OK)
			error_handler(__LINE__, "PAPI_start", retval);

		/* The child runs the application while the parent measure the hardware counters */
		pid = fork();
		if(pid < 0)
			error_handler(__LINE__, "fork", 1);
		/* Parent */
		else if(pid){
			pid_t child;
			int status;

			/* Read hardware counters until child execution' ends */
			while(1){
				/* Reading the counters */
				retval = PAPI_read(EventSet1, values);
				if(retval != PAPI_OK)
					error_handler(__LINE__, "PAPI_read", retval);

				/* Writting the hardware counters */
				for(i = 0; i < num_events; i++)
					fprintf(f, "%s,%lld\n", list_events[i], values[i]);

				/* Reducing overhead with a delay between samples */
				usleep((useconds_t) sample_delay_ms * 1000);

				/* Is the child running? */
				child = waitpid(pid, &status, WNOHANG);
				if(child == pid)
					break;
			}

			/* Write the output file */
			fclose(f);

			/* Check child status */
			if(WEXITSTATUS(status) != 0)
				error_handler(__LINE__, "Exit status of child to attach to", PAPI_EMISC);

			/* Stop PAPI */
			retval = PAPI_stop(EventSet1, values);
			if(retval != PAPI_OK)
				error_handler(__LINE__, "PAPI_stop", retval);

			/* Remove the Events */
			retval = PAPI_cleanup_eventset(EventSet1);
			if(retval != PAPI_OK)
				error_handler(__LINE__, "PAPI_cleanup_eventset", retval);			

			/* Shutdown the EventSet */
			retval = PAPI_destroy_eventset(&EventSet1);
			if(retval != PAPI_OK)
				error_handler(__LINE__, "PAPI_destroy_eventset", retval);

			/* Freeing allocated memory */
			for(i = 0; i < num_events; i++)
				free(list_events[i]);
			free(list_events);
			free(values);


			exit(EXIT_SUCCESS);
		}
	}
}
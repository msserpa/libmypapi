#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <papi.h>

char *event, exe[2048];
int retval, length, EventSet1 = PAPI_NULL;
long long value[2];
long long overflows = 0;

/* Use a positive value of retval to simply print an error message */
void error_handler(int line, const char *call, int retval){
	if(retval >= 0 || retval == PAPI_ESYS)
		fprintf(stderr, "libmypapi - Error: %s on line %d\n", call, line);
	else
		fprintf(stderr, "libmypapi - Error: %s : %s on line %d\n\n", call, PAPI_strerror(retval), line);

	_exit(1);
}

void overflow_handler(int EventSet, void *address, long_long overflow_vector, void *context){
	overflows++;
	if(overflows >= LLONG_MAX / (long long) INT_MAX)
		error_handler(__LINE__, "overflow", 1);	
}

static void __attribute__ ((constructor)) constructor();

static void constructor(){
	length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if(length == -1)
        error_handler(__LINE__, "readlink", 1);

    /* Only programs whose name ends with ".x" are accepted */
	if(exe[length - 2] == '.' && exe[length - 1] == 'x'){

		/* Init the PAPI library */
		retval = PAPI_library_init(PAPI_VER_CURRENT);
		if(retval != PAPI_VER_CURRENT){
			error_handler(__LINE__, "PAPI_library_init", retval);
		}

		/* Initialize the EventSet */
		retval = PAPI_create_eventset(&EventSet1);
		if(retval != PAPI_OK)
			error_handler(__LINE__, "PAPI_create_eventset", retval);

		/* Get environment variable */
		event = getenv("PAPI_EVENT");
		if(event == NULL)
			error_handler(__LINE__, "PAPI_EVENT is not defined", 1);

		/* Add event from ENV */
		retval = PAPI_add_named_event(EventSet1, event);
		if(retval != PAPI_OK){
			error_handler(__LINE__, "Trouble adding event", retval);
		}

		int eventCode;

		retval = PAPI_event_name_to_code(event, &eventCode);
		if(retval != PAPI_OK){
  			switch(retval){
  				case PAPI_EINVAL: 
  					error_handler(__LINE__, "One or more of the arguments is invalid", retval);
  				break;
  				case PAPI_ENOTPRESET:
  					error_handler(__LINE__, "The hardware event specified is not a valid PAPI preset", retval);
  				break;
  				case PAPI_ENOEVNT:
  					error_handler(__LINE__, "The PAPI preset is not available on the underlying hardware", retval);
  				break;
  				case PAPI_ENOINIT:
  					error_handler(__LINE__, "The PAPI library has not been initialized", retval);
  				break;
  				default:
  					error_handler(__LINE__, "PAPI_event_name_to_code error", retval);
  			}
  		}

  		if(eventCode < 0)
  			printf("libmypapi - Warning: eventCode is not defined!\n");
  		else{
	  		retval = PAPI_overflow(EventSet1, eventCode, INT_MAX, 0, overflow_handler);
	  		if(retval != PAPI_OK){
	  			switch(retval){
	  				case PAPI_EINVAL: 
	  					error_handler(__LINE__, "One or more of the arguments is invalid. Specifically, a bad threshold value", retval);
	  				break;
	  				case PAPI_ENOMEM:
	  					error_handler(__LINE__, "Insufficient memory to complete the operation", retval);
	  				break;
	  				case PAPI_ENOEVST:
	  					error_handler(__LINE__, "The EventSet specified does not exist", retval);
	  				break;
	  				case PAPI_EISRUN:
	  					error_handler(__LINE__, "The EventSet is currently counting events", retval);
	  				break;
	  				case PAPI_ECNFLCT:
	  					error_handler(__LINE__, "The underlying counter hardware cannot count this event and other events in the EventSet simultaneously. Or you are trying to overflow both by hardware and by forced software at the same time", retval);
	  				break;
	  				case PAPI_ENOEVNT:
	  					error_handler(__LINE__, "The PAPI preset is not available on the underlying hardware", retval);
	  				break;
	  				case PAPI_ENOINIT:
	  					error_handler(__LINE__, "The PAPI library has not been initialized", retval);
	  				break;  				
	  				default:
	  					error_handler(__LINE__, "PAPI_EVENT overflowed", retval);
	  			}
	  		}
  		}

		/* Start PAPI */
		retval = PAPI_start(EventSet1);
		if(retval != PAPI_OK)
			error_handler(__LINE__, "PAPI_start", retval);

	}
}

static void __attribute__ ((destructor)) destructor();

static void destructor(){
    /* Only programs whose name ends with ".x" are accepted */
	if(exe[length - 2] == '.' && exe[length - 1] == 'x'){

		/* Stop PAPI */
		retval = PAPI_stop(EventSet1, value);
		if(retval != PAPI_OK)
			error_handler(__LINE__, "PAPI_stop", retval);

		/* Shutdown the EventSet */
		retval = PAPI_remove_named_event(EventSet1, event);
		if(retval != PAPI_OK)
			error_handler(__LINE__, "PAPI_remove_named_event", retval);

		retval = PAPI_destroy_eventset(&EventSet1);
		if(retval != PAPI_OK)
			error_handler(__LINE__, "PAPI_destroy_eventset", retval);
	
		/* It checks if event name is cuda-based and split it */
		unsigned int i, j;

		for(i = 0; i < strlen(event); i++)
			if(strncmp(&event[i], "event:", 6) == 0){
				i += 6;
				break;
			}

		for(j = i; j < strlen(event); j++)
			if(strncmp(&event[j], "device", 6) == 0){
				event[j - 1] = '\0';
				break;
			}

		/* If it is not cuda-based then do not split */
		if(i == j)
			i = 0;

		printf("%s,%lld\n", &event[i], value[0]);

	}
}
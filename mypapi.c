#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <papi.h>

char *event, exe[2048];
int retval, length, EventSet1 = PAPI_NULL;
long long value[2];

/* Use a positive value of retval to simply print an error message */
void test_fail(const char *file, int line, const char *call, int retval){
	char buf[128];
	(void)file;

	memset(buf, '\0', sizeof(buf));

	fprintf(stderr, "FAILED!!!");
	fprintf(stderr, "\nLine # %d ", line);

	if(retval == PAPI_ESYS){
		sprintf(buf, "System error in %s", call);
		perror(buf);
	}else if(retval > 0)
		fprintf(stdout, "Error: %s\n", call);
	else if(retval == 0)
		fprintf(stderr, "Error: %s\n", call);
	else
		fprintf(stderr, "Error in %s: %s\n", call, PAPI_strerror(retval));

	if(PAPI_is_initialized())
		PAPI_shutdown();

	exit(1);
}

static void __attribute__ ((constructor)) constructor();

static void constructor(){
	length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if(length == -1)
        test_fail(__FILE__, __LINE__, "readlink", 1);

    /* Only programs whose name ends with ".x" are accepted */
	if(exe[length - 2] == '.' && exe[length - 1] == 'x'){

		/* Init the PAPI library */
		retval = PAPI_library_init(PAPI_VER_CURRENT);
		if(retval != PAPI_VER_CURRENT){
			test_fail(__FILE__, __LINE__, "PAPI_library_init", retval);
		}

		/* Initialize the EventSet */
		retval = PAPI_create_eventset(&EventSet1);
		if(retval != PAPI_OK)
			test_fail(__FILE__, __LINE__, "PAPI_create_eventset", retval);

		/* Get environment variable */
		event = getenv("PAPI_EVENT");
		if(event == NULL)
			test_fail(__FILE__, __LINE__, "PAPI_EVENT is not defined", 1);

		/* Add event from ENV */
		retval = PAPI_add_named_event(EventSet1, event);
		if(retval != PAPI_OK){
			test_fail(__FILE__, __LINE__, "Trouble adding event", retval);
		}

		/* Start PAPI */
		retval = PAPI_start(EventSet1);
		if(retval != PAPI_OK)
			test_fail(__FILE__, __LINE__, "PAPI_start", retval);

	}
}

static void __attribute__ ((destructor)) destructor();

static void destructor(){
    /* Only programs whose name ends with ".x" are accepted */
	if(exe[length - 2] == '.' && exe[length - 1] == 'x'){

		/* Stop PAPI */
		retval = PAPI_stop(EventSet1, value);
		if(retval != PAPI_OK)
			test_fail(__FILE__, __LINE__, "PAPI_stop", retval);

		/* Shutdown the EventSet */
		retval = PAPI_remove_named_event(EventSet1, event);
		if(retval != PAPI_OK)
			test_fail(__FILE__, __LINE__, "PAPI_remove_named_event", retval);

		retval = PAPI_destroy_eventset(&EventSet1);
		if(retval != PAPI_OK)
			test_fail(__FILE__, __LINE__, "PAPI_destroy_eventset", retval);

		fprintf(stderr, "%s=%lld\n", event, value[0]);

	}
}
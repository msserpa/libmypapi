#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#ifndef BUFFER_SIZE
	#define BUFFER_SIZE 1024
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <time.h>

char exe[BUFFER_SIZE], short_exe[BUFFER_SIZE], log_dir[BUFFER_SIZE];
uint32_t i, length, max_cpus, freq_time_ms = 1;
pthread_t *thread_freq;
volatile uint32_t alive = 0;

void *freq_monitor(void *data){
	printf("freq monitor!\n");
	uint64_t *cpu = (uint64_t *) data;
	char buffer[BUFFER_SIZE];
	size_t read;
	FILE *fr, *fw;

	sprintf(buffer, "/sys/devices/system/cpu/cpu%lu/cpufreq/scaling_cur_freq", *cpu);
	fr = fopen(buffer, "r");

	sprintf(buffer, "%s/%s.%lu.freq", log_dir, short_exe, *cpu);
	fw = fopen(buffer, "w");
	
	fprintf(fw, "%lu\n", (unsigned long) time(NULL));
	while(alive){
		read = fread(buffer, 1, BUFFER_SIZE - 1, fr);
		buffer[read] = '\0';
	 	fseek(fr, 0, SEEK_SET);
	 	fwrite(buffer, 1, read, fw);
		usleep((uint64_t) freq_time_ms * 1000);
	}

	fclose(fr);
	fclose(fw);

	pthread_exit(NULL);
}

static void __attribute__ ((constructor)) constructor();

static void constructor(){
	length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if(length == -1){
		printf("readlink failed\n");
		exit(EXIT_FAILURE);
	}

    /* Only programs whose name ends with ".x" are accepted */
	if(exe[length - 2] == '.' && exe[length - 1] == 'x'){

		strcpy(short_exe,  strrchr(exe, '/') + 1);

		time_t t = time(NULL);
		struct tm tm = *localtime(&t);

		struct stat st = {0};

		sprintf(log_dir, "./log/%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
		if(stat(log_dir, &st) == -1)
	    	mkdir(log_dir, 0700);

		sprintf(log_dir, "%s/%02d-%02d-%02d",log_dir, tm.tm_hour, tm.tm_min, tm.tm_sec);
		if(stat(log_dir, &st) == -1)
	    	mkdir(log_dir, 0700);		

		
		if(getenv("OMP_NUM_THREADS"))
			max_cpus = atoi(getenv("OMP_NUM_THREADS"));
		else
			max_cpus = sysconf(_SC_NPROCESSORS_ONLN);

		thread_freq = calloc(max_cpus, sizeof(pthread_t));

		alive = 1;
		for(i = 0; i < max_cpus; i++){
			uint64_t *cpu = (uint64_t *) malloc(sizeof(uint64_t));
			*cpu = i;
			pthread_create(&thread_freq[i], NULL, freq_monitor, (void *) cpu);
		}
	}
}

static void __attribute__ ((destructor)) destructor();

static void destructor(){
    /* Only programs whose name ends with ".x" are accepted */
	if(exe[length - 2] == '.' && exe[length - 1] == 'x'){

		alive = 0;
		
		for(i = 0; i < max_cpus; i++)
			pthread_join(thread_freq[i], NULL);

	}
}
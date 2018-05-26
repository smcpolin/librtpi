/******************************************************************************
 *
 *   Copyright © International Business Machines  Corp., 2009
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * NAME
 *      pthread_cond_hang.c
 *
 * DESCRIPTION
 *      Using vanilla cond vars results in a pthread_cond_wait priority
 *      inversion deadlock. With PI cond vars it works fine.
 *
 * USAGE:
 *      gcc -D_GNU_SOURCE -lpthread -lrt -ldl pthread_cond_hang.c \
 *      -o pthread_cond_hang
 *
 *      export LD_LIBRARY_PATH=path_to_libpthread.so_with_pi_cond_api
 *      ./pthread_cond_hang => WILL PASS (on SMP)
 *      taskset -c 0 ./pthread_cond_hang -p0 => WILL HANG
 *      taskset -c 0 ./pthread_cond_hang -p1 => WILL PASS
 *
 * AUTHOR
 *      Dinakar Guniguntala <dino@in.ibm.com>
 *
 * HISTORY
 *      2009-Nov-04: Created based on John Stultz's testcase
 *      2009-Nov-12: Added dlsym support by Darren Hart <dvhltc@us.ibm.com>
 *
 *****************************************************************************/

#include <dlfcn.h>
#include <stdio.h>
#include <sched.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#define NUM_THREADS	1
#define LOW_PRIO	30
#define MED_PRIO	50
#define HIGH_PRIO	70

pthread_cond_t race_var;
pthread_mutex_t race_mut;

pthread_cond_t sig1, sig2, sig3;
pthread_mutex_t m1, m2, m3;

unsigned int use_pi = 0;
unsigned int done = 0;

int (*dl_pthread_condattr_setprotocol)(pthread_condattr_t *attr, int protocol) = NULL;

void usage(void)
{
	printf("pthread_cond_hang options\n");
	printf("  -p(0,1)	0: don't use pi cond, 1: use pi cond (default 0)\n");
}

int dl_function_init()
{
	char *lib = "libpthread.so.0";
	char *error;
	int ret = 0;

	void *dl_libpthread = dlopen(lib, RTLD_LAZY);
	if (dl_libpthread) {
		dlerror();
		*(void **)(&dl_pthread_condattr_setprotocol) = 
			dlsym(dl_libpthread, "pthread_condattr_setprotocol_np");
		error = dlerror();
		if (error)
			fprintf(stderr, "%s\n", error);
		dlclose(dl_libpthread);

	} else {
		error = dlerror();
		fprintf(stderr, "dlopen failed: %s\n", error);
		ret = -1;
	}
	return ret;
}

int parse_args(int c, char *v[])
{
	char options[] = "p:h";
	int op;

	while ((op = getopt(c, v, options)) != -1) {
		switch (op) {
			case 'p':
				use_pi = atoi(optarg);
				break;

			case 'h':
			default:
				usage();
				exit(0);
		}
	}
	return 0;
}

void* low_thread(void* arg)
{
	/*signal block*/
	pthread_mutex_lock(&m1);
	pthread_cond_wait(&sig1, &m1);

	/*race block*/
	pthread_mutex_lock(&race_mut);
	printf("Low prio thread: locked\n");
	pthread_cond_signal(&sig2);

	pthread_cond_wait(&race_var, &race_mut);
	printf("Low prio thread: done waiting\n");
	pthread_mutex_unlock(&race_mut);

	return NULL;
}


void* high_thread(void* arg)
{
	/*signal block*/
	pthread_mutex_lock(&m2);
	pthread_cond_wait(&sig2, &m2);

	/*race block*/
	pthread_mutex_lock(&race_mut);
	printf("Hi  prio thread: locked\n");

	pthread_cond_signal(&sig3);

	pthread_cond_wait(&race_var, &race_mut);
	printf("Hi  prio thread: done waiting\n");
	pthread_mutex_unlock(&race_mut);

	done = 1;
	return NULL;
}


void* medium_thread(void* arg)
{
	/*signal block*/
	pthread_mutex_lock(&m3);
	pthread_cond_wait(&sig3, &m3);

	printf("Med prio thread: spinning\n");

	/*race block*/
	while(!done)
		/*busy wait to block low threads*/;
	return NULL;
}


int main(int argc, char* argv[])
{
	pthread_t low_threads[NUM_THREADS];
	pthread_t med_threads[NUM_THREADS];
	pthread_t hi_threads[NUM_THREADS];
	struct sched_param param;
	pthread_attr_t attr;
	pthread_mutexattr_t m_attr;
	pthread_condattr_t c_attr;

	int i, ret;

	parse_args(argc, argv);

	if (use_pi && (ret = dl_function_init()) != 0)
		return ret;

	pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

	pthread_condattr_init(&c_attr);

	if (use_pi) {
		if (dl_pthread_condattr_setprotocol) {
			dl_pthread_condattr_setprotocol(&c_attr, PTHREAD_PRIO_INHERIT);
		} else {
			fprintf(stderr, "PI Condvars unavailable, aborting\n");
			return -1;
		}
	}

	if ((ret = pthread_cond_init(&sig1, &c_attr)) != 0)
		perror("Failed to init cond sig1");
	if ((ret = pthread_cond_init(&sig2, &c_attr)) != 0)
		perror("Failed to init cond sig2");
	if ((ret = pthread_cond_init(&sig3, &c_attr)) != 0)
		perror("Failed to init cond sig3");
	if ((ret = pthread_cond_init(&race_var, &c_attr)) != 0)
		perror("Failed to init cond race_var");

	pthread_mutexattr_init(&m_attr);
	pthread_mutexattr_setprotocol(&m_attr, PTHREAD_PRIO_INHERIT);
	if ((ret = pthread_mutex_init(&m1, &m_attr)) != 0)
		perror("Failed to init mutex m1");
	if ((ret = pthread_mutex_init(&m2, &m_attr)) != 0)
		perror("Failed to init mutex m2");
	if ((ret = pthread_mutex_init(&m3, &m_attr)) != 0)
		perror("Failed to init mutex m3");
	if ((ret = pthread_mutex_init(&race_mut, &m_attr)) != 0)
		perror("Failed to init mutex race_mut");

	param.sched_priority = 90;
        sched_setscheduler(0, SCHED_FIFO, &param);

	param.sched_priority = LOW_PRIO;
        pthread_attr_setschedparam(&attr, &param);
	for(i=0; i<NUM_THREADS; i++)
		pthread_create(&low_threads[i], &attr, low_thread,(void*)NULL);

	param.sched_priority = MED_PRIO;
        pthread_attr_setschedparam(&attr, &param);
	for(i=0; i<NUM_THREADS; i++)
		pthread_create(&med_threads[i], &attr, medium_thread,(void*)NULL);

	param.sched_priority = HIGH_PRIO;
        pthread_attr_setschedparam(&attr, &param);
	for(i=0; i<NUM_THREADS; i++)
		pthread_create(&hi_threads[i], &attr, high_thread,(void*)NULL);

	usleep(1000);
	pthread_cond_signal(&sig1);
	usleep(1000);
	pthread_cond_broadcast(&race_var);

	for(i=0; i<NUM_THREADS; i++)
		pthread_join(low_threads[i], (void**) NULL);
	for(i=0; i<NUM_THREADS; i++)
		pthread_join(med_threads[i], (void**) NULL);
	for(i=0; i<NUM_THREADS; i++)
		pthread_join(hi_threads[i], (void**) NULL);

	return 0;
}

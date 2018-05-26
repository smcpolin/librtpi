/******************************************************************************
 *
 *   Copyright © International Business Machines  Corp., 2010
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
 *      condvar_perf.c
 *
 * DESCRIPTION
 *      This test is designed to test the efficiency of the pthread_cond_wait
 *      and pthread_cond_signal code paths within glibc.
 *
 * AUTHOR
 *      Darren Hart <dvhltc@us.ibm.com>
 *
 * HISTORY
 *      2010-Feb-23: Initial version by Darren Hart <dvhltc@us.ibm.com>
 *
 *****************************************************************************/

#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <stdio.h>
#include <pthread.h>
#include <getopt.h>
#include <math.h>

#define USEC_PER_SEC 1000000
#define NSEC_PER_USEC 1000
#define DEFAULT_ITERS 10000
#define DEFAULT_POPULATION 10000

#define MIN(A,B) ((A)<(B)?(A):(B))
#define MAX(A,B) ((A)>(B)?(A):(B))

static int iters = DEFAULT_ITERS;
static int sched_fifo_prio = 0;
static int synch = 0;
static int cur_iter;

static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -f #  Run as SCHED_FIFO prio # (default SCHED_OTHER)\n");
	printf("  -h	Display this help message\n");
	printf("  -i #	Number of iterations (default %d)\n", DEFAULT_ITERS);
	printf("  -p #	Population size per datapoint (default %d)\n", DEFAULT_POPULATION);
	printf("  -s 	Synchronous signaling (lock mutex)\n");
}

void * waiter(void *arg)
{
	while (cur_iter) {
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond, &mutex);
		cur_iter--;
		pthread_mutex_unlock(&mutex);
	}
	return NULL;
}

float cond_perf(void)
{
	struct timespec start_ts, end_ts;
	long long delta_us;
	float result = 0.0;
	pthread_t child_id;

	cur_iter = iters;

	if ((pthread_create(&child_id, NULL, waiter, NULL))) {
		perror("pthread_create failed");
		exit(1);
	}

	clock_gettime(CLOCK_REALTIME, &start_ts);

	while (cur_iter) {
		if (synch) pthread_mutex_lock(&mutex);
		pthread_cond_signal(&cond);
		if (synch) pthread_mutex_unlock(&mutex);
	}

	clock_gettime(CLOCK_REALTIME, &end_ts);

	pthread_join(child_id, NULL);

	delta_us = (end_ts.tv_sec - start_ts.tv_sec) * USEC_PER_SEC;
	delta_us += (end_ts.tv_nsec - start_ts.tv_nsec) / NSEC_PER_USEC;
	result = (float)iters/((float)delta_us/USEC_PER_SEC);

	return result;
}

static int n = 0;
static float mean = 0.0;
static float m2 = 0.0;
static float var = 0.0;
static float min = 0.0;
static float max = 0.0;
void online_variance(float x)
{
	float delta;
	if (n++ == 0)
		min = x;
	min = MIN(min, x);
	max = MAX(max, x);
	delta = x - mean;
	mean = mean + delta/n;
	m2 = m2 + delta*(x - mean);
	var = m2/n; /* population variance */
}

int main(int argc, char *argv[])
{
	int population = DEFAULT_POPULATION;
	int c, i, ret = 0;

	while ((c = getopt(argc, argv, "f:hi:p:s")) != -1) {
		switch(c) {
		case 'f':
			sched_fifo_prio = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'i':
			iters = atoi(optarg);
			break;
		case 'p':
			population = atoi(optarg);
			break;
		case 's':
			synch = 1;
			break;
		default:
			usage(argv[0]);
			exit(1);
		}
	}

	printf("Scheduling: %s %d\n", sched_fifo_prio ? "SCHED_FIFO" : "SCHED_OTHER",
				      sched_fifo_prio);
	printf("Iterations: %d\n", iters);
	printf("Population: %d\n", population);

	if (sched_fifo_prio) {
		struct sched_param sp;
		sp.sched_priority = sched_fifo_prio;
		if ((ret = sched_setscheduler(0, SCHED_FIFO, &sp))) {
			perror("sched_setscheduler");
			goto out;
		}
	}

	for (i = 0; i < population; i++)
		online_variance(cond_perf());

	printf("Min: %f\n", min);
	printf("Max: %f\n", max);
	printf("Avg: %f\n", mean);
	printf("Var: %f\n", var);
	printf("Std: %f\n", sqrt(var));
 out:
	return ret;
}

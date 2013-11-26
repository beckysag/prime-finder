/*
 * Find all prime numbers within the range up to a 32-bit unsigned integer
 * usage: primePThread [-qv] [-m max_prime] [-c concurrency]
 *
 * to compile: gcc -pthread -o primePThread primePThread.c
 *
 * Rebecca Sagalyn
 */

#define _POSIX_SOURCE
#define _BSD_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>	/* for debug functions */

unsigned int next_prime(unsigned int cur_seed);
void* find_primes(void *t);
void print_primes(unsigned int n);
void printbits(unsigned int arr[], int i);
void perror_and_exit(char *msg);
int count_primes();
void usage();

/****************************************************************/
/*              Macros and typedefs                      		*/
/****************************************************************/
typedef enum { FALSE, TRUE } Boolean;
#define INT_BIT 32
#define OFFT 1

#define ISCOMPOSITE(x,i) ((x[i/INT_BIT] & (1<<(i % INT_BIT)))!=0)
#define ISPRIME(x,i) ((x[(i-OFFT)/INT_BIT] & (1 << ((i-OFFT)%INT_BIT))) ==0)
#define SET_NOT_PRIME(x,i) x[(i-OFFT)/INT_BIT]|=(1<<((i-OFFT)%INT_BIT));
#define SET_PRIME(x,i) x[(i-OFFT)/INT_BIT]&=(1<<((i-OFFT)%INT_BIT))^0xFFFFFFFF;

/****************************************************************/
/*              Global variables                                */
/****************************************************************/
unsigned int *bit_arr;
unsigned int num_workers;
unsigned int max_prime;
unsigned int prime_count;
pthread_mutex_t mtx;
Boolean verbose = FALSE; // if TRUE, show timings and count

/****************************************************************/
/*              Function definitions                            */
/****************************************************************/
int main(int argc, char **argv) {
	Boolean be_guiet = FALSE; // if TRUE, be quiet
	unsigned int i, j = 0;
	int c;
	unsigned long t = 0;
	double seconds;
	time_t start, end;

	/* Set default values for threads and primes */
	num_workers = 1;
	max_prime = UINT_MAX;

	/* Parse options */
	while ((c = getopt(argc, argv, "c:m:qv")) != -1) {
		switch (c) {
		case 'c':
			num_workers = atoi(optarg);
			break;
		case 'm':
			max_prime = atoi(optarg);
			break;
		case 'q':
			be_guiet = TRUE;
			break;
		case 'v':
			verbose = TRUE;
			break;
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}

	/* start timing if needed */
	if (verbose == TRUE)
		time(&start);

	/* Allocate memory for the bit array */
	unsigned int size = ceil((max_prime - 1) / INT_BIT)+1;
	bit_arr = malloc(size * (sizeof(unsigned int)));

	/* Allocate the number of pthreads given by num_workers */
	pthread_t *threads = (pthread_t*) malloc(num_workers * sizeof(pthread_t));

	/* Initialize bit array: mark all evens as non-prime. */
	for (i = 0; i < size; i++)
		bit_arr[i] = 0b10101010101010101010101010101010; // l -> r: bit-32 -> bit-1


	/*  mark multiples of 3 as non prime */
	for (i = 0; i + 2 < size; i += 3) {
		bit_arr[i] = bit_arr[i] ^ 0b00000100000100000100000100000100;
		bit_arr[i + 1] = bit_arr[i + 1] ^ 0b01000001000001000001000001000001;
		bit_arr[i + 2] = bit_arr[i + 2] ^ 0b00010000010000010000010000010000;
	}
	/* set any remainder elemnts */
	if (i < size) bit_arr[i] = bit_arr[i] ^ 0b00000100000100000100000100000100;
	if (i + 1 < size) bit_arr[i + 1] = bit_arr[i + 1] ^ 0b01000001000001000001000001000001;

	SET_NOT_PRIME(bit_arr, 1);	/* set 1 to not prime */
	SET_PRIME(bit_arr, 2);		/* set 2 to prime */
	SET_PRIME(bit_arr, 3); 		/* unmark 3: set 3 to prime */

	/* Mark primes from 5 to sqrt(max_prime) */
	for (i = 5; i <= sqrt(sqrt(max_prime)); i++) {
		if (ISPRIME(bit_arr,i)) {
			for (j = 5; (i * j) <= (sqrt(max_prime)); j++)
				SET_NOT_PRIME(bit_arr, (i*j)); //all multiples of i <= limit
		}
	}

	/* Initialize mutex object */
	pthread_mutex_init(&mtx, NULL);

	/* Initialize thread attribute to create threads in joinable state */
	pthread_attr_t attr;
	if ( pthread_attr_init(&attr) != 0)
		perror_and_exit("pthread_attr_init()");
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0)
		perror_and_exit("pthread_attr_setdetachstate()");


	/* Create threads */
	for (i = 0; i < num_workers; i++) {
		if (pthread_create(&threads[i], &attr, find_primes, (void *) t) != 0)
			perror_and_exit("pthread_create()");
		t++;
	}

	/* Wait for threads to complete */
	for (i = 0; i < num_workers; i++) {
		if (pthread_join(threads[i], NULL) != 0)
			perror_and_exit("pthread_join()");
	}

	/* Print the primes, if not quiet */
	if (be_guiet == FALSE) print_primes(max_prime);

	if (verbose == TRUE) {
		/* Count the primes */
		count_primes();
		printf("Number of primes is %d.\n", prime_count);

		/* Print timings */
		time(&end);
		seconds = difftime(end, start);
		printf("%.f seconds.\n", seconds);
	}

	/* Clean up */
	if (pthread_attr_destroy(&attr) != 0)
		perror_and_exit("Error destroying pthread attr.\n");
	if (pthread_mutex_destroy(&mtx) != 0)
		perror_and_exit("Error destroying mutex.\n");
	free(bit_arr);
	free(threads);
	pthread_exit(NULL);
	return 0;
}

void *find_primes(void *t) {
	/* Mark composites within calculated range */
	int work_done = 0;
	int mycount = 0;
	long myid = (long) t;
	unsigned int j,k;
	unsigned int min = floor(myid * (max_prime + 1) / num_workers);
	unsigned int max = floor((myid + 1) * ((max_prime + 1) / num_workers)) - 1;

	j = 3; // while loop will start with 5, all muliples of 2 & 3 already marked
	while ((j = next_prime(j)) != 0) {
		mycount++;
		for (k = (min / j < 5) ? 5 : (min / j); (j * k) <= max; k++) {
			if (ISPRIME(bit_arr, (j*k))) { // if not yet marked as composite
				pthread_mutex_lock(&mtx);
				SET_NOT_PRIME(bit_arr, (j*k));//all multiples of j <= max_prime
				pthread_mutex_unlock(&mtx);
				work_done++;
			}
		}
	}
	if (verbose == TRUE)
		printf ("thread %lu   min: %d  max: %d  count: %d   work: %d\n", myid, min, max, mycount, work_done);
	pthread_exit(NULL);
	return 0;
}

unsigned int next_prime(unsigned int cur_seed) {
	/* Return next seeded prime, or 0 at max_prime */
	unsigned int i;
	for (i = cur_seed + 2; i <= sqrt(max_prime); i += 2)
		if (ISPRIME(bit_arr,i))
			return i;
	return 0;
}

void perror_and_exit(char *msg) {
	/* print error message and exit with failure status */
	perror(msg);
	exit(EXIT_FAILURE);
}

int count_primes() {
	/* Counts number of primes in bit_arr */
	int i;
	prime_count = 0;
	for (i = 0; i < max_prime; i++) {
		/* start at i=0, which corresponds to number 1 */
		if (ISPRIME(bit_arr, i))
			prime_count++;
	}
	return -1;
}

void printbits(unsigned int arr[], int i) {
	/* for debugging */
	int n;
	bool b;
	for (n = (32 * i); n < (32 * (i + 1)); n++) {
		b = ISCOMPOSITE(arr, n);
		if (b)
			printf("%d", 1);
		else
			printf("%d", 0);
	}
	printf("\n");
}

void print_primes(unsigned int n) {
	/* prints primes */
	unsigned int i;
	for (i = 2; i <= n; i++) {
		if (ISPRIME(bit_arr,i))
			printf("%u\n", i);
	}
}

void usage() {
	/* print usage message */
	printf("%s  %s", "Usage:\n",
			"primePThread [-qv] [-m max_prime] [-c concurrency]\n\n");
	printf("%s", "Options:\n");
	printf("%*s%*s\n", 4, "-q", 29, "don't print prime numbers");
	printf("%*s%*s\n", 4, "-v", 23, "verbose: print timing and count");
	printf("%*s%*s\n", 4, "-m", 36, "maximum size of the prime number");
	printf("%*s%*s\n", 4, "-c", 23, "concurrency to use\n");
}


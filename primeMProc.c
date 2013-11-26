/**
 * Finds all prime numbers within the range up to a 32-bit unsigned integer
 * usage: primePThread [-qv] [-m max_prime] [-c concurrency]
 *
 * Rebecca Sagalyn
 *
 */

#define _POSIX_SOURCE
#define _BSD_SOURCE		/* ftruncate() */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>		/* ftruncate() */
#include <stdbool.h>	/* for debug functions */

void find_primes(unsigned int **args);
unsigned int next_prime(unsigned int cur_seed, unsigned int max_prime, unsigned int *arr);
void print_primes(unsigned int n,unsigned int *arr);
void printbits(unsigned int arr[], int i);
void count_primes(unsigned int *arr,unsigned int *prime_count, unsigned int max);
void perror_and_exit(char *msg);
void usage();

/********************************************************************/
/* 				Global macros and typedefs							*/
/********************************************************************/
typedef enum {
	FALSE, TRUE
} Boolean;

typedef unsigned int uint;

#define INT_BIT 32
#define OFFT 1

#define ISCOMPOSITE(x,i) ((x[i/INT_BIT] & (1<<(i % INT_BIT)))!=0)
#define ISPRIME(x,i) ((x[(i-OFFT)/INT_BIT] & (1 << ((i-OFFT)%INT_BIT))) ==0)
#define SET_NOT_PRIME(x,i) x[(i-OFFT)/INT_BIT]|=(1<<((i-OFFT)%INT_BIT));
#define SET_PRIME(x,i) x[(i-OFFT)/INT_BIT]&=(1<<((i-OFFT)%INT_BIT))^0xFFFFFFFF;



/********************************************************************/
/*				Function definitions								*/
/********************************************************************/
int main(int argc, char **argv) {
	unsigned int *bit_arr;
	unsigned int *prime_count = 0;
	unsigned int max_prime;
	unsigned int num_workers;
	unsigned int i, j = 0;
	Boolean be_quiet = FALSE; // if TRUE, be quiet
	Boolean verbose = FALSE; // if TRUE, show timings
	double seconds;
	time_t start, end;
	char *sh_path = "sh_mem_path";
	pid_t pid;
	int c, r;

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
			be_quiet = TRUE;
			break;
		case 'v':
			verbose = TRUE;
			break;
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}



	/* start clock if verbose */
	if (verbose == TRUE)
		time(&start);


	/* Set memory size needed for the bit array */
	unsigned int num_arr_elements = (int)ceil((max_prime - 1.0) / INT_BIT); // number of ints in array
	unsigned int bitmap_size = num_arr_elements * sizeof (unsigned int); // bytes in array
	unsigned int sh_size = bitmap_size + sizeof (unsigned int); // size of entire segment of shared memory

	/* Create shared memory object (connected to shm_fd) */
	int shm_fd = shm_open(sh_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (shm_fd < 0)
		perror_and_exit("In shm_open()");

	/* Set size of shared memory object to hold bit_arr */
	if (ftruncate(shm_fd, sh_size) == -1)
		perror_and_exit("In ftruncate()");

	/* Map shared memory object into process's virtual address space */
	void *mem = mmap(NULL, sh_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (mem == MAP_FAILED)
		perror_and_exit("mmap");

	// num_arr_elements = number of unsigned ints in bitarr
	bit_arr = (unsigned int*)mem;
	prime_count = bit_arr + num_arr_elements;

	/* Initialize bit array: mark all evens as non-prime.
	 * Sets bitmap_size bytes */
	for (i = 0; i < num_arr_elements; i++) {
		bit_arr[i] = 0b10101010101010101010101010101010; // l -> r: bit-32 -> bit-1
	}

	/* mark multiples of 3 as non prime */
	for (i = 0; i + 2 < num_arr_elements; i += 3) {
		bit_arr[i] = bit_arr[i]^0b00000100000100000100000100000100;
		bit_arr[i + 1] = bit_arr[i + 1]^0b01000001000001000001000001000001;
		bit_arr[i + 2] = bit_arr[i + 2]^0b00010000010000010000010000010000;
	}
	/* set any remainder elemnts */
	if (i < num_arr_elements)
		bit_arr[i] = bit_arr[i] ^ 0b00000100000100000100000100000100;
	if (i + 1 < num_arr_elements)
		bit_arr[i + 1] = bit_arr[i + 1] ^ 0b01000001000001000001000001000001;

	SET_NOT_PRIME(bit_arr, 1); /* set 1 to not prime */
	SET_PRIME(bit_arr, 2); /* set 2 to prime */
	SET_PRIME(bit_arr, 3); // unmark 3: set 3 to prime

	/* Mark primes from 5 to sqrt(max_prime) */
	unsigned int limit = sqrt(max_prime);
	for (i = 5; i <= sqrt(limit); i++) {
		if (ISPRIME(bit_arr, i)) {
			for (j = 5; (i * j) <= limit; j++)
				SET_NOT_PRIME(bit_arr, (i * j)); //all multiples of i <= limit
		}
	}

	/* Variables to pass to child processes*/
	unsigned int proc_no = 0;
	unsigned int **args = malloc(4 * sizeof(unsigned int *));
	*prime_count = 0;

	/* Create processes */
	for (i = 0; i < num_workers; i++) {
		switch (pid = fork()) {
		case -1:
			perror_and_exit("In fork()");
			exit(EXIT_FAILURE);
			break;

		/* child case*/
		case 0:
			args[0] = &proc_no;
			args[1] = &num_workers;
			args[2] = &max_prime;;
			args[3] = bit_arr;

			find_primes(args);

			/* unmap shared memory */
			r = munmap(mem, sh_size);
			if (r != 0)
				perror_and_exit("In munmap() of child");
			exit(EXIT_SUCCESS);
			break;

		/* parent case*/
		default:
			proc_no++;
			break;
		}
	}
	free(args);

	/* 'fd' is no longer needed once chlildren have inherited it */
	if (close(shm_fd) == -1)
		perror_and_exit("close fd");

	int status;
	/* wait for proceses to complete */
	for (i = 0; i < num_workers; i++) {
		r = wait(&status);
		if (r < 0)
			perror_and_exit("In wait()");
	}

	if (be_quiet == FALSE) {
		/* Print the primes */
		print_primes(max_prime,bit_arr);
	}

	if (verbose == TRUE) {
		/* Count the primes */
		count_primes(bit_arr, prime_count, max_prime);
		printf("Number of primes is %d.\n", *prime_count);

		/* Print timings */
		time(&end);
		seconds = difftime(end, start);
		printf("%.f seconds.\n", seconds);
	}

	/* Clean up */
	r = munmap(mem, sh_size);
	if (r != 0) perror_and_exit("In munmap()");
	r = shm_unlink(sh_path);
	if (r != 0) perror_and_exit("shm_unlink");
	return 0;
}



void find_primes(unsigned int **args) {
	/* Child process function to mark composites as not prime */
	int proc_no = *args[0];
	unsigned int num_workers = *args[1];
	unsigned int max_prime = *args[2];
	unsigned int *bit_arr = args[3];
	unsigned int min = floor(proc_no * (max_prime+1)/num_workers);
	unsigned int max = floor( (proc_no+1) * ((max_prime+1)/num_workers))-1;
	int work_done = 0;
	unsigned int j, k;

	j = 3; // while loop will start with 5, all muliples of 2 & 3 already marked
	while ((j = next_prime(j,max_prime,bit_arr)) != 0) {
		for (k = (min / j < 5) ? 5 : (min / j); (j * k) <= max; k++) {
			if (ISPRIME(bit_arr, (j*k))) { // if not yet marked as composite
				SET_NOT_PRIME(bit_arr, (j*k)); //all multiples of j <= max_prime
				work_done++;
			}
		}
	}
}


unsigned int next_prime(unsigned int cur_seed, unsigned int max_prime, unsigned int *arr) {
	/* return next prime, or 0 if max is reached*/
	unsigned int i;
	for (i = cur_seed + 2; i <= sqrt(max_prime); i+=2)
		if (ISPRIME(arr,i)) {
			return i;
		}
	return 0;
}


void print_primes(unsigned int max,unsigned int *arr) {
	/* Print primes up to max */
	unsigned int i;
	for (i = 2; i <= max; i++)
		if (ISPRIME(arr,i))
			printf("%u\n", i);
}


void count_primes(unsigned int *arr,unsigned int *count, unsigned int max){
	/* Counts number of primes in bit_arr  */
	int i;
	for (i = 0; i < max; i++)	/* loop corresponds to values 1 thru max */
		if(ISPRIME(arr, i))
			*count = *count + 1;
}

void printbits(unsigned int arr[], int i) {
	/* for debugging */
	int n;
	bool b;
	for (n=(32*i); n<(32*(i+1)); n++)  {
		b = ISCOMPOSITE(arr, n);
		if (b)
			printf("%d", 1);
		else printf("%d", 0);
	}
	printf("\n");
}

void perror_and_exit(char *msg) {
	/* print error message and exit with failure status */
	perror(msg);
	exit(EXIT_FAILURE);
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


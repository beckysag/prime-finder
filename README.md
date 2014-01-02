# Prime Number Finder

C program that finds all prime numbers between 1 and a number (specified as a command-line argument) in the range [1..2^32]. It keeps track of them using a bitmap, and writes all found prime numbers to standard output in ascending order.

Includes two versions:
* ``` primePThread```: a threaded version with implicit memory sharing
* ``` primeMProc```: a version using multiple processes and shared memory



### primePThread
* Uses the pthreads library
* Uses a bit array stored in the heap to store the results of prime number checking

### primeMProc
* forks the appropriate number of child processes
* parent process creates a shared memory object to which the sub-processes will attach
* Uses a bit array stored in a shared memory object to store the results of prime number checking


## Usage
### From the Command Line
primeMProc:

``` make primeMProc```

primePThread:

``` make primePThread```

Both versions of the application allow 3 command line options:
```
–q be quiet (default is to output list of prime numbers) 
–m (size) the maximum size of the prime number for which you check (default is UINT_MAX)
–c (concurrency) number of threads or processes to use (default is 1)
```

### Example usage: 
```
make primePThread
./primePThread –q –m 500000000 –c 5
```

## Validation script

```primeTest.bash``` is a Bash shell script that tests and validates the code:

* Builds both versions of the application, primePThread and primeMProc.
* Validates the output against a file (```primes-500k.txt```) containing first 500,000 prime numbers. 
* Runs both applications, varying the number of threads/sub-processes from 1 to 10, and collects timing numbers.
* Outputs timing numbers to file.


## Testing 
I wrote/tested my program on my quad-core i5 linux machine. I periodically ran tests on os-class to measure timings and speedup from various changes I made.

For both portions of the assignment, I partitioned the list of integers to check for primality into equal segments, one for each thread (or process). Using the Sieve of Eratosthenes, each thread/process was responsible for a fixed portion of the list to mark composite numbers. That way, each thread/process could execute each iteration of their list continously, without waiting for all the other threads/processes to finish their iteration. (Initially I tried a method where the threads took turns iterating the entire list. The main thread was finding the next prime and distributing to the worker-threads in a round robin fashion. Each thread would have to wait for the current thread to finish its loop, and their was time wasted on syncronization, so I decided to partition the list among the threads.)

I started testing my program's efficiency by using a sieve with one, two, and three threads for a large number of primes, and examining where the threads spent the most time. I tried marking all the even numbers before creating the threads, and found that it took a constant, negligable, amount of time up to UMAX_INT (0 to 1 second for UMAX_INT), so I implmented that in my program.

I also tried marking all multiples of 3 so see the tradeoff between speedup and time consumed. That also took a constant amount of time (0 to 1 second for UMAX_INT) so I implemented that as well. For both the intial marking of 2 and 3 as primes, instead of using a loop to set each bit, I looped through integer array elements and set 32 bits at a time. 

Once I finished writing my program, I added a variable for each thread/process to keep track of how many bits it set. When the verbose flag is set at the command line, each thread/process prints out how many bits it set after it completes its work. This allowed me to check whether the threads/proceses were being assigned an even amount of work, which they were. 

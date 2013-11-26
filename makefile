# consulted: http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

# compiler
CC = gcc

# compiler flags
CFLAGS = -g -Wall -std=c99 -O2

# build target executable
TARGET = primes

# libraries to link into executable
LIBS = -lrt -lm -lpthread 

# define the C source files
SRCS = primeMProc.c primeMThread.c

# define the C object files 
OBJS = $(SRCS:.c=.o)


all: primeMProc primePThread

primeMProc: primeMProc.o
	@$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

primePThread: primePThread.o
	@$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	@$(CC) $(CFLAGS) -c $^ 
	
.PHONY: clean
clean:
	@echo "Cleaning files"
	@rm *.o primeMProc primePThread
	@rm -f sagalynr-assignment5.tar.gz
tar:
	tar -jcvf sagalynr-assignment5.tar.bzip *.c makefile prime-list primeTest.bash
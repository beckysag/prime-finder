#!/bin/bash
##### prime-list.txt contains first million primes
##### 1 millionth prime = 15,485,863
##### csv file columns: 1=number of threads 2=max prime 3=time in seconds

TC="/usr/bin/time -f "
CMD='"\n%E elapsed,\n%U user,\n%S system,\n%M memory\n%x status"'
UINTMAX=4294967295
COUNTS=( 500000000 1000000000 1500000000 2000000000 2500000000 3000000000 3500000000 $UINTMAX)
#N=( 1 2 3 4 5 6 7 8 9 10)
N=( 1 2 3 4 5)

# Build primePThread and primeMProc using Makefile
make

# Validate primePThread output for first 1 million primes against test file
./primePThread -m 15485863 > my_primes_t.txt
diff my_primes_t.txt prime-list
echo "primePThread validated for first 1 million primes"
rm my_primes_t.txt

# Validate primeMProc output for first 1 million primes against test file
./primeMProc -m 15485863 > my_primes_p.txt
diff my_primes_p.txt prime-list
echo "primePThread validated for first 1 million primes"
rm my_primes_p.txt

# Collect timing numbers for primePThread:
touch thread_timing.txt
for i in ${N[@]}  # 1 - 10 threads
do	
	# for varying sizes of m
	for m in ${COUNTS[@]}
	do
		echo "Running primePThread with $i threads and $m as a max..."
		
		exec 3>&1 4>&2
		output=$( { time ./primePThread -q -m $m -c $i 1>&3 2>&4; } 2>&1 ) # change some_command
		exec 3>&- 4>&-
		
		#get everything to the right of first "*user "
		user=${output#*user }
		#get everything to the left of the first "s*"
		user=${user%%s*}
		#get everythig to let left of "m*"
		min=${user%%m*}
		#get everything to the right of "*m" and left of ".*"
		sec=${user#*m}
		sec=${sec%%.*}
		#get everything to the right of "*."
		usec=${user#*.}	
		c="$i, $m, $usec" 		
		echo "$c" >> thread_timing.txt	

	done
done


# Collect timing numbers for primePThread:
touch proc_timing.txt
for i in ${N[@]}   # 1 - 10 threads
do	
	# for varying sizes of m
	for m in ${COUNTS[@]}
	do		
		exec 3>&1 4>&2
		output=$( { time ./primeMProc -q -m $m -c $i 1>&3 2>&4; } 2>&1 ) # change some_command
		exec 3>&- 4>&-

		#get everything to the right of first "*user "
		user=${output#*user }
		#get everything to the left of the first "s*"
		user=${user%%s*}
		#get everythig to let left of "m*"
		min=${user%%m*}
		#get everything to the right of "*m" and left of ".*"
		sec=${user#*m}
		sec=${sec%%.*}
		#get everything to the right of "*."
		usec=${user#*.}	
		c="$i, $m, $usec" 		
		echo "$c" >> proc_timing.txt	
	done
done

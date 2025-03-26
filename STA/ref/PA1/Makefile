CC=g++
CFLAGS=-w -std=c++11

run: sta.out
	./sta.out NLDM_lib_max2Inp b19_1.isc

main: soham_5618068_ee5301_PA1.cpp
	$(CC) $(CFLAGS) soham_5618068_ee5301_PA1.cpp -o sta.out

clean: a.out
	rm -f sta.out

test_main: sta.out
	./sta.out NLDM_lib_max2Inp b19_1.isc

.PHONY: test_main

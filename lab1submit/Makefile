CC = g++

CC_FLAGS = -std=c++17 -Wall -O2 -pthread

pfxsum: PrefixScanMain.cc PrefixScan.hpp
	$(CC) $(CC_FLAGS) -o pfxsum PrefixScanMain.cc PrefixScan.hpp -lboost_program_options

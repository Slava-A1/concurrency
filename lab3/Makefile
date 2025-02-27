CC = g++

CC_FLAGS = -std=c++17 -Wall -O2 -pthread -fsanitize=address

kmeans: kmeans.cpp
	$(CC) $(CC_FLAGS) -o kmeans kmeans.cpp -lboost_program_options

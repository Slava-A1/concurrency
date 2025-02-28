CC = g++

CC_FLAGS = -std=c++17 -Wall -O2 -pthread -fsanitize=address

kmeans: kmeans.cpp kmeans.h kmeans_arg_parse.h
	$(CC) $(CC_FLAGS) -o kmeans kmeans.cpp kmeans.h

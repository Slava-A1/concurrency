nvcc -o kmeans_kernel.o -c kmeans_kernel.cu -arch=sm_75
g++ -o kmeans.o -c kmeans.cpp
nvcc kmeans.o kmeans_kernel.o -o kmeans


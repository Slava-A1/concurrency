#ifndef KMEANS_H
#define KMEANS_H

#include <vector>
#include <string>


extern int num_cluster, dims, max_num_iter, seed, inputThreads;
extern std::string inputfilename;
extern float threshold;
extern bool printCentroids, useGPU, useSharedMem, useKmeanspp;

void cudaBasicTest(const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& res);

#endif //KMEANS_H

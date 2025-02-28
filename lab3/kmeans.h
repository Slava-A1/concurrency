#include <vector>
#include <string>


extern int num_cluster, dims, max_num_iter, seed, inputThreads;
extern std::string inputfilename;
extern float threshold;
extern bool printCentroidIds, useGPU, useSharedMem, useKmeanspp;

std::vector<std::vector<float>> getRandomCentroids(const std::vector<std::vector<float>>& data);

std::vector<std::vector<float>> genCentroidPar(const std::vector<std::vector<float>>& data, std::vector<int> centroidIds);

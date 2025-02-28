#include <cuda.h>
#include <vector>
#include <cmath>
#include "kmeans.h"
#include <iostream>

#define CHECK_CUDA1(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA Error: " << cudaGetErrorString(err) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        } \
    } while (0)

//Credit to Rishi and Oakridge National Labs (which is where I think GPT pulled his macro from)
#define CHECK_CUDA(call) ({ \
    cudaError_t err = call; \
    if(err != cudaSuccess){ \
        std::cerr << "CUDA Error: " << cudaGetErrorString(err) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
    } \
})

__global__ void addVectors(float* a, float* b, float* res, int sz){
    int myIndex = (blockIdx.x * blockDim.x) + threadIdx.x;

    if(myIndex < sz){
        res[myIndex] = a[myIndex] + b[myIndex];
    }
} 

void cudaBasicTest(const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& out){
    const int sz = a.size();
    float *daPtr, *dbPtr, *dresPtr;
    size_t allocSize = sizeof(float) * sz;
    CHECK_CUDA(cudaMalloc(&daPtr, allocSize));
    CHECK_CUDA(cudaMalloc(&dbPtr, allocSize));
    CHECK_CUDA(cudaMalloc(&dresPtr, allocSize));
     
    CHECK_CUDA(cudaMemcpy(daPtr, a.data(), allocSize, cudaMemcpyHostToDevice));
    CHECK_CUDA(cudaMemcpy(dbPtr, b.data(), allocSize, cudaMemcpyHostToDevice));

    int threadsPerBlock = 256; //why not
    int nBlocks = (int)std::ceil(((float)sz) / threadsPerBlock);

    addVectors<<<nBlocks, threadsPerBlock>>>(daPtr, dbPtr, dresPtr, sz);
    cudaError_t lastErr = cudaGetLastError();
    if(lastErr != cudaSuccess){
        std::cerr << "CUDA Error: " << cudaGetErrorString(lastErr) << std::endl;
    }
    
    CHECK_CUDA(cudaDeviceSynchronize());

    CHECK_CUDA(cudaMemcpy(out.data(), dresPtr, allocSize, cudaMemcpyDeviceToHost));

    CHECK_CUDA(cudaFree(daPtr));
    CHECK_CUDA(cudaFree(dbPtr));
    CHECK_CUDA(cudaFree(dresPtr));
}

//number of get distance ops is num_points * num_clusters
//Idea: one block for each get distance, with one thread per distance operation (a - b)^2

//Idea: use critical section to update max in parallel?
//Better: store distances in array, then have another kernel find max for each point
//Technically a parallel reduction would be best but not sure if there will be enough time to write that

/*
//Returns the centroids and populates the passed in vector of integers with the centroid id for each point
std::vector<std::vector<float>> genCentroidPar(const std::vector<std::vector<float>>& data, std::vector<int> centroidIds){
    //Set up
    const int nPoints = data.size();
    std::vector<std::vector<float>> centroids = getRandomCentroids(data);
    //Stores number of points associated with each centroid
    std::vector<int> numAssocPoints(num_cluster);
    int currIter = 1;
    bool done = false;
    float currConv = 1.0;

    //Making everything accessible to CUDA
    float** centroidsPtr = new float*[num_cluster];
    for(int i = 0; i < num_cluster; ++i){
        //extracting pointers from the vectors
        centroidsPtr[i] = centroids[i].data();
    }

    while(!done){
        float conv = 0.0;
        
        std::fill(numAssocPoints.begin(), numAssocPoints.end(), 0);
        //Stores sums of values for each centroid, will be divided later
        std::vector<std::vector<float>> nextCentroids(num_cluster, std::vector<float>(dims, 0.0));
        
        //Finding closest centroid to each point
        for(int i = 0; i < nPoints; ++i){
            std::pair<int, float> closePair = findClosestCentroid(data[i], centroids);
            centroidIds[i] = closePair.first;
            conv += closePair.second;

            //One more point associated with the selected centroid
            numAssocPoints[closePair.first] += 1;
            //add in values for later division
            for(int j = 0; j < dims; ++j){
                nextCentroids[closePair.first][j] += data[i][j];
            }
        }
        
        //updating centroids
        for(int i = 0; i < num_cluster; ++i){
            updateCentroid(nextCentroids[i], numAssocPoints[i]);
        }
        
        centroids = std::move(nextCentroids);
        
        float improvementAmt = std::abs(currConv - conv) / currConv;
        currConv = conv;

        done = (currIter == max_num_iter) || (improvementAmt < threshold);
        ++currIter;
    }
    
    delete centroidsPtr;

    //Returns final centroids in the case that they need to be printed
    return centroids;
}

*/

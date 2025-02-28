#include <iostream>
#include <iomanip>
#include <limits>
#include <string>
#include <chrono>
#include <sstream>
#include <fstream>
#include <semaphore.h>
#include <pthread.h>
#include <atomic>
#include <unistd.h>
#include <cmath>
#include <stdio.h>
#include "kmeans.h"

int num_cluster, dims, max_num_iter, seed, inputThreads;
std::string inputfilename;
float threshold;
bool printCentroids, useGPU, useSharedMem, useKmeanspp;

//TODO: verify that fix to parallel input parsing works as intended

struct ThreadArgs {
    std::atomic_ulong* procInd;
    std::vector<std::stringstream>* streams;
    std::vector<std::vector<float>>* data;
    sem_t* semPtr;
};

void processInputLine(std::stringstream& ss, std::vector<float>& res){
    int buf;
    ss >> buf; //get rid of first int in each line
    for(int i = 0; i < dims; ++i){
        ss >> res[i];
    }
}

void* inputDataWork(void* args){
    ThreadArgs* ta = (ThreadArgs*)args;
    const long nLines = ta->streams->size();
    //Need sem to protect atomic otherwise some threads access streams
    //that are not ready for processing
    sem_wait(ta->semPtr);
    long myInd = ta->procInd->fetch_add(1, std::memory_order_seq_cst);
    while(myInd < nLines){
        processInputLine((*(ta->streams))[myInd], (*(ta->data))[myInd]);
        sem_wait(ta->semPtr);
        myInd = ta->procInd->fetch_add(1, std::memory_order_seq_cst);
    }
   
    return nullptr;
}

void processDataSeq(std::vector<std::vector<float>>& inputData, std::ifstream& infile){
    const int nLines = inputData.size();
    std::string buf;
    std::getline(infile, buf); //clear newline

    for(int i = 0; i < nLines; ++i){
        std::getline(infile, buf);
        std::stringstream ss(std::move(buf));
        processInputLine(ss, inputData[i]);
    }

}

void processDataPar(std::vector<std::vector<float>>& inputData, std::ifstream& infile){
    const int nLines = inputData.size();
    std::vector<std::stringstream> dataStreams(nLines);
    std::string buf;
    std::getline(infile, buf); //clear newline
    sem_t sem;
    sem_init(&sem, 0, 0);
    std::atomic_ulong procInd(0);

    ThreadArgs args;
    args.procInd = &procInd;
    args.streams = &dataStreams;
    args.data = &inputData;
    args.semPtr = &sem;

    std::vector<pthread_t> threads(inputThreads);

    for(int i = 0; i < inputThreads; ++i){
       pthread_create(&threads[i], NULL, inputDataWork, &args);
    }

    for(int i = 0; i < nLines; ++i){
        std::getline(infile, buf);
        dataStreams[i] = std::stringstream(std::move(buf));
        sem_post(&sem);
    }

    //Send through extra sem increments so all threads can exit their work function
    for(int i = 0; i < inputThreads; ++i){
        sem_post(&sem);
    }

    for(int i = 0; i < inputThreads; ++i){
        int errorCode = pthread_join(threads[i], NULL);
        if(errorCode != 0) {
            std::cout << "Error in pthread " << i << " exit code: " << errorCode << '\n';
        }
    }

    sem_destroy(&sem);
}

inline float rand_float(){
    return rand() / static_cast<float>((long long) RAND_MAX + 1);
}

std::vector<std::vector<float>> getRandomCentroids(const std::vector<std::vector<float>>& data){
    std::vector<std::vector<float>> centroids(num_cluster);
    
    for(int i = 0; i < num_cluster; ++i){
        int index = (int)(rand_float() * data.size());
        centroids[i] = data[index];
    }

    return centroids;
}

float getDistance(const std::vector<float>& a, const std::vector<float>& b){ 
    const int dim = a.size();
    float distance = 0.0;
    
    for(int i = 0; i < dim; ++i){
        distance += (a[i] - b[i]) * (a[i] - b[i]);
    }

    return std::sqrt(distance);
}

void updateCentroid(std::vector<float>& centroid, const int nPoints){
    
    if(nPoints == 0){
        return;
    }

    const int n = centroid.size();
    for(int i = 0; i < n; ++i){
        centroid[i] /= nPoints;
    }
}

std::pair<int, float> findClosestCentroid(const std::vector<float>& point, const std::vector<std::vector<float>>& centroids){
    const int nCentroids = centroids.size();
    float closestDist = std::numeric_limits<float>::infinity();
    int closestInd = 0;
    
    for(int i = 0; i < nCentroids; ++i){
        float dist = getDistance(point, centroids[i]);
        if(dist < closestDist){
            closestDist = dist;
            closestInd = i;
        }
    }

    return {closestInd, closestDist};
}

//Returns the centroids and populates the passed in vector of integers with the centroid id for each point
std::vector<std::vector<float>> genCentroidSeq(const std::vector<std::vector<float>>& data, std::vector<int> centroidIds){
    const int nPoints = data.size();
    std::vector<std::vector<float>> centroids = getRandomCentroids(data);
    //Stores number of points associated with each centroid
    std::vector<int> numAssocPoints(num_cluster);
    int currIter = 1;
    bool done = false;
    //float currConv = 1.0;
    
    while(!done){
        //float conv = 0.0;
        
        std::fill(numAssocPoints.begin(), numAssocPoints.end(), 0);
        //Stores sums of values for each centroid, will be divided later
        std::vector<std::vector<float>> nextCentroids(num_cluster, std::vector<float>(dims, 0.0));
        
        //Finding closest centroid to each point
        for(int i = 0; i < nPoints; ++i){
            std::pair<int, float> closePair = findClosestCentroid(data[i], centroids);
            centroidIds[i] = closePair.first;
            //conv += closePair.second;

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
        
        //get max dist between any old + new centroid
        float improvementAmt = 0.0;
        for(int i = 0; i < num_cluster; ++i){
            improvementAmt = std::max(improvementAmt, getDistance(nextCentroids[i], centroids[i]));
        }

        centroids = std::move(nextCentroids);

        //old, broken
        //float improvementAmt = std::abs(currConv - conv) / currConv;
        //currConv = conv;

        done = (currIter == max_num_iter) || (improvementAmt < threshold);
        ++currIter;
    }
    
    //Returns final centroids in the case that they need to be printed
    return centroids;
}


int main(int argc, char** argv){
    std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();

/*
 *
    -k num_cluster: an integer specifying the number of clusters
    -d dims: an integer specifying the dimension of the points
    -i inputfilename: a string specifying the input filename
    -m max_num_iter: an integer specifying the maximum number of iterations
    -t threshold: a double specifying the threshold for convergence test.
    -c: a flag to control the output of your program. If -c is specified, your program should output the centroids of all clusters. If -c is not specified, your program should output the labels of all points. See details below.
    -s seed: an integer specifying the seed for rand(). This is used by the autograder to simplify the correctness checking process. See details below.
    -g: a flag to enable the GPU implementation
    -f: a flag to enable the shared-memory GPU implementation
    -p: a flag to enable the Kmeans++ implementation
    -w: number of pthreads to use for input parsing

*/

    num_cluster = -1;
    dims = -1;
    
    max_num_iter = -1;
    threshold = -1.0;
    printCentroids = false;
    seed = -1;
    useGPU = false;
    useSharedMem = false;
    useKmeanspp = false;
    inputThreads = 8;


    //TODO: do command line argument parsing here with getopt
    char c;
    while((c = getopt(argc, argv, "pfgcw:s:t:m:i:d:k:")) != -1){
        switch(c){
            case 'k':
                num_cluster = atoi(optarg);
                break;
            case 'd':
                dims = atoi(optarg);
                break;
            case 'i':
                inputfilename = optarg; //init string with passed in char*
                break;
            case 'm':
                max_num_iter = atoi(optarg);
                break;
            case 't':
                threshold = atof(optarg);
                break;
            case 'c':
                printCentroids = true;
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'g':
                useGPU = true;
                break;
            case 'f':
                useSharedMem = true;
                break;
            case 'p':
                useKmeanspp = true;
                break;
            case 'w':
                inputThreads = atoi(optarg);
                break;
            case '?':
                std::cerr << "Unknown or missing argument!\n";
                return 1;
            default:
                return 1;
        }
    }

    if(num_cluster == -1){
        std::cerr << "Missing number of clusters!\n";
        return 1;
    }
    if(dims == -1){
        std::cerr << "Missing point dimensions!\n";
        return 1;
    }
    if(inputfilename.size() == 0){
        std::cerr << "Missing input file!\n";
        return 1;
    }
    if(max_num_iter == -1){
        std::cerr << "Missing number of iterations\n";
        return 1;
    }
    if(threshold == -1.0){
        std::cerr << "Missing threshold!\n";
        return 1;
    }
    if(seed == -1.0){
        std::cerr << "Missing seed!\n";
        return 1;
    }

    /*
    std::cout << "num_cluster: " << num_cluster << '\n';
    std::cout << "dims: " << dims << '\n';
    std::cout << "inputfilename: " << inputfilename << '\n';
    std::cout << "max_num_iter: " << max_num_iter << '\n';
    std::cout << "threshold: " << threshold << '\n';
    std::cout << "seed: " << seed << '\n';
    std::cout << "inputThreads: " << inputThreads << '\n';
    std::cout << "printCentroids: " << printCentroids << '\n';
    std::cout << "useGPU: " << useGPU << '\n';
    std::cout << "useSharedMem: " << useSharedMem << '\n';
    std::cout << "useKmeanspp: " << useKmeanspp << '\n';
    */
    srand(seed);

    std::ifstream infile(inputfilename);
    long nLines;
    infile >> nLines;

    std::vector<std::vector<float>> inputData(nLines, std::vector<float>(dims));

    if(inputThreads == 0){
        processDataSeq(inputData, infile);
    } else {
        processDataPar(inputData, infile);
    }

    std::vector<int> centroidIds(nLines);
    std::vector<std::vector<float>> centroids;
    
    if(useSharedMem){
        //TODO: implement this
        std::cerr << "Need to implement shared mem approach!\n";
        return 1;
    } else if(useGPU){

        ///*         
        std::vector<float> testA(2048, 1.0);
        std::vector<float> testB(2048, 5.0);
        std::vector<float> output(2048);

        cudaBasicTest(testA, testB, output);
        
        for(int i = 0; i < 2048; ++i){
            
            if(output[i] != 6.0){
                std::cout << "Error at index " << i << " expected 6.0 got " << output[i] << '\n';
                return 1;
            }

        }
        
        std::cout << "Basic CUDA test success!\n";
        return 0;
        //*/

    } else if(useKmeanspp) {
        //TODO: implement this
        std::cerr << "Need to implement kmeans++ approach!\n";
        return 1;
    } else { //Sequential approach on CPU
       centroids = genCentroidSeq(inputData, centroidIds);
    }

    /* 
    //temp sorting to help with debug
    std::sort(centroids.begin(), centroids.end(), [](const std::vector<float>& first, const std::vector<float>& second){
        return first[0] < second[0];
    });
    */

    if(printCentroids){

        for(int i = 0; i < num_cluster; ++i){
            std::cout << i << ' ';
            for(int j = 0; j < dims; ++j){
                std::cout << centroids[i][j] << ' ';
            }
            std::cout << '\n';
        }
    } else {
        std::cout << "clusters: " << std::setprecision(5);
        for(int i = 0; i < nLines; ++i){
            std::cout << ' ' << centroidIds[i];
        }
        
        std::cout << '\n';
    }

    /*
    for(int i = 0; i < nLines; ++i){
        for(int j = 0; j < dims; ++j){
            std::cout << inputData[i][j] << ' ';
        }
        std::cout << '\n';
    }
    */

    std::chrono::high_resolution_clock::time_point endPoint = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = endPoint - startPoint;
    double execTime = time.count();
    std::cout << std::setprecision(12) << execTime << '\n';
}

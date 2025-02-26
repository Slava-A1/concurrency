#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>
#include <string>
#include <chrono>
#include <sstream>
#include <fstream>
#include <semaphore.h>
#include <pthread.h>
#include <atomic>

int num_cluster, dims, max_num_iter, seed, inputThreads;
std::string inputfilename;
double threshold;
bool compAllClusters, useGPU, useSharedMem, useKmeanspp;

//TODO: debug race condition in parallel input parsing

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
        if(ss.fail()){
            std::cerr << "Stringstream failed! ";
        }
        ss >> res[i];
    }
}

void* inputDataWork(void* args){
    ThreadArgs* ta = (ThreadArgs*)args;
    long myInd = ta->procInd->fetch_add(1, std::memory_order_seq_cst);
    const long nLines = ta->streams->size();
    while(myInd < nLines){
        sem_wait(ta->semPtr);
        processInputLine((*(ta->streams))[myInd], (*(ta->data))[myInd]);
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

void processDataPar(std::vector<std::vector<float>>& inputData, std::ifstream& infile, int workers){
    const int nLines = inputData.size();
    std::vector<std::stringstream> dataStreams(nLines);
    std::string buf;
    std::getline(infile, buf); //clear newline
    sem_t sem;
    sem_init(&sem, 0, 0);
    std::atomic_ulong procInd = 0;

    ThreadArgs args;
    args.procInd = &procInd;
    args.streams = &dataStreams;
    args.data = &inputData;
    args.semPtr = &sem;

    std::vector<pthread_t> threads(workers);

    for(int i = 0; i < workers; ++i){
       pthread_create(&threads[i], NULL, inputDataWork, &args);
    }

    for(int i = 0; i < nLines; ++i){
        std::getline(infile, buf);
        dataStreams[i] = std::stringstream(std::move(buf));
        sem_post(&sem);
    }

    for(int i = 0; i < workers; ++i){
        int errorCode = pthread_join(threads[i], NULL);
        if(errorCode != 0) {
            std::cout << "Error in pthread " << i << " exit code: " << errorCode << '\n';
        }
    }

    sem_destroy(&sem);
}

namespace po = boost::program_options;

int main(int argc, char** argv){
    std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();

    try{

        po::command_line_style::style_t style = po::command_line_style::style_t(
            po::command_line_style::unix_style | 
            po::command_line_style::allow_long_disguise | 
            po::command_line_style::long_allow_adjacent |
            po::command_line_style::short_allow_adjacent
        );

        po::options_description desc("Program options: ");
        desc.add_options()
            ("help", "List option descriptions")
            ("k", po::value<int>(&num_cluster)->default_value(-1), "Integer specifying the number of clusters to use")
            ("d", po::value<int>(&dims)->default_value(-1), "Integer specifying the dimensions of the points")
            ("i", po::value<std::string>(&inputfilename), "String specifying the input file name")
            ("m", po::value<int>(&max_num_iter)->default_value(-1), "Integer specifying the maximum number of iterations")
            ("t", po::value<double>(&threshold)->default_value(-1), "Double specifying the threshold for convergence test")
            ("c", "If present, program will output the centroids of all clusters, otherwise it will output the labels of all points")
            ("s", po::value<int>(&seed)->default_value(-1), "Integer specifying the seed for rand()")
            ("g", "Enables the GPU implementation")
            ("f", "Enables the shared memory GPU implementation")
            ("p", "Enables the Kmeans++ implementation")
            ("iw", po::value<int>(&inputThreads)->default_value(0), "Integer specifying the number of threads to spawn for input parsing")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc, style), vm);
        po::notify(vm);

        if(vm.count("help")){
            std::cout << desc << '\n';
            return 0;
        }

        compAllClusters = vm.count("c");
        useGPU = vm.count("g");
        useSharedMem = vm.count("f");
        useKmeanspp = vm.count("p");

    } catch(std::exception& e){
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    std::ifstream infile(inputfilename);
    long nLines;
    infile >> nLines;

    std::vector<std::vector<float>> inputData(nLines, std::vector<float>(dims));

    if(inputThreads == 0){
        processDataSeq(inputData, infile);
    } else {
        processDataPar(inputData, infile, inputThreads);
    }

    for(int i = 0; i < nLines; ++i){
        for(int j = 0; j < dims; ++j){
            std::cout << inputData[i][j] << ' ';
        }
        std::cout << '\n';
    }

    std::chrono::high_resolution_clock::time_point endPoint = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = endPoint - startPoint;
    double execTime = time.count();
    std::cout << std::setprecision(12) << execTime << '\n';
}

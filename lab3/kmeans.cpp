#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

namespace po = boost::program_options;

int main(int argc, char** argv){
    std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();
    int num_cluster, dims, max_num_iter, seed;
    std::string inputfilename;
    double threshold;
    bool compAllClusters, useGPU, useSharedMem, useKmeanspp;

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
            ("d", po::value<int>(&dims)->default_value(-1), "Integer specifying the number of clusters")
            ("i", po::value<std::string>(&inputfilename), "String specifying the input file name")
            ("m", po::value<int>(&max_num_iter)->default_value(-1), "Integer specifying the maximum number of iterations")
            ("t", po::value<double>(&threshold)->default_value(-1), "Double specifying the threshold for convergence test")
            ("c", "If present, program will output the centroids of all clusters, otherwise it will output the labels of all points")
            ("s", po::value<int>(&seed)->default_value(-1), "Integer specifying the seed for rand()")
            ("g", "Enables the GPU implementation")
            ("f", "Enables the shared memory GPU implementation")
            ("p", "Enables the Kmeans++ implementation")
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

    //TODO: begin implementation here

    std::chrono::high_resolution_clock::time_point endPoint = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = endPoint - startPoint;
    double execTime = time.count();
    std::cout << std::setprecision(12) << execTime << '\n';
}

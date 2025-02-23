#include <boost/program_options.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <iomanip>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include "PrefixScan.hpp"

//TODO: add parallel io

int nThreads = -1;
bool myBarrier = false;
bool doTwoLevel = false;

namespace po = boost::program_options;

int main(int argc, char** argv){
    std::chrono::high_resolution_clock::time_point startPoint = std::chrono::high_resolution_clock::now();
    nThreads = -1;
    myBarrier = false;
    std::string iPath, oPath;

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
            ("n", po::value<int>(&nThreads)->default_value(-1), "[REQUIRED] Number of threads to use. 0 indicates sequential")
            ("i", po::value<std::string>(&iPath), "[REQUIRED] Path to input file")
            ("o", po::value<std::string>(&oPath), "[REQUIRED] Path to output file")
            ("s", "[OPTIONAL] Indicates that my barrier implementation should be used")
            ("t", "[OPTIONAL] Indicates that the two level algorihtm should be used")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc, style), vm);
        po::notify(vm);

        if(vm.count("help")){
            std::cout << desc << '\n';
            return 0;
        }

        if(vm.count("n")){
            if(nThreads < 0){
                std::cerr << "Error: number of threads argument is negative!\n";
                return 1;
            }
        }
        else{
            std::cerr << "Error: number of threads argument is missing!\n";
            return 1;
        }

        if(!vm.count("i")){
            std::cerr << "Error: input path argument is missing!\n";
            return 1;
        }

        if(!vm.count("o")){
            std::cerr << "Error: output path argument is missing\n";
        }

        if(vm.count("s")){
            myBarrier = true;
        }

        if(vm.count("t")){
           doTwoLevel = true; 
        }

    }
    catch(std::exception& e){
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    
    std::ifstream inFile(iPath);
    std::ofstream outFile(oPath);

    if(!inFile.is_open()){
        std::cerr << "Error: failed to open input file!\n";
        return 1;
    }

    int vecLen, numElems;
    inFile >> vecLen >> numElems;

    if(vecLen == 0){
        std::vector<int> vals(numElems);
        for(int i = 0; i < numElems; ++i){
            inFile >> vals[i];
        }
        
        //std::vector<int> vals(inputData);
        launch_threads(vals, &scan_op<int, ElemAdder<int>>);

        /*
        std::cout << "Collected values:\n";
        for(int i = 0; i < vals.size(); ++i){
            std::cout << vals[i] << ' ';
        }
        std::cout << "\ndone!\n";
        */
        
        outFile << vals[numElems - 1] << '\n';
    }
    else{
        std::vector<std::vector<double>> valsVecs(numElems, std::vector<double>(vecLen));
        std::string buf;
        std::getline(inFile, buf); //need to collect newline after numElems so it doesn't mess with collection
        for(int i = 0; i < numElems; ++i){
            std::getline(inFile, buf);
            std::stringstream ss(buf);

            for(int j = 0; j < vecLen; ++j){
                ss >> valsVecs[i][j];
                if(ss.peek() == ','){
                    ss.ignore();
                }
            }

        }
        
        //std::vector<std::vector<double>> valsVecs(inputVecs);
        launch_threads(valsVecs, &scan_op<std::vector<double>, MutVectorAdder<double>>);
        
        /*
        std::cout << "Collected values:\n";
        for(int i = 0; i < valsVecs.size(); ++i){
            
            for(int j = 0; j < valsVecs[i].size(); ++j){
                std::cout << valsVecs[i][j] << ' ';
            }

            std::cout << '\n';

        }
        std::cout << "done!\n";
        */
        
        
        for(int i = 0; i < vecLen; ++i){
            outFile << std::setprecision(12) << valsVecs[numElems - 1][i] << ' ';
        }
        outFile << '\n';
        
    }
    

    std::chrono::high_resolution_clock::time_point endPoint = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = endPoint - startPoint;
    double execTime = time.count();
    std::cout << std::setprecision(12)  << execTime << '\n';
}


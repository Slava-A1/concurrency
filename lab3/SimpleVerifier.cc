#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstdio>
//TODO: multithread this later


void processInputLine(std::stringstream& ss, std::vector<float>& res){
    int buf;
    ss >> buf; //get rid of first int in each line
    float val;
    while(ss >> val){
       res.push_back(val); 
    }
}

void processDataSeq(std::vector<std::vector<float>>& inputData, std::istream& infile){
    
    std::string buf;

    while(std::getline(infile, buf)){
        if(buf.size() == 0){
            //hit newline at end of file
            break;
        }
        std::stringstream ss(std::move(buf));
        inputData.push_back(std::vector<float>());
        processInputLine(ss, inputData.back());
    }

}

//Command strings will need to pass in their char* ptr into this
std::string exec(const std::string& cmd){
    char buffer[512];
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if(!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    try {
        while(fgets(buffer, sizeof(buffer), pipe) != NULL){
            //Fgets adds a null terminator so there shouldn't be issues with extra chars being added to the output string
            output += buffer;
        }
    }
    catch (...){
        pclose(pipe);
        throw;
    }

    pclose(pipe);
    return output;
}

void printErrorInfo(const std::string& failCmd, const std::vector<std::vector<float>>& failData){
    std::cout << "failed command: " << failCmd << '\n';

    for(int i = 0; i < failData.size(); ++i){
        std::cout << i << ' ';
        for(int j = 0; j < failData[0].size(); ++j){
            std::cout << failData[i][j] << ' ';
        }
        std::cout << '\n';
    }

}

//TODO: increase this back to 50 later
constexpr int NUM_ITERS = 1;

int main(int argc, char** argv){

    if(argc != 3) {
        std::cerr << "Error: incorrect number of command line arguments!";
        return -1;
    }

    //Index 1 should be the commands file
    //Index 2 should be the solution file
    
    std::ifstream cmdsFile(argv[1]);
    std::ifstream solFile(argv[2]);
    std::string buf;
    
    std::vector<std::string> cmds;
    
    while(std::getline(cmdsFile, buf)){
        cmds.push_back(buf);
    }

    std::vector<std::vector<float>> solData;

    processDataSeq(solData, solFile);
    
    const int nClusters = solData.size();
    const int nDims = solData[0].size();

    std::sort(solData.begin(), solData.end(), [](const std::vector<float>& first, const std::vector<float>& second){
        return first[0] < second[0];
    });
    
    /*
    for(int i = 0; i < solData.size(); ++i){
        std::cout << i << ' ';
        for(int j = 0; j < solData[0].size(); ++j){
            std::cout << solData[i][j] << ' ';
        }
        std::cout << '\n';
    }
    */
    

    std::vector<int> numGood(cmds.size(), 0);

    //Need to reset these at end of loop
    std::string myStr;
    
    for(int k = 0; k < cmds.size(); ++k){
        for(int j = 0; j < NUM_ITERS; ++j){
            std::string cmdOut = exec(cmds[k]);
            std::stringstream ss(std::move(cmdOut));
            std::vector<std::vector<float>> outData;
            processDataSeq(outData, ss);
            
            //Need to have -1 to account for timing line
            if(nClusters != (outData.size() - 1)){
                std::cout << "Number of clusters differs!\n";
                printErrorInfo(cmds[k], outData);
                return 1;
            }

            if(nDims != outData[0].size()){
                std::cout << "Dimension differs!\n";
                printErrorInfo(cmds[k], outData);
                return 1;
            }
            
            std::sort(outData.begin(), outData.end(), [](const std::vector<float>& first, const std::vector<float>& second){
                return first[0] < second[0];
            });
            
            bool allGood = true;
            //Verifying all of the clusters
            for(int cluster = 0; cluster < nClusters; ++cluster){
                bool allSame = true;
                for(int dim = 0; dim < nDims; ++dim){
                    if(std::abs(outData[cluster][dim] - solData[cluster][dim]) > 0.0001){
                        allSame = false;
                        break;
                    }
                }

                if(!allSame){
                    allGood = false;
                    std::cout << "Different centroid in iteration " << j << "\nSol centroid: ";

                    for(int dim = 0; dim < nDims; ++dim){
                        std::cout << solData[cluster][dim] << ' ';
                    }

                    std::cout << "\nOut centroid: ";
                    for(int dim = 0; dim < nDims; ++dim){
                        std::cout << outData[cluster][dim] << ' ';
                    }
                    std::cout << '\n';

                }
            }

        
        }
    }
    
    std::ofstream outFile("resultsSimple.txt");

    for(int i = 0; i < cmds.size(); ++i){
        outFile << cmds[i] << '\n' << numGood[i] << '/' << NUM_ITERS << '\n';
    }
    
}

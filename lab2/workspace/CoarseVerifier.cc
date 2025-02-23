#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstdio>

//Command strings will need to pass in their char* ptr into this
std::string exec(const std::string& cmd){
    char buffer[256];
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


constexpr int NUM_ITERS = 50;

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

    std::string idealOutput;

    while(std::getline(solFile, buf)){
        idealOutput += buf;
        idealOutput += '\n';
    }

    std::vector<int> numGood(cmds.size(), 0);

    //Need to reset these at end of loop
    std::string myStr, cmdOut;;
    std::map<long long, std::vector<int>> hashToVals;
    std::vector<std::vector<int>> groups(33);
    
    for(int k = 0; k < cmds.size(); ++k){

        for(int j = 0; j < NUM_ITERS; ++j){
            cmdOut = exec(cmds[k]);
            std::stringstream ss(cmdOut);
            ss.ignore(cmdOut.size(), '\n');
            ss.ignore(cmdOut.size(), '\n');
            ss.ignore(cmdOut.size(), '\n');

            for(int i = 0; i < 50; ++i){
                long long hash;
                int val;
                ss >> hash;
                ss.ignore(1, ':');
                //ss.get();
                while(ss >> val){
                    hashToVals[hash].push_back(val);
                    ss.ignore(1, ' ');
                    if(ss.peek() == '\n'){
                        break;
                    }
                }
                //std::sort(hashToVals[hash].begin(), hashToVals[hash].end());
            }
            
            ss.ignore(1, '\n');
            //Skip other timing line
            ss.ignore(cmdOut.size(), '\n');
            ss.get();
            
            for(int i = 0; i < 33; ++i){
                
                ss.ignore(cmdOut.size(), ':');
                ss.get();
                int val;
                while(ss >> val){
                    groups[i].push_back(val);
                }
                ss.clear();
                //std::sort(groups[i].begin(), groups[i].end());
            }
            
            
            std::sort(groups.begin(), groups.end(), [](const std::vector<int>& a, const std::vector<int>& b){
                return a[0] < b[0];
            });
            
            for(const std::pair<long long, std::vector<int>>& entry : hashToVals) {
                myStr += std::to_string(entry.first);
                myStr += ": ";
                for(const int& val : entry.second){
                    myStr += std::to_string(val);
                    myStr += ' ';
                }
                myStr += '\n';
            }

            for(int i = 0; i < 33; ++i){
                myStr += "group ";
                myStr += std::to_string(i);
                myStr += ": ";
                for(int j = 0; j < groups[i].size(); ++j){
                    myStr += std::to_string(groups[i][j]);
                    myStr += ' ';
                }
                myStr += '\n';
            }

            if(myStr == idealOutput){
                //std::cout << "The two strings are identical!\n";
                numGood[k] += 1;
            }
            else {
                
                /*
                for(int ii = 0; ii < myStr.size(); ++ii){
                    if(myStr[ii] != idealOutput[ii]){
                        std::cout << "index: " << ii << " char: " << myStr[ii] << ' ' << idealOutput[ii] << '\n';
                    }
                }
                */
                std::cout << "K " << k << " Command: " << cmds[k] << '\n';
                std::cout << myStr << '\n' << '\n' << idealOutput << '\n';

                std::cout << myStr.size() << ' ' << idealOutput.size() << '\n';

                return 1;
            }
            //Cleanup
            for(int i = 0; i < 33; ++i){
                groups[i].clear();
            }
            hashToVals.clear();
            myStr.clear();
        
        }
    }
    
    std::ofstream outFile("resultsCoarse.txt");

    for(int i = 0; i < cmds.size(); ++i){
        outFile << cmds[i] << '\n' << numGood[i] << '/' << NUM_ITERS << '\n';
    }
    
}

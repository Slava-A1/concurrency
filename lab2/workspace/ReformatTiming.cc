#include <iostream>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    
    //Input argument should be the name of the test case
    std::string infileName(argv[1]);
    infileName += ".txt";
    std::string outfileName(argv[1]);
    outfileName += "Formatted.txt";
    std::ifstream infile(infileName);
    std::string buf;
    std::vector<std::string> outlines;
    while(std::getline(infile, buf)){
        std::string currLine(std::move(buf));

        for(int i = 0; i < 10; ++i){
            currLine += ',';
            std::getline(infile, buf);
            int colInd = buf.find(':');
            //grab the time, skipping : and space
            currLine += buf.substr(colInd + 2);
        }
        
        outlines.push_back(std::move(currLine));
    }
   
    std::ofstream outfile(outfileName);

    for(int i = 0; i < outlines.size(); ++i){
        outfile << outlines[i] << '\n';
    }

}

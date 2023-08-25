#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
#include <getopt.h>
#include <fstream>
#include <map>
#include <queue>
#include "../utils/Node.h"
#include "../utils/utimer.cpp"

using namespace std;

#define ASCII_MAX 256
// OPT LIST
/*
 * h HELP
 * i input file path
 * p encoded file path
 * o decoded file path
 */
#define OPT_LIST "hi:p:"

void readFrequencies(vector<uintmax_t>* ascii, ifstream &myFile, string* file, uintmax_t size);
void writeToFile(const string& bits, const string& encodedFile);
void createOutput(string* file, map<uintmax_t, string> myMap);

int main(int argc, char* argv[])
{

    char option;
    vector<uintmax_t> ascii(ASCII_MAX, 0);
    string inputFile, encodedFile, decodedFile;

    inputFile = "./data/TestFiles/";
    encodedFile = "./data/EncodedFiles/Sequential/";

    while((option = (char)getopt(argc, argv, OPT_LIST)) != -1){
        switch (option) {
            case 'i':
                inputFile += optarg;
                break;
            case 'p':
                encodedFile += optarg;
                break;
            default:
                cout << "Invalid option" << endl;
                return 1;
        }
    }


    map<uintmax_t, string> myMap;
    ifstream myFile (inputFile, ifstream::binary | ifstream::ate);
    uintmax_t fileSize = myFile.tellg();
    string file;
    {
        utimer total("Total");
        {
            utimer t("Calculate freq");
            readFrequencies(&ascii, myFile, &file, fileSize);
        }
        {
            utimer t("CreateMap");
            Node::createMap(Node::buildTree(ascii), &myMap);
        }
        {
            utimer timer("Create output");
            createOutput(&file, myMap);
        }
        {
            utimer timer("Write to file");
            writeToFile(file, encodedFile);
        }
    }
    return 0;
}

void readFrequencies(vector<uintmax_t>* ascii, ifstream &myFile, string* file, uintmax_t size){
    *file = string(size, ' ');
    myFile.seekg(0);
    myFile.read(&(*file)[0], size);
    for (auto &i : *file) {
        (*ascii)[i]++;
    }
}


void createOutput(string* inputFile, map<uintmax_t, string> myMap) {
    string tmp;
    for (auto &i : *inputFile) {
        (tmp).append(myMap[i]);
    }
    (tmp).append(string(8 - (tmp).size() % 8, '0'));
    swap(tmp, *inputFile);
}

void writeToFile(const string& bits, const string& encodedFile){
    ofstream outputFile(encodedFile, ios::binary | ios::out);
    uint8_t value = 0;
    for (long long unsigned i = 0; i < bits.size(); i += 8) {
        for (int j = 0; j < 8; j++) {
            value = (bits[i + j]=='1') | value << 1;
        }
        outputFile.write((char*) (&value), 1);
        value = 0;
    }
}
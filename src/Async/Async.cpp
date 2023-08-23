#include <iostream>
#include <vector>
#include <getopt.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <future>
#include <map>
#include "../utils/Node.h"
#include "../utils/utimer.cpp"
#include "../utils/utils.cpp"

using namespace std;

// OPT LIST
/*
 * h HELP
 * i input file path
 * p encoded file path
 * o decoded file path
 */
#define OPT_LIST "hi:p:t:"

void writeToFile(vector<string>* bits, const string& encodedFile, int numThreads);
void readFrequencies(ifstream* myFile, int numThreads, uintmax_t len, vector<string>* file, vector<int>* uAscii);
void createOutput(vector<string>* file, const map<int, string>& myMap, int numThreads);
vector<future<void>> futures;

int main(int argc, char* argv[])
{

    char option;
    vector<int> ascii(ASCII_MAX, 0);
    string inputFile, encodedFile, decodedFile;
    int numThreads = 4;

    inputFile = "./data/TestFiles/";
    encodedFile = "./data/EncodedFiles/Async/";

    while((option = (char)getopt(argc, argv, OPT_LIST)) != -1){
        switch (option) {
            case 'i':
                inputFile += optarg;
                break;
            case 'p':
                encodedFile += optarg;
                break;
            case 't':
                numThreads = (int)atoi(optarg);
                break;
            default:
                cout << "Invalid option" << endl;
                return 1;
        }
    }

    ifstream in(inputFile, ifstream::ate | ifstream::binary);
    uintmax_t fileSize = in.tellg();

    vector<string> file(numThreads);
    map<int, string> myMap;
    {
        utimer timer("Total");
        {
            utimer t("Read Frequencies");
            readFrequencies(&in, numThreads, fileSize, &file, &ascii);
        }
        {
            utimer t("Create Map");
            Node::createMap(Node::buildTree(ascii), &myMap);
        }
        {
            utimer t("Create Output");
            createOutput(&file, myMap, numThreads);
        }
        {
            utimer t("Write To File");
            writeToFile(&file, encodedFile, numThreads);
        }
    }
    return 0;
}

void readFrequencies(ifstream* myFile, int numThreads, uintmax_t len, vector<string>* file, vector<int>* uAscii){
    // Read file
    uintmax_t size1 = len / numThreads;
    mutex readFileMutex;
    mutex writeAsciiMutex;
    // Read file line by line
    for (int i = 0; i < numThreads; i++){
        uintmax_t size = (i==numThreads-1) ? len - (i*size1) : size1;
        (*file)[i] = string(size, ' ');
        futures.emplace_back(async(launch::async, calcChar, &(*myFile), &(*file)[i], i, size, size1, &readFileMutex, &writeAsciiMutex, &(*uAscii)));
    }
    for (int i = 0; i < numThreads; i++) futures[i].get();
}

void createOutput(vector<string>* file, const map<int, string>& myMap, int numThreads) {
    // Read file
    futures = vector<future<void>>();
    for (int i = 0; i < numThreads; i++){
        futures.emplace_back(async(launch::async, toBits, myMap, &(*file)[i]));
    }
    for (int i = 0; i < numThreads; i++) futures[i].get();
}

void writeToFile(vector<string>* bits, const string& encodedFile, int numThreads){
    ofstream outputFile(encodedFile, ios::binary | ios::out);
    vector<string> output(numThreads);
    mutex fileMutex;
    vector<std::thread> threads;
    uintmax_t writePos = 0;
    uint8_t Start, End = 0;
    string line;
    for (int i = 0; i < numThreads; i++) {
        Start = End;
        End = 8 - ((*bits)[i].size()-Start)%8;
        if (i == numThreads-1)
            (*bits)[i] += (string(((*bits)[i].size()-Start)%8, '0'));
        threads.emplace_back(wWrite, Start, End, &(*bits), i, writePos, ref(outputFile), ref(fileMutex));
        writePos += (((*bits)[i].size()-Start)+End);
    }
    for (int i = 0; i < numThreads; i++) {
        threads[i].join();
    }
}
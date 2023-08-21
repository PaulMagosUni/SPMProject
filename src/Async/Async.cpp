#include <iostream>
#include <utility>
#include <vector>
#include <getopt.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <future>
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
#define OPT_LIST "hi:p:t:"

Node buildTree(vector<int> ascii);
void writeToFile(string bits, const string& encodedFile, int numThreads);
vector<int> readFrequencies(ifstream* myFile, int numThreads, uintmax_t len, vector<string>* file);
void createMap(Node root, map<int, string> *map, const string &prefix = "");
string createOutput(vector<string> file, const map<int, string>& myMap, int numThreads);

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
    string output;
    {
        utimer timer("Total");
        {
            utimer t("readFrequencies");
            ascii = readFrequencies(&in, numThreads, fileSize, &file);
        }
        {
            utimer t("createMap");
            createMap(buildTree(ascii), &myMap);
        }
        {
            utimer t("createOutput");
            output = createOutput(file, myMap, numThreads);
        }
        {
            utimer t("writeToFile");
            writeToFile(output, encodedFile, numThreads);
        }
    }
    return 0;
}

vector<int> calcChar(ifstream* myFile, string* myString, int i, uintmax_t size, uintmax_t size1, mutex* readFileMutex){
    vector<int> ascii(ASCII_MAX, 0);
    {
        unique_lock<mutex> lock(*readFileMutex);
        (*myFile).seekg(i * size1);
        (*myFile).read(&(*myString)[0], size);
    }
    for (char j : (*myString))
        ascii[j]++;
    return ascii;
}

vector<int> readFrequencies(ifstream* myFile, int numThreads, uintmax_t len, vector<string>* file){
    // Read file
    uintmax_t size1 = len / numThreads;
    mutex readFileMutex;
    vector<int> ascii(ASCII_MAX, 0);
    vector<future<vector<int>>> futures;
    // Read file line by line
    for (int i = 0; i < numThreads; i++){
        uintmax_t size = (i==numThreads-1) ? len - (i*size1) : size1;
        (*file)[i] = string(size, ' ');
        futures.emplace_back(async(launch::async, calcChar, &(*myFile), &(*file)[i], i, size, size1, &readFileMutex));
    }
    for (int i = 0; i < numThreads; i++) {
        auto ascii2 = futures[i].get();
        for (int j = 0; j < ASCII_MAX; j++) {
            ascii[j] += ascii2[j];
        }
    }
    return ascii;
}

// Method for building the tree
Node buildTree(vector<int> ascii)
{
        Node *lChild, *rChild, *top;
        // Min Heap
        priority_queue<Node *, vector<Node *>, Node::cmp> minHeap;
        for (int i = 0; i < ASCII_MAX; i++) {
            if (ascii[i] != 0) {
                minHeap.push(new Node(ascii[i], i));
            }
        }
        while (minHeap.size() != 1) {
            lChild = minHeap.top();
            minHeap.pop();

            rChild = minHeap.top();
            minHeap.pop();

            top = new Node(lChild->getValue() + rChild->getValue(), 256);

            top->setLeftChild(lChild);
            top->setRightChild(rChild);

            minHeap.push(top);
        }
        return *minHeap.top();
}


void createMap(Node root, map<int, string> *map, const string &prefix){
    if (root.getChar() != 256) {
        (*map).insert(pair<int, string>(root.getChar(), prefix));
    } else {
        createMap(root.getLeftChild(), &(*map), prefix + "0");
        createMap(root.getRightChild(), &(*map), prefix + "1");
    }
}


string toBits(map<int, string> myMap, const string& line){
    string bits;
    for (char j : line) {
        bits.append(myMap[j]);
    }
    return bits;
}

string createOutput(vector<string> file, const map<int, string>& myMap, int numThreads) {
    // Read file
    vector<future<string>> futures;
    for (int i = 0; i < numThreads; i++){
        futures.emplace_back(async(launch::async, toBits, myMap, file[i]));
    }
    string output;
    for (int i = 0; i < numThreads; i++) {
        output.append(futures[i].get());
    }
    while (output.size()%8 != 0)
        output.push_back(false);
    return output;
}

void writeToFile(string bits, const string& encodedFile, int numThreads){
    ofstream outputFile(encodedFile, ios::binary | ios::out);
    vector<string> output(numThreads);
    mutex fileMutex;
        vector<std::thread> threads;
        uintmax_t Start = ((((bits).size() - ((bits).size() % 8)) /8) / numThreads + 1) *8;
        uintmax_t chunkSize = Start;
        for (int i = 0; i < numThreads; i++) {
            chunkSize += (i==numThreads-1) ? (bits).size() - ((i+1)*Start) : 0;
            threads.emplace_back([&bits, start=i*Start, chunkSize, &outputFile, &fileMutex]() {
                string output;
                uint8_t n = 0;
                uint8_t value = 0;
                for(int j = 0; j<chunkSize; j++){
                    value = ((bits)[start+j] == '1') | value << 1;
                    if(++n == 8)
                    {
                        output.append((char*) (&value), 1);
                        n = 0;
                        value = 0;
                    }
                }
                {
                    unique_lock<mutex> lock(fileMutex);
                    outputFile.seekp(start/8);
                    outputFile.write(output.c_str(), output.size());
                }
            });
        }
        for (int i = 0; i < numThreads; i++) {
            threads[i].join();
        }
        outputFile.close();
}
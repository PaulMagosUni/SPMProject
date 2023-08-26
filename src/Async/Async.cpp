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

int NUM_OF_THREADS = thread::hardware_concurrency();
vector<future<void>> futures;

int main(int argc, char* argv[])
{

    char option;
    vector<uintmax_t> ascii(ASCII_MAX, 0);
    string inputFile, encFileName, decodedFile, MyDir, csvFile, encFile;
    map<uintmax_t, string> myMap;

    MyDir = "./data/EncodedFiles/Async/";
    inputFile = "./data/TestFiles/";
    csvFile = "./data/CSV/Async";
    #if not defined(ALL)
        csvFile += "All";
    #elif defined(ALL)
        csvFile += (ALL? "All":"");
    #endif
    csvFile += ".csv";

    while((option = (char)getopt(argc, argv, OPT_LIST)) != -1){
        switch (option) {
            case 'i':
                inputFile += optarg;
                break;
            case 'p':
                encFileName += optarg;
                break;
            case 't':
                NUM_OF_THREADS = (int)atoi(optarg);
                break;
            default:
                cout << "Invalid option" << endl;
                return 1;
        }
    }
    encFile = MyDir + encFileName;

    ifstream in(inputFile, ifstream::ate | ifstream::binary);
    uintmax_t fileSize = in.tellg();
    vector<string> file(NUM_OF_THREADS);
    vector<uintmax_t> writePositions = vector<uintmax_t>(NUM_OF_THREADS);
    uintmax_t writePos = 0;


    vector<long> timers(8, 0);

    {
        utimer timer("Total", &timers[6]);
        {
            utimer t("Read File", &timers[0]);
            read(fileSize, &in, &file, NUM_OF_THREADS);
        }
        #if not defined(ALL) or ALL
            {
                utimer t("Count Frequencies", &timers[1]);
                mutex asciiMutex;
                for (int i = 0; i < NUM_OF_THREADS; i++)
                    futures.emplace_back(async(launch::async, countFrequency, &(file[i]), &ascii, &asciiMutex));
                for (int i = 0; i < NUM_OF_THREADS; i++) futures[i].get();
            }
            {
                utimer t("Create Map", &timers[2]);
                Node::createMap(Node::buildTree(ascii), &myMap);
            }
            {
                utimer t("Create Output", &timers[3]);
                futures = vector<future<void>>();
                for (int i = 0; i < NUM_OF_THREADS; i++){
                    futures.emplace_back(async(launch::async, toBits, myMap, &file, i));
                }
                for (int i = 0; i < NUM_OF_THREADS; i++) futures[i].get();
            }
            {
                utimer t("Bits To Bytes", &timers[4]);
                futures = vector<future<void>>();
                vector<string> output(NUM_OF_THREADS);
                uint8_t Start, End = 0;
                for (int i = 0; i < NUM_OF_THREADS; i++) {
                    Start = End;
                    if (i == NUM_OF_THREADS-1)
                        (file)[i] += (string(8-((file)[i].size()-Start)%8, '0'));
                    End = (8 - ((file)[i].size()-Start)%8) % 8;
                    (writePositions)[i] = (writePos);
                    futures.emplace_back(async(launch::async, toByte, Start, End, (file), i, &output[i]));
                    (writePos) += file[i].size() - Start + End;
                }
                for (int i = 0; i < NUM_OF_THREADS; i++) futures[i].get();
                file = output;
            }
        #elif defined(ALL) and not ALL
            mutex asciiMutex;
            for (int i = 0; i < NUM_OF_THREADS; i++)
                futures.emplace_back(async(launch::async, countFrequency, &(file[i]), &ascii, &asciiMutex));
            for (int i = 0; i < NUM_OF_THREADS; i++) futures[i].get();
            Node::createMap(Node::buildTree(ascii), &myMap);
            futures = vector<future<void>>();
            for (int i = 0; i < NUM_OF_THREADS; i++){
                futures.emplace_back(async(launch::async, toBits, myMap, &file, i));
            }
            for (int i = 0; i < NUM_OF_THREADS; i++) futures[i].get();
            futures = vector<future<void>>();
            vector<string> output(NUM_OF_THREADS);
            uint8_t Start, End = 0;
            for (int i = 0; i < NUM_OF_THREADS; i++) {
                Start = End;
                if (i == NUM_OF_THREADS-1)
                    (file)[i] += (string(8-((file)[i].size()-Start)%8, '0'));
                End = (8 - ((file)[i].size()-Start)%8) % 8;
                (writePositions)[i] = (writePos);
                futures.emplace_back(async(launch::async, toByte, Start, End, (file), i, &output[i]));
                (writePos) += file[i].size() - Start + End;
            }
            for (int i = 0; i < NUM_OF_THREADS; i++) futures[i].get();
            file = output;
        #endif
        {
            utimer t("Write To File", &timers[5]);
            write(encFile, file, writePositions, NUM_OF_THREADS);
        }
    }



    // Time without read and write
    timers[7] = timers[6] - timers[0] - timers[5];
    #if defined(ALL) and not ALL
        timers[0] = 0;
        timers[5] = 0;
    #endif
    writeResults(encFileName, fileSize, writePos, 1, timers, csvFile, false,
    #if defined(ALL)
        ALL
    #else
    true
    #endif
    );
    return 0;
}


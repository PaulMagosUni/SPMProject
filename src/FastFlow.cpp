#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <iostream>
#include <thread>
#include <fstream>
#include <map>
#include <utility>
#include <vector>
#include <condition_variable>
#include <getopt.h>
#include "utils/utimer.cpp"
#include "utils/utils.cpp"
#include "utils/Node.h"
#include "utils/ffstructures.h"


#define ASCII_MAX 256
// OPT LIST
/*
 * h HELP
 * i input file path
 * p encoded file path
 * o decoded file path
 */
#define OPT_LIST "hi:p:t:"

using namespace std;
using namespace ff;
int NUM_OF_THREADS = thread::hardware_concurrency();
#ifdef PRINT
bool print = true;
#else
bool print = false;
#endif
#if defined(NO3)
string csvPath = "./data/ResultsNO3.csv";
#else
string csvPath = "./data/Results.csv";
#endif

int Tasks = NUM_OF_THREADS;

int main(int argc, char* argv[]) {
    /* -----------------        VARIABLES        ----------------- */
    char option;
    vector<uintmax_t> ascii(ASCII_MAX, 0);
    string inputFile, encFileName, decodedFile, MyDir, encFile;
    map<uintmax_t, string> myMap;

    MyDir = "./data/EncodedFiles/FastFlow/";
    inputFile = "./data/TestFiles/";

    while ((option = (char) getopt(argc, argv, OPT_LIST)) != -1) {
        switch (option) {
            case 'i':
                inputFile += optarg;
                break;
            case 'p':
                encFileName += optarg;
                break;
            case 't':
                NUM_OF_THREADS = (int) atoi(optarg);
                break;
            default:
                cout << "Invalid option" << endl;
                return 1;
        }
    }
    encFile = MyDir;
    #ifdef NO3
        encFile += "NO3";
    #endif
    encFile += encFileName;
    /* -----------------       MUTEXES      ----------------- */
    mutex readFileMutex;
    mutex writeAsciiMutex;


    /* -----------------        FILE        ----------------- */
    /* Create input stream */
    ifstream in(inputFile, ifstream::ate | ifstream::binary);
    uintmax_t fileSize = in.tellg();
    utils::optimal(&Tasks, &NUM_OF_THREADS, fileSize);

    uintmax_t writePos = 0;
    vector<string> file(Tasks);
    vector<uintmax_t> writePositions(Tasks, 0);
    vector<uintmax_t> Starts(Tasks, 0);
    vector<uintmax_t> Ends(Tasks, 0);
    vector<long> timers = vector<long>(4, 0);

    if(print) cout << "Starting FastFlow Test with " << NUM_OF_THREADS << " threads, on file: "
                   << inputFile << " Size: ~" << utils::ConvertSize(fileSize, 'M') << "MB" << endl;
    {
        utimer total("Total time", &timers[2]);
        {
            utimer readTime("Read file", &timers[0]);
            utils::read(fileSize, &in, &file, Tasks);
        }
        vector<ff_task_t*> tasks = vector<ff_task_t*>(Tasks);
        for (size_t i = 0; i < Tasks; i++) {
            tasks[i] = new ff_task_t(&file[i], i, Tasks);
        }
        /* -----------------        FAST FLOW EMITTER FOR 1st FARM        ----------------- */
        Emitter emitter(NUM_OF_THREADS, &tasks);

        /* -----------------        FAST FLOW 1st FARM                    ----------------- */
        // Create farm with 1 emitter and NUM_OF_THREADS workers
        // This one will count the occurrences of each character
        ff_Farm<ff_task_t, ff_task_t> counts( []() {
            std::vector<std::unique_ptr<ff_node> > W;
            for(size_t i=0;i<NUM_OF_THREADS;++i) {
                W.push_back(make_unique<Worker>());
            }
            return W;
        }(), emitter );

        /* -----------------        FAST FLOW COLLECTOR                   ----------------- */
        // This one will collect the counts from the workers and add them to the ascii vector
        collectCounts collectCounts(&ascii, &file);
        counts.add_collector(collectCounts);

        /* -----------------        FAST FLOW MAP Creation (Huffman Tree) ----------------- */
        mapWorker createMap(&myMap, NUM_OF_THREADS, &file);

        /* -----------------        FAST FLOW MAP Application (FARM)      ----------------- */
        ff_Farm<ff_task_t, ff_task_t> mapApp( [&file]() {
            std::vector<std::unique_ptr<ff_node> > W;
            for(size_t i=0;i<NUM_OF_THREADS;++i) {
                W.push_back(make_unique<mapApply>(&file));
            }
            return W;
        }(),createMap);

        /* -----------------        FAST FLOW CALCULATE INDICES           ----------------- */
        // This one calculates the positions of the vector of strings from which each thread will read
        calcIndices calcIdx(&writePositions, &Starts, &Ends, &writePos, &file);
        mapApp.add_collector(calcIdx);

        /* -----------------        FAST FLOW APPLY BIT                   ----------------- */
        // This one will apply transform the string of bits into bytes
        applyBit appBit(NUM_OF_THREADS, encFile);
        /* -----------------        FAST FLOW PIPELINE                    ----------------- */
        // This one is the complete pipeline of all the previous nodes
        ff_Pipe<> pipe(counts, mapApp, appBit);
        pipe.run_and_wait_end();
        #ifdef TIME
        {
            utimer writeTime("Write file", &timers[1]);
            utils::write(encFile, file, writePositions, Tasks);
        }
        #endif
    }

    // Time without read and write
    timers[3] = timers[2] - timers[0] - timers[1];
    timers[0] = 0;
    timers[1] = 0;
    utils::writeResults("FastFlow", encFileName, fileSize, writePos, NUM_OF_THREADS, timers, true, false, Tasks, print, csvPath);
    return 0;
}
//
// Created by p.magos on 8/23/23.
//
#include <iostream>
#include <fstream>
#include <mutex>
#include <vector>
#include <cmath>
#include <thread>
#include <map>
#define ASCII_MAX 256
using namespace std;

#ifdef FTH
    bool forceThreads = true;
#else
    bool forceThreads = false;
#endif

namespace utils{
    void countFrequency(string* file, vector<uintmax_t>* uAscii, mutex* writeAsciiMutex){
        vector<uintmax_t> ascii(ASCII_MAX, 0);
        for (char j : (*file)) (ascii)[j]++;
        {
            unique_lock<mutex> lock(*writeAsciiMutex);
            for (int j = 0; j < ASCII_MAX; j++) {
                if ((ascii)[j] != 0) {
                    (*uAscii)[j] += (ascii)[j];
                }
            }
        }
    }

    

    void toBits(map<uintmax_t, string> myMap, vector<string>* file, int i){
        string bits;
        for (char j : (*file)[i]) {
            bits.append(myMap[j]);
        }
        (*file)[i] = bits;
    }

    void toByte(uint8_t Start, uint8_t End, vector<string>* bits, int pos, string* out){
        string output;
        uint8_t value = 0;
        int i;
        for (i = Start; i < (*bits)[pos].size(); i+=8) {
            for (uint8_t n = 0; n < 8; n++)
                value = ((*bits)[pos][i+n]=='1') | value << 1;
            output.append((char*) (&value), 1);
            value = 0;
        }
        if (pos != (*bits).size()-1) {
            for (uint32_t n = 0; n < 8-End; n++)
                value = ((*bits)[pos][i-8+n]=='1') | value << 1;
            for (i = 0; i < End; i++)
                value = ((*bits)[pos+1][i]=='1') | value << 1;
            output.append((char*) (&value), 1);
        }
        (*out)=output;
    }

    void writeResults(const string& kind, const string& testName, uintmax_t fileSize, uintmax_t writePos, int numOfThreads, const vector<long>& timers, bool pool= false, bool myImpl= false, int Tasks=0, bool print=false, const string& CSVPath=""){
        string csv;
        csv.append(kind).append(";");
        csv.append(testName).append(";");
        if (print) cout << "Test File: " << testName << endl;
        csv.append(to_string(fileSize)).append(";");
        if (print) cout << "File Size: " << fileSize << endl;
        csv.append(to_string(writePos/8)).append(";");
        if (print) cout << "Encoding Size: " << writePos/8 << endl;
        csv.append(to_string(numOfThreads)).append(";");
        if (print) cout << "Nw: " << numOfThreads << endl;
        if (pool) {
            csv.append(to_string(Tasks)).append(";");
            if (print) cout << "Tasks: " << Tasks << endl;
        }else{
            csv.append(to_string(numOfThreads)).append(";");
        }
        for (long timer : timers) if(timer!=0) csv.append(to_string(timer)).append(";");
        if(print){
            cout << "Total: " << timers[timers.size()-2] << endl;
            if(!myImpl)
                cout << "Computation: " << timers[timers.size()-1] << endl;
        }
        ofstream file;
        // Open file, if empty add header else append
        file.open(CSVPath, ios::out | ios::app);
        if (file.tellp() == 0) {
            file << "Kind;Test File;File Size;Encoding Size;Nw;Tasks;Total;Computation;\n";
        }
        else
            file.seekp(-1, ios_base::end);
        file << csv << endl;
        file.close();
    }

    void optimal(int* tasks, int* nw, uintmax_t fileSize){
        auto division = (uintmax_t)(fileSize/100000);
        if (division < *nw) {
            if(!forceThreads) *nw = 1;
            *tasks = 1;
        }else
            *tasks = (uintmax_t)(division / *nw);
        // Nearest power of 2
        if(!forceThreads) *nw = pow(2, ceil(log2(*nw)));
        *tasks = pow(2, ceil(log2(*tasks)));
        if (*tasks < *nw){
            uintmax_t tmp = *tasks;
            *tasks = *nw;
            if(!forceThreads) *nw = tmp;
        }
        if(*nw > thread::hardware_concurrency() and !forceThreads) *nw = thread::hardware_concurrency();
    }

    void read(uintmax_t fileSize, ifstream* in, vector<string>* file, int nw){
        uintmax_t chunk = fileSize / nw;
        for(int i = 0; i < nw; i++) {
            (*in).seekg(i*chunk, ios::beg);
            chunk = (i == nw-1)? fileSize - i*chunk : chunk;
            (*file)[i] = string(chunk, ' ');
            (*in).read(&(*file)[i][0], chunk);
        }
    }
    void read(ifstream* myFile, string* file, uintmax_t size){
        *file = string(size, ' ');
        (*myFile).seekg(0);
        (*myFile).read(&(*file)[0], size);
    }

    void write(const string& encFile, vector<string> file, vector<uintmax_t> writePositions, int nw){
        ofstream outputFile(encFile, ios::binary | ios::out);
        for (int i = 0; i < nw; i++) {
            outputFile.seekp(writePositions[i]/8);
            outputFile.write(file[i].c_str(), (file)[i].size());
        }
    }

    void write(const string& encFile, const string& bits, uintmax_t writePos){
        ofstream outputFile(encFile, ios::binary | ios::out);
        (outputFile).seekp(writePos/8);
        (outputFile).write((bits).c_str(), (bits).size());
    }
    
    /* https://stackoverflow.com/a/3984743 NOT MINE */
    static double ConvertSize(double bytes, char type)
    {
        switch (type)
        {
            case 'M':
                //convert to megabytes
                return (bytes / pow(1024, 2));
                break;
            case 'G':
                //convert to gigabytes
                return (bytes / pow(1024, 3));
                break;
            default:
                //default
                return bytes;
                break;
        }
    }
}

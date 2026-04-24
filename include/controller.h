#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>
using namespace std;
struct CompressionResult {
    string method;
    double entropy;
    double adaptiveRatio;
    double huffmanRatio;
    double timeTaken;
    string compressedData;
    string originalFilename;
};

struct DecompressionResult {
    string data;
    string originalFilename;
};

CompressionResult runAdaptiveCompression(const  string &data,const  string &originalFilename = "");

DecompressionResult runDecompression(const  string &packedData);

#endif

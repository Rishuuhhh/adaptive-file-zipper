#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>

struct CompressionResult {
    std::string method;
    double entropy;
    double adaptiveRatio;
    double huffmanRatio;
    double timeTaken;
    std::string compressedData;
};

CompressionResult runAdaptiveCompression(const std::string &data);

std::string runDecompression(const std::string &packedData);

#endif
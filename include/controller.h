#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>

struct CompressionResult {
    std::string method;
    double entropy;          // measured entropy of original input
    double adaptiveRatio;    // chosenPayloadSize / originalSize
    double huffmanRatio;     // entropy-based estimated lower bound
    double timeTaken;        // end-to-end compression time (ms)
    std::string compressedData;
    std::string originalFilename;
};

struct DecompressionResult {
    std::string data;
    std::string originalFilename;
};

CompressionResult runAdaptiveCompression(const std::string &data,
                                         const std::string &originalFilename = "");

DecompressionResult runDecompression(const std::string &packedData);

#endif
#include "controller.h"
#include "entropy.h"
#include "rle.h"
#include "block_huffman.h"
#include "file_io.h"
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>
using namespace std;
using namespace chrono;
static double estimateHuffmanRatio(const string &input) {
    if (input.empty()) return 1.0;
    int freq[256] = {};
    for (unsigned char b : input) {
        freq[b]++;
    }
    double entropy = 0.0;
    double n = (double)input.size();
    for (int i = 0; i < 256; i++) {
        if (freq[i] == 0) continue;
        double p = freq[i] / n;
        entropy -= p * log2(p);
    }
    return entropy / 8.0;
}
CompressionResult runAdaptiveCompression(const string &input, const string &originalFilename) {
    auto startTime = high_resolution_clock::now();
    if (input.empty()) {
        string blob = serializeCompressedData("NONE", 0.0, "", "", originalFilename);
        return {"NONE", 0.0, 1.0, 1.0, 0.0, blob, originalFilename};
    }
    double entropy = calculateEntropy(input);
    double huffmanEstimate = estimateHuffmanRatio(input);
    if (entropy >= 7.8) {
        auto endTime = high_resolution_clock::now();
        double ms = duration<double, milli>(endTime - startTime).count();
        string blob = serializeCompressedData("NONE", entropy, "", input, originalFilename);
        return {"NONE", entropy, 1.0, huffmanEstimate, ms, blob, originalFilename};
    }
    string rleOutput        = rleCompress(input);
    string globalHuffOutput = globalHuffmanCompress(input);
    string blockHuffOutput  = blockSplitCompress(input);
    double originalSize = (double)input.size();
    string bestMethod = "RLE";
    string bestOutput = rleOutput;
    if ((int)globalHuffOutput.size() < (int)bestOutput.size()) {
        bestMethod = "GLOBAL_HUFF";
        bestOutput = globalHuffOutput;
    }
    if ((int)blockHuffOutput.size() < (int)bestOutput.size()) {
        bestMethod = "BLOCK_HUFF";
        bestOutput = blockHuffOutput;
    }
    double adaptiveRatio = (double)bestOutput.size() / originalSize;
    auto endTime = high_resolution_clock::now();
    double ms = duration<double, milli>(endTime - startTime).count();
    string blob = serializeCompressedData(bestMethod, entropy, "", bestOutput, originalFilename);
    if ((int)blob.size() >= (int)input.size()) {
        string noneBlob = serializeCompressedData("NONE", entropy, "", input, originalFilename);
        return {"NONE", entropy, 1.0, huffmanEstimate, ms, noneBlob, originalFilename};
    }
    return {bestMethod, entropy, adaptiveRatio, huffmanEstimate, ms, blob, originalFilename};
}
DecompressionResult runDecompression(const string &blob) {
    if (blob.empty()) {
        throw runtime_error("Cannot decompress an empty file");
    }
    string method, codeMapData, payload, originalFilename;
    double entropy;
    deserializeCompressedData(blob, method, entropy, codeMapData, payload, originalFilename);
    string decompressed;
    if (method == "RLE") {
        decompressed = rleDecompress(payload);
    } else if (method == "GLOBAL_HUFF" || method == "BLOCK_HUFF") {
        decompressed = blockHuffmanDecompress(payload, {});
    } else if (method == "NONE") {
        decompressed = payload;
    } else if (method == "BLOCK_HUFFMAN" || method == "RLE_HUFFMAN") {
        auto legacyCodeMap = deserializeCodeMap(codeMapData);
        if (method == "RLE_HUFFMAN") {
            string huffDecoded = blockHuffmanDecompress(payload, legacyCodeMap);
            decompressed = rleDecompress(huffDecoded);
        } else {
            decompressed = blockHuffmanDecompress(payload, legacyCodeMap);
        }
    } else {
        throw runtime_error("Unknown compression method in file: " + method);
    }
    return {decompressed, originalFilename};
}

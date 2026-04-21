#include "controller.h"
#include "entropy.h"
#include "rle.h"
#include "block_huffman.h"
#include "file_io.h"

#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace std;
using namespace chrono;

// Estimate achievable Huffman ratio using Shannon entropy.
// Keeping this separate makes it easier to reason about stats shown in UI.
static double estimateHuffmanRatio(const string &inputData) {
    if (inputData.empty()) return 1.0;

    int fr[256] = {};
    for (int i = 0; i < static_cast<int>(inputData.size()); i++) {
        fr[static_cast<unsigned char>(inputData[i])]++;
    }

    double entropy = 0.0;
    double inputSize = static_cast<double>(inputData.size());
    for (int i = 0; i < 256; i++) {
        if (fr[i] > 0) {
            double probability = fr[i] / inputSize;
            entropy -= probability * log2(probability);
        }
    }

    return entropy / 8.0;
}

// Stores one candidate method with its output and ratio.
struct Cand {
    string meth;
    string out;
    double rat;
};

static vector<Cand> buildCompressionCandidates(const string &inputData) {
    const double originalSize = static_cast<double>(inputData.size());

    string rleOutput = rleCompress(inputData);
    string globalHuffmanOutput = globalHuffmanCompress(inputData);
    string blockHuffmanOutput = blockSplitCompress(inputData);

    return {
        {"RLE", rleOutput, static_cast<double>(rleOutput.size()) / originalSize},
        {"GLOBAL_HUFF", globalHuffmanOutput, static_cast<double>(globalHuffmanOutput.size()) / originalSize},
        {"BLOCK_HUFF", blockHuffmanOutput, static_cast<double>(blockHuffmanOutput.size()) / originalSize}
    };
}

static Cand pickBestCandidate(const vector<Cand> &candidates) {
    int bestIndex = 0;
    for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
        if (candidates[i].rat < candidates[bestIndex].rat) {
            bestIndex = i;
        }
    }
    return candidates[bestIndex];
}

// Try three methods and select the best one.
CompressionResult runAdaptiveCompression(const string &inputData, const string &originalFilename) {
    auto t0 = high_resolution_clock::now();

    if (inputData.empty()) {
        string pk = serializeCompressedData("NONE", 0.0, "", "", originalFilename);
        return {"NONE", 0.0, 1.0, 1.0, 0.0, pk, originalFilename};
    }

    double entropy = calculateEntropy(inputData);
    double estimatedHuffmanRatio = estimateHuffmanRatio(inputData);

    // Near-random data usually does not benefit from compression.
    if (entropy >= 7.8) {
        auto t1 = high_resolution_clock::now();
        double ms = duration<double, milli>(t1 - t0).count();
        string pk = serializeCompressedData("NONE", entropy, "", inputData, originalFilename);
        return {"NONE", entropy, 1.0, estimatedHuffmanRatio, ms, pk, originalFilename};
    }

    vector<Cand> candidates = buildCompressionCandidates(inputData);
    Cand bestCandidate = pickBestCandidate(candidates);

    auto t1 = high_resolution_clock::now();
    double ms = duration<double, milli>(t1 - t0).count();

    // Build packed output from the best candidate method.
    string packedBest = serializeCompressedData(
        bestCandidate.meth,
        entropy,
        "",
        bestCandidate.out,
        originalFilename
    );

    return {
        bestCandidate.meth,
        entropy,
        bestCandidate.rat,
        estimatedHuffmanRatio,
        ms,
        packedBest,
        originalFilename
    };
}

// Dispatch decompression based on stored method tag.
DecompressionResult runDecompression(const string &pk) {
    if (pk.empty()) {
        throw runtime_error("Cannot decompress empty input");
    }

    string meth, cms, pay, origFilename;
    double ent;
    deserializeCompressedData(pk, meth, ent, cms, pay, origFilename);

    string data;
    if (meth == "RLE") {
        // RLE payload is raw run records.
        data = rleDecompress(pay);
    } else if (meth == "GLOBAL_HUFF" || meth == "BLOCK_HUFF") {
        // Both variants use the same payload format with mode marker inside.
        data = blockHuffmanDecompress(pay, {});
    } else if (meth == "NONE") {
        data = pay;
    } else if (meth == "BLOCK_HUFFMAN" || meth == "RLE_HUFFMAN") {
        // Backward compatibility for legacy method tags.
        auto cm = deserializeCodeMap(cms);
        if (meth == "RLE_HUFFMAN") {
            string rd = blockHuffmanDecompress(pay, cm);
            data = rleDecompress(rd);
        } else {
            data = blockHuffmanDecompress(pay, cm);
        }
    } else {
        throw runtime_error("Unknown method: " + meth);
    }

    return {data, origFilename};
}

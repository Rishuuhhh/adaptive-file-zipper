#include "controller.h"
#include "entropy.h"
#include "rle.h"
#include "block_huffman.h"
#include "file_io.h"

#include <chrono>
#include <cmath>
#include <vector>

using namespace std;
using namespace chrono;

// ─────────────────────────────────────────────────────────────────────────────
// Huffman ratio baseline: Shannon entropy estimate (theoretical lower bound)
// ─────────────────────────────────────────────────────────────────────────────

static double estimateHuffmanRatio(const string &data) {
    if (data.empty()) return 1.0;
    int freq[256] = {};
    for (int i = 0; i < (int)data.size(); i++)
        freq[(unsigned char)data[i]]++;
    double H = 0.0;
    double n = (double)data.size();
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double p = freq[i] / n;
            H -= p * log2(p);
        }
    }
    return (H * n / 8.0) / n;  // pure entropy ratio, no header overhead
}

// ─────────────────────────────────────────────────────────────────────────────
// Candidate descriptor
// ─────────────────────────────────────────────────────────────────────────────

struct Candidate {
    string method;
    string payload;   // compressed bytes (no outer wrapper)
    double ratio;
};

// ─────────────────────────────────────────────────────────────────────────────
// Adaptive compression: run RLE, global Huffman, block Huffman — pick best
// ─────────────────────────────────────────────────────────────────────────────

CompressionResult runAdaptiveCompression(const string &data) {
    auto start = high_resolution_clock::now();

    if (data.empty()) {
        return {"NONE", 0.0, 1.0, 1.0, 0.0, ""};
    }

    double ent = calculateEntropy(data);
    double originalSize = (double)data.size();
    double huffmanDisplayRatio = estimateHuffmanRatio(data);

    // Near-random data: no method can help
    if (ent >= 7.8) {
        auto end = high_resolution_clock::now();
        double t = duration<double, milli>(end - start).count();
        string packed = packData("NONE", ent, "", data);
        return {"NONE", ent, 1.0, huffmanDisplayRatio, t, packed};
    }

    // ── Strategy 1: RLE ───────────────────────────────────────────────────────
    string rleOut = rleCompress(data);
    double rleRatio = (double)rleOut.size() / originalSize;

    // ── Strategy 2: Global canonical Huffman ─────────────────────────────────
    string globalOut = globalHuffmanCompress(data);
    double globalRatio = (double)globalOut.size() / originalSize;

    // ── Strategy 3: Block-based canonical Huffman ─────────────────────────────
    string blockOut = blockSplitCompress(data);
    double blockRatio = (double)blockOut.size() / originalSize;

    // ── Pick the smallest ─────────────────────────────────────────────────────
    vector<Candidate> candidates;
    candidates.push_back({"RLE",          rleOut,    rleRatio});
    candidates.push_back({"GLOBAL_HUFF",  globalOut, globalRatio});
    candidates.push_back({"BLOCK_HUFF",   blockOut,  blockRatio});

    int bestIdx = 0;
    for (int i = 1; i < (int)candidates.size(); i++)
        if (candidates[i].ratio < candidates[bestIdx].ratio) bestIdx = i;

    string chosenMethod = candidates[bestIdx].method;
    string payload      = candidates[bestIdx].payload;
    double adaptiveRatio = candidates[bestIdx].ratio;

    auto end = high_resolution_clock::now();
    double timeTaken = duration<double, milli>(end - start).count();

    // No codeMap needed — all encoding metadata is embedded in the payload
    string packed = packData(chosenMethod, ent, "", payload);
    return {chosenMethod, ent, adaptiveRatio, huffmanDisplayRatio, timeTaken, packed};
}

// ─────────────────────────────────────────────────────────────────────────────
// Decompression: dispatch by stored method tag
// ─────────────────────────────────────────────────────────────────────────────

string runDecompression(const string &packedData) {
    string method, codeMapSerialized, payload;
    double entropy;
    unpackData(packedData, method, entropy, codeMapSerialized, payload);

    if (method == "RLE") {
        return rleDecompress(payload);
    }
    if (method == "GLOBAL_HUFF" || method == "BLOCK_HUFF") {
        // blockHuffmanDecompress handles both 'G' and 'B' tagged payloads
        return blockHuffmanDecompress(payload, {});
    }
    if (method == "NONE") {
        return payload;
    }

    // Legacy methods from older compressed files
    if (method == "BLOCK_HUFFMAN" || method == "RLE_HUFFMAN") {
        auto codeMap = deserializeCodeMap(codeMapSerialized);
        if (method == "RLE_HUFFMAN") {
            string rleData = blockHuffmanDecompress(payload, codeMap);
            return rleDecompress(rleData);
        }
        return blockHuffmanDecompress(payload, codeMap);
    }

    return "";
}

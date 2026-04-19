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

// Estimate achievable Huffman compression ratio from entropy.
static double estHuffRatio(const string &d) {
    if (d.empty()) return 1.0;
    int fr[256] = {};
    for (int i = 0; i < (int)d.size(); i++)
        fr[(unsigned char)d[i]]++;
    double h = 0.0, n = (double)d.size();
    for (int i = 0; i < 256; i++) {
        if (fr[i] > 0) {
            double p = fr[i] / n;
            h -= p * log2(p);
        }
    }
    return (h * n / 8.0) / n;
}

// Stores one candidate method with its output and ratio.
struct Cand {
    string meth;
    string out;
    double rat;
};

// Try three methods and select the best one.
CompressionResult runAdaptiveCompression(const string &d) {
    auto t0 = high_resolution_clock::now();

    if (d.empty())
        return {"NONE", 0.0, 1.0, 1.0, 0.0, ""};

    double ent = calculateEntropy(d);
    double orig = (double)d.size();
    double hr = estHuffRatio(d);

    // Near-random data usually does not benefit from compression.
    if (ent >= 7.8) {
        auto t1 = high_resolution_clock::now();
        double ms = duration<double, milli>(t1 - t0).count();
        string pk = serializeCompressedData("NONE", ent, "", d);
        return {"NONE", ent, 1.0, hr, ms, pk};
    }

    // Try RLE.
    string ro = rleCompress(d);
    double rr = (double)ro.size() / orig;

    // Try global Huffman.
    string go = globalHuffmanCompress(d);
    double gr = (double)go.size() / orig;

    // Try block-split Huffman.
    string bo = blockSplitCompress(d);
    double br = (double)bo.size() / orig;

    // Select the smallest output among all candidates.
    vector<Cand> cv = {
        {"RLE",         ro, rr},
        {"GLOBAL_HUFF", go, gr},
        {"BLOCK_HUFF",  bo, br}
    };

    int bi = 0;
    for (int i = 1; i < (int)cv.size(); i++)
        if (cv[i].rat < cv[bi].rat) bi = i;

    string meth = cv[bi].meth;
    string pay  = cv[bi].out;
    double ar   = cv[bi].rat;

    auto t1 = high_resolution_clock::now();
    double ms = duration<double, milli>(t1 - t0).count();

    string pk = serializeCompressedData(meth, ent, "", pay);
    return {meth, ent, ar, hr, ms, pk};
}

// Dispatch decompression based on stored method tag.
string runDecompression(const string &pk) {
    string meth, cms, pay;
    double ent;
    deserializeCompressedData(pk, meth, ent, cms, pay);

    if (meth == "RLE")
        return rleDecompress(pay);

    if (meth == "GLOBAL_HUFF" || meth == "BLOCK_HUFF")
        return blockHuffmanDecompress(pay, {});

    if (meth == "NONE")
        return pay;

    // Backward compatibility for legacy method tags.
    if (meth == "BLOCK_HUFFMAN" || meth == "RLE_HUFFMAN") {
        auto cm = deserializeCodeMap(cms);
        if (meth == "RLE_HUFFMAN") {
            string rd = blockHuffmanDecompress(pay, cm);
            return rleDecompress(rd);
        }
        return blockHuffmanDecompress(pay, cm);
    }

    return "";
}

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "controller.h"
#include "huffman.h"
#include "file_io.h"
#include "entropy.h"

#include <string>
#include <cmath>
#include <algorithm>

// Compute repeat density: fraction of equal adjacent pairs.
static double repDens(const std::string &s) {
    if (s.size() < 2) return 0.0;
    size_t rr = 0;
    for (size_t i = 1; i < s.size(); i++)
        if (s[i] == s[i-1]) rr++;
    return (double)rr / s.size();
}

// Highly repetitive data should choose RLE.
RC_GTEST_PROP(Controller, RLEForHighRepeat, ()) {
    char c = *rc::gen::arbitrary<char>();
    int cnt = *rc::gen::inRange<int>(4, 200);
    std::string in(cnt, c);
    RC_ASSERT(repDens(in) > 0.35);
    auto r = runAdaptiveCompression(in);
    RC_ASSERT(r.method == "RLE");
}

// Low-repeat, low-entropy data should choose BLOCK_HUFFMAN.
RC_GTEST_PROP(Controller, BlockHuffmanForLowRepeatLowEntropy, ()) {
    int al = *rc::gen::inRange<int>(4, 9);
    int len = *rc::gen::inRange<int>(2, 100);

    std::string alpha;
    for (int i = 0; i < al; i++) alpha += (char)('a' + i);

    std::string in;
    in.reserve(len);
    char prev = '\0';
    for (int i = 0; i < len; i++) {
        int idx = *rc::gen::inRange<int>(0, al);
        char ch = alpha[idx];
        if (ch == prev && al > 1) ch = alpha[(idx + 1) % al];
        in += ch;
        prev = ch;
    }

    double rd = repDens(in);
    double ent = calculateEntropy(in);
    RC_PRE(rd <= 0.35 && ent < 7.5);

    auto r = runAdaptiveCompression(in);
    RC_ASSERT(r.method == "BLOCK_HUFFMAN");
}

// Near-random data should choose NONE.
RC_GTEST_PROP(Controller, NoneForRandomData, ()) {
    int rep = *rc::gen::inRange<int>(1, 5);
    int len = 256 * rep;

    std::string in;
    in.reserve(len);
    for (int r = 0; r < rep; r++)
        for (int i = 0; i < 256; i++)
            in += (char)i;

    double rd = repDens(in);
    double ent = calculateEntropy(in);
    RC_PRE(rd <= 0.35 && ent >= 7.5);

    auto r = runAdaptiveCompression(in);
    RC_ASSERT(r.method == "NONE");
    RC_ASSERT(r.adaptiveRatio == 1.0);
}

// huffmanRatio formula should be correct.
RC_GTEST_PROP(Controller, HuffmanRatioCorrect, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    auto r = runAdaptiveCompression(in);
    std::string hb = huffmanCompress(in);
    double exp = (double)hb.size() / (in.size() * 8.0);
    RC_ASSERT(std::abs(r.huffmanRatio - exp) < 1e-9);
}

// adaptiveRatio formula should be correct.
RC_GTEST_PROP(Controller, AdaptiveRatioCorrect, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    auto r = runAdaptiveCompression(in);
    RC_PRE(r.method != "NONE");

    std::string meth, cms, pay;
    double ent;
    unpackData(r.compressedData, meth, ent, cms, pay);

    double exp = (double)pay.size() / (double)in.size();
    RC_ASSERT(std::abs(r.adaptiveRatio - exp) < 1e-9);
}

// compress -> decompress should restore original input.
RC_GTEST_PROP(Controller, RoundTrip, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    auto r = runAdaptiveCompression(in);
    RC_ASSERT(runDecompression(r.compressedData) == in);
}

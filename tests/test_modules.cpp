#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include "entropy.h"
#include "rle.h"
#include "block_huffman.h"
#include "controller.h"
#include "file_io.h"
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>

// khaali input pe entropy 0 honi chahiye
TEST(Entropy, EmptyInputReturnsZero) {
    EXPECT_DOUBLE_EQ(calculateEntropy(""), 0.0);
}

// sab same chars ho toh entropy 0 hogi
TEST(Entropy, AllSameCharReturnsZero) {
    EXPECT_DOUBLE_EQ(calculateEntropy("aaaaaaa"), 0.0);
    EXPECT_DOUBLE_EQ(calculateEntropy("z"), 0.0);
}

// entropy hamesha 0 se 8 ke beech honi chahiye
RC_GTEST_PROP(Entropy, AlwaysInRange, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    double e = calculateEntropy(in);
    RC_ASSERT(e >= 0.0);
    RC_ASSERT(e <= 8.0);
}

// n distinct chars ho toh entropy = log2(n) hogi
RC_GTEST_PROP(Entropy, UniformMaximizesEntropy, ()) {
    int n = *rc::gen::inRange<int>(2, 257);
    std::string in;
    for (int i = 0; i < n; i++) in += (char)i;
    double exp = std::log2((double)n);
    double act = calculateEntropy(in);
    RC_ASSERT(std::abs(act - exp) < 1e-9);
}

// RLE output format sahi hona chahiye
RC_GTEST_PROP(RLE, OutputFormatIsWellFormed, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    std::string c = rleCompress(in);
    RC_ASSERT(!c.empty());
    RC_ASSERT(c.back() != ';');

    std::string tok;
    std::istringstream ss(c);
    while (std::getline(ss, tok, ';')) {
        int pc = (int)std::count(tok.begin(), tok.end(), '|');
        RC_ASSERT(pc == 1);
        auto p = tok.find('|');
        RC_ASSERT(p != std::string::npos);
        std::string cs = tok.substr(p + 1);
        RC_ASSERT(!cs.empty());
        for (char ch : cs)
            RC_ASSERT(std::isdigit((unsigned char)ch));
        RC_ASSERT(std::stoi(cs) > 0);
    }
}

// RLE compress -> decompress wapas original dena chahiye
RC_GTEST_PROP(RLE, RoundTrip, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    RC_ASSERT(rleDecompress(rleCompress(in)) == in);
}

// block huffman output valid hona chahiye
RC_GTEST_PROP(BlockHuffman, OutputIsValidBitString, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    BlockResult r = blockHuffmanCompress(in);
    for (char c : r.encoded)
        RC_ASSERT(c == '0' || c == '1');
    RC_ASSERT(!r.codeMap.empty());
}

// block huffman round-trip
RC_GTEST_PROP(BlockHuffman, RoundTrip, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    BlockResult r = blockHuffmanCompress(in);
    RC_ASSERT(blockHuffmanDecompress(r.encoded, r.codeMap) == in);
}

// khaali input pe NONE method aana chahiye
TEST(Controller, EmptyInputReturnsNone) {
    CompressionResult r = runAdaptiveCompression("");
    EXPECT_EQ(r.method, "NONE");
    EXPECT_DOUBLE_EQ(r.entropy, 0.0);
    EXPECT_DOUBLE_EQ(r.adaptiveRatio, 1.0);
    EXPECT_TRUE(r.compressedData.empty());
}

// unknown method pe khaali string aani chahiye
TEST(Controller, UnknownMethodReturnsEmpty) {
    std::string pk = packData("UNKNOWN", 0.0, "", "some payload");
    EXPECT_EQ(runDecompression(pk), "");
}

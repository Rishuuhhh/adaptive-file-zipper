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

// Entropy should be 0 for empty input.
TEST(Entropy, EmptyInputReturnsZero) {
    EXPECT_DOUBLE_EQ(calculateEntropy(""), 0.0);
}

// Entropy should be 0 when all characters are identical.
TEST(Entropy, AllSameCharReturnsZero) {
    EXPECT_DOUBLE_EQ(calculateEntropy("aaaaaaa"), 0.0);
    EXPECT_DOUBLE_EQ(calculateEntropy("z"), 0.0);
}

// Entropy should always stay within [0, 8].
RC_GTEST_PROP(Entropy, AlwaysInRange, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    double e = calculateEntropy(in);
    RC_ASSERT(e >= 0.0);
    RC_ASSERT(e <= 8.0);
}

// For n distinct symbols with uniform distribution, entropy = log2(n).
RC_GTEST_PROP(Entropy, UniformMaximizesEntropy, ()) {
    int n = *rc::gen::inRange<int>(2, 257);
    std::string in;
    for (int i = 0; i < n; i++) in += (char)i;
    double exp = std::log2((double)n);
    double act = calculateEntropy(in);
    RC_ASSERT(std::abs(act - exp) < 1e-9);
}

// RLE output format should be valid.
// The actual RLE format is binary: each run is a 3-byte record [char][countHigh][countLow].
RC_GTEST_PROP(RLE, OutputFormatIsWellFormed, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    std::string c = rleCompress(in);
    RC_ASSERT(!c.empty());
    // Output length must be a multiple of 3 (each run = 3 bytes).
    RC_ASSERT(c.size() % 3 == 0);
    // Each 3-byte record must have a positive run count.
    for (size_t i = 0; i + 2 < c.size(); i += 3) {
        int runLen = ((unsigned char)c[i + 1] << 8) | (unsigned char)c[i + 2];
        RC_ASSERT(runLen > 0);
    }
}

// RLE compress -> decompress should restore original input.
RC_GTEST_PROP(RLE, RoundTrip, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    RC_ASSERT(rleDecompress(rleCompress(in)) == in);
}

// Block Huffman output should be non-empty packed binary (not a text bit string).
// The encoded field contains a binary-packed bitstream with a header byte ('G' or 'B').
RC_GTEST_PROP(BlockHuffman, OutputIsValidBitString, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    BlockResult r = blockHuffmanCompress(in);
    RC_ASSERT(!r.encoded.empty());
    // First byte must be 'G' (global mode) or 'B' (block-split mode).
    char mode = r.encoded[0];
    RC_ASSERT(mode == 'G' || mode == 'B');
}

// Block Huffman round-trip.
RC_GTEST_PROP(BlockHuffman, RoundTrip, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    BlockResult r = blockHuffmanCompress(in);
    RC_ASSERT(blockHuffmanDecompress(r.encoded, r.codeMap) == in);
}

// Empty input should select NONE method.
TEST(Controller, EmptyInputReturnsNone) {
    CompressionResult r = runAdaptiveCompression("");
    EXPECT_EQ(r.method, "NONE");
    EXPECT_DOUBLE_EQ(r.entropy, 0.0);
    EXPECT_DOUBLE_EQ(r.adaptiveRatio, 1.0);
    // compressedData is a valid serialized blob (not empty) so it can round-trip.
    EXPECT_FALSE(r.compressedData.empty());
}

// Unknown method should throw std::runtime_error.
TEST(Controller, UnknownMethodThrows) {
    std::string pk = packData("UNKNOWN", 0.0, "", "some payload");
    EXPECT_THROW(runDecompression(pk), std::runtime_error);
}

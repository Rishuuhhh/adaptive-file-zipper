// Feature: adaptive-file-zipper, Property 4: Entropy always in range [0.0, 8.0]
// Feature: adaptive-file-zipper, Property 5: Uniform distribution maximizes entropy
// Feature: adaptive-file-zipper, Property 6: RLE output format is well-formed
// Feature: adaptive-file-zipper, Property 7: RLE round-trip
// Feature: adaptive-file-zipper, Property 8: Block Huffman output is valid bit-string
// Feature: adaptive-file-zipper, Property 9: Block Huffman round-trip

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

// ─── Unit tests: calculateEntropy ────────────────────────────────────────────

// Req 1a.4: empty input returns 0.0
TEST(Entropy, EmptyInputReturnsZero) {
    EXPECT_DOUBLE_EQ(calculateEntropy(""), 0.0);
}

// Req 1a.2: all-same-char input returns 0.0
TEST(Entropy, AllSameCharReturnsZero) {
    EXPECT_DOUBLE_EQ(calculateEntropy("aaaaaaa"), 0.0);
    EXPECT_DOUBLE_EQ(calculateEntropy("z"), 0.0);
}

// ─── Property 4: Entropy is always in range [0.0, 8.0] ───────────────────────
// Validates: Requirements 1a.5
RC_GTEST_PROP(Entropy, EntropyAlwaysInRange, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    double e = calculateEntropy(input);
    RC_ASSERT(e >= 0.0);
    RC_ASSERT(e <= 8.0);
}

// ─── Property 5: Uniform distribution maximizes entropy ──────────────────────
// Validates: Requirements 1a.3
RC_GTEST_PROP(Entropy, UniformDistributionMaximizesEntropy, ()) {
    // n >= 2 distinct chars, one occurrence each → entropy ≈ log2(n)
    int n = *rc::gen::inRange<int>(2, 257);

    std::string input;
    input.reserve(n);
    for (int i = 0; i < n; ++i) {
        input += static_cast<char>(i);
    }

    double expected = std::log2(static_cast<double>(n));
    double actual   = calculateEntropy(input);
    RC_ASSERT(std::abs(actual - expected) < 1e-9);
}

// ─── Property 6: RLE output format is well-formed ────────────────────────────
// Validates: Requirements 2.1
RC_GTEST_PROP(RLE, OutputFormatIsWellFormed, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    std::string compressed = rleCompress(input);

    // Must not end with ';'
    RC_ASSERT(!compressed.empty());
    RC_ASSERT(compressed.back() != ';');

    // Split by ';' and validate each token
    std::string token;
    std::istringstream stream(compressed);
    while (std::getline(stream, token, ';')) {
        // Each token must contain exactly one '|'
        int pipeCount = static_cast<int>(std::count(token.begin(), token.end(), '|'));
        RC_ASSERT(pipeCount == 1);

        auto pos = token.find('|');
        RC_ASSERT(pos != std::string::npos);

        // Part after '|' must be a positive integer
        std::string countStr = token.substr(pos + 1);
        RC_ASSERT(!countStr.empty());
        for (char ch : countStr) {
            RC_ASSERT(std::isdigit(static_cast<unsigned char>(ch)));
        }
        int count = std::stoi(countStr);
        RC_ASSERT(count > 0);
    }
}

// ─── Property 7: RLE round-trip ──────────────────────────────────────────────
// Validates: Requirements 2.2, 2.3
RC_GTEST_PROP(RLE, RoundTrip, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    RC_ASSERT(rleDecompress(rleCompress(input)) == input);
}

// ─── Property 8: Block Huffman output is a valid bit-string ──────────────────
// Validates: Requirements 3.1
RC_GTEST_PROP(BlockHuffman, OutputIsValidBitString, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    BlockResult result = blockHuffmanCompress(input);

    // encoded must contain only '0' and '1'
    for (char c : result.encoded) {
        RC_ASSERT(c == '0' || c == '1');
    }

    // codeMap must be non-empty
    RC_ASSERT(!result.codeMap.empty());
}

// ─── Property 9: Block Huffman round-trip ────────────────────────────────────
// Validates: Requirements 3.2, 3.3, 3.4
RC_GTEST_PROP(BlockHuffman, RoundTrip, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    BlockResult result = blockHuffmanCompress(input);
    std::string recovered = blockHuffmanDecompress(result.encoded, result.codeMap);
    RC_ASSERT(recovered == input);
}

// ─── Unit test: runAdaptiveCompression with empty input ───────────────────────
// Validates: Requirements 1.4
TEST(Controller, EmptyInputReturnsNone) {
    CompressionResult result = runAdaptiveCompression("");
    EXPECT_EQ(result.method, "NONE");
    EXPECT_DOUBLE_EQ(result.entropy, 0.0);
    EXPECT_DOUBLE_EQ(result.adaptiveRatio, 1.0);
    EXPECT_TRUE(result.compressedData.empty());
}

// ─── Unit test: runDecompression with unknown method ─────────────────────────
// Validates: Requirements 7.4
TEST(Controller, UnknownMethodReturnsEmpty) {
    std::string packed = packData("UNKNOWN", 0.0, "", "some payload");
    std::string result = runDecompression(packed);
    EXPECT_EQ(result, "");
}

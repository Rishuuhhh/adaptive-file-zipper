// Feature: adaptive-file-zipper, Property 1: RLE selected for high repeat density
// Feature: adaptive-file-zipper, Property 2: BLOCK_HUFFMAN selected for low repeat density and low entropy
// Feature: adaptive-file-zipper, Property 3: NONE selected for near-random data
// Feature: adaptive-file-zipper, Property 12: huffmanRatio formula correctness
// Feature: adaptive-file-zipper, Property 13: adaptiveRatio formula correctness
// Feature: adaptive-file-zipper, Property 14: Full compression-decompression pipeline round-trip

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

// Helper: compute repeat density the same way controller.cpp does
static double computeRepeatDensity(const std::string &s) {
    if (s.size() < 2) return 0.0;
    size_t runRepeats = 0;
    for (size_t i = 1; i < s.size(); ++i) {
        if (s[i] == s[i - 1]) runRepeats++;
    }
    return static_cast<double>(runRepeats) / s.size();
}

// Property 1: RLE is selected for high repeat density
// Validates: Requirements 1.1
RC_GTEST_PROP(Controller, RLESelectedForHighRepeatDensity, ()) {
    // Generate a char and a count > 3, repeat the char count times.
    // This guarantees repeatDensity = (count-1)/count > 0.35 for count > 3.
    char c = *rc::gen::arbitrary<char>();
    int count = *rc::gen::inRange<int>(4, 200);
    std::string input(count, c);

    // Sanity: verify repeatDensity > 0.35
    RC_ASSERT(computeRepeatDensity(input) > 0.35);

    auto result = runAdaptiveCompression(input);
    RC_ASSERT(result.method == "RLE");
}

// Property 2: BLOCK_HUFFMAN is selected for low repeat density and low entropy
// Validates: Requirements 1.2
RC_GTEST_PROP(Controller, BlockHuffmanSelectedForLowRepeatDensityAndLowEntropy, ()) {
    // Generate strings from a small alphabet (4-8 distinct chars) with no adjacent repeats.
    // This keeps repeatDensity = 0 and entropy low (log2(4..8) = 2..3 bits).
    int alphabetSize = *rc::gen::inRange<int>(4, 9);
    int length = *rc::gen::inRange<int>(2, 100);

    // Build alphabet of distinct printable chars
    std::string alphabet;
    for (int i = 0; i < alphabetSize; ++i) {
        alphabet += static_cast<char>('a' + i);
    }

    // Build string with no adjacent repeats by picking chars != previous
    std::string input;
    input.reserve(length);
    char prev = '\0';
    for (int i = 0; i < length; ++i) {
        // Pick a random index in alphabet that differs from prev
        int idx = *rc::gen::inRange<int>(0, alphabetSize);
        char ch = alphabet[idx];
        // If same as prev, shift to next in alphabet
        if (ch == prev && alphabetSize > 1) {
            ch = alphabet[(idx + 1) % alphabetSize];
        }
        input += ch;
        prev = ch;
    }

    double rd = computeRepeatDensity(input);
    double ent = calculateEntropy(input);

    RC_PRE(rd <= 0.35 && ent < 7.5);

    auto result = runAdaptiveCompression(input);
    RC_ASSERT(result.method == "BLOCK_HUFFMAN");
}

// Property 3: NONE is selected for near-random data
// Validates: Requirements 1.3, 6.3
RC_GTEST_PROP(Controller, NoneSelectedForNearRandomData, ()) {
    // Generate strings using all 256 byte values with roughly equal frequency.
    // Use a length that is a multiple of 256 to ensure uniform distribution.
    int repeats = *rc::gen::inRange<int>(1, 5);
    int length = 256 * repeats;

    std::string input;
    input.reserve(length);
    for (int r = 0; r < repeats; ++r) {
        for (int i = 0; i < 256; ++i) {
            input += static_cast<char>(i);
        }
    }

    double rd = computeRepeatDensity(input);
    double ent = calculateEntropy(input);

    RC_PRE(rd <= 0.35 && ent >= 7.5);

    auto result = runAdaptiveCompression(input);
    RC_ASSERT(result.method == "NONE");
    RC_ASSERT(result.adaptiveRatio == 1.0);
}

// Property 12: huffmanRatio formula correctness
// Validates: Requirements 6.1
RC_GTEST_PROP(Controller, HuffmanRatioFormulaCorrectness, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());

    auto result = runAdaptiveCompression(input);

    std::string huffBits = huffmanCompress(input);
    double expected = static_cast<double>(huffBits.size()) / (input.size() * 8.0);

    RC_ASSERT(std::abs(result.huffmanRatio - expected) < 1e-9);
}

// Property 13: adaptiveRatio formula correctness
// Validates: Requirements 6.2
RC_GTEST_PROP(Controller, AdaptiveRatioFormulaCorrectness, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());

    auto result = runAdaptiveCompression(input);

    RC_PRE(result.method != "NONE");

    // Unpack the compressedData to extract the payload
    std::string method, codeMapSerialized, payload;
    double entropy;
    unpackData(result.compressedData, method, entropy, codeMapSerialized, payload);

    double expected = static_cast<double>(payload.size()) / static_cast<double>(input.size());

    RC_ASSERT(std::abs(result.adaptiveRatio - expected) < 1e-9);
}

// Property 14: Full compression-decompression pipeline round-trip
// Validates: Requirements 7.1, 7.2, 7.3
RC_GTEST_PROP(Controller, FullCompressionDecompressionRoundTrip, ()) {
    auto input = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());

    auto result = runAdaptiveCompression(input);
    std::string recovered = runDecompression(result.compressedData);

    RC_ASSERT(recovered == input);
}

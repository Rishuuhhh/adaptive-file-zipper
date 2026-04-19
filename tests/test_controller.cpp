#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "controller.h"
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

// Low-repeat, low-entropy data should choose a Huffman variant (GLOBAL_HUFF or BLOCK_HUFF).
RC_GTEST_PROP(Controller, BlockHuffmanForLowRepeatLowEntropy, ()) {
    int al = *rc::gen::inRange<int>(4, 9);
    // Use a minimum length of 20 so Huffman overhead is amortized and beats RLE.
    int len = *rc::gen::inRange<int>(20, 100);

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
    // Exclude all-same-char strings (they get RLE-compressed).
    bool allSame = (in.find_first_not_of(in[0]) == std::string::npos);
    RC_PRE(rd <= 0.35 && ent < 7.5 && !allSame);

    auto r = runAdaptiveCompression(in);
    // For low-repeat, low-entropy data the controller picks a Huffman variant.
    RC_ASSERT(r.method == "GLOBAL_HUFF" || r.method == "BLOCK_HUFF");
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
// huffmanRatio is an entropy-based estimate: H(data) / 8.0
RC_GTEST_PROP(Controller, HuffmanRatioCorrect, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    auto r = runAdaptiveCompression(in);
    double exp = r.entropy / 8.0;
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

// Feature: frontend-compression-enhancements, Property 9: Compression round-trip (binary data)
// Validates: Requirements 3.4
RC_GTEST_PROP(Controller, RoundTripBinaryData, ()) {
    // Generate arbitrary byte sequences (including null bytes and all 0x00–0xFF values).
    auto in = *rc::gen::nonEmpty(rc::gen::container<std::string>(rc::gen::arbitrary<char>()));
    auto r = runAdaptiveCompression(in);
    RC_ASSERT(runDecompression(r.compressedData) == in);
}

// Feature: frontend-compression-enhancements, Property 10: Method tag is always a valid value
// Validates: Requirements 3.1
RC_GTEST_PROP(Controller, MethodTagIsValid, ()) {
    auto in = *rc::gen::nonEmpty(rc::gen::arbitrary<std::string>());
    auto r = runAdaptiveCompression(in);

    static const std::vector<std::string> validMethods = {
        "RLE", "GLOBAL_HUFF", "BLOCK_HUFF", "NONE"
    };
    bool methodValid = std::find(validMethods.begin(), validMethods.end(), r.method) != validMethods.end();
    RC_ASSERT(methodValid);

    // Also verify the deserialized method tag from compressedData matches r.method.
    std::string meth, cms, pay;
    double ent;
    deserializeCompressedData(r.compressedData, meth, ent, cms, pay);
    RC_ASSERT(meth == r.method);
}

// ── Error-path unit tests (Requirements 3.3, 3.6) ────────────────────────────

// A truncated Serialized_Blob (missing the entropy and codeMapLength lines)
// must cause deserializeCompressedData to throw std::runtime_error.
// Validates: Requirement 3.6
TEST(Controller, TruncatedBlobThrows) {
    // Only the method tag line — no entropy or codeMapLength lines.
    std::string truncated = "BLOCK_HUFF\n";
    std::string meth, cms, pay;
    double ent = 0.0;
    EXPECT_THROW(deserializeCompressedData(truncated, meth, ent, cms, pay),
                 std::runtime_error);
}

// A Serialized_Blob whose codeMapLength claims more bytes than are present
// must also cause deserializeCompressedData to throw std::runtime_error.
// Validates: Requirement 3.6
TEST(Controller, TruncatedPayloadThrows) {
    // Claim a codeMapLength of 9999 but provide no actual bytes after the header.
    std::string truncated = "RLE\n3.5\n9999\n";
    std::string meth, cms, pay;
    double ent = 0.0;
    EXPECT_THROW(deserializeCompressedData(truncated, meth, ent, cms, pay),
                 std::runtime_error);
}

// An unknown method tag stored in a well-formed blob must cause
// runDecompression to throw std::runtime_error.
// Validates: Requirement 3.6
TEST(Controller, UnknownMethodTagThrows) {
    // Build a syntactically valid blob with an unrecognised method tag.
    std::string blob = serializeCompressedData("UNKNOWN_METHOD", 3.0, "", "some payload");
    EXPECT_THROW(runDecompression(blob), std::runtime_error);
}

// Empty input must be compressed with method NONE and must round-trip back
// to an empty string.
// Validates: Requirement 3.3
TEST(Controller, EmptyInputCompressesToNoneAndRoundTrips) {
    CompressionResult r = runAdaptiveCompression("");
    ASSERT_EQ(r.method, "NONE");
    std::string decompressed = runDecompression(r.compressedData);
    ASSERT_EQ(decompressed, "");
}

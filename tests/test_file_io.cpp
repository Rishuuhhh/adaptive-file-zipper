#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "file_io.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

// codeMap serialize -> deserialize should preserve content.
RC_GTEST_PROP(FileIO, CodeMapRoundTrip, ()) {
    auto m = *rc::gen::container<std::unordered_map<std::string, std::string>>(
        rc::gen::arbitrary<std::string>(),
        rc::gen::arbitrary<std::string>()
    );
    RC_ASSERT(deserializeCodeMap(serializeCodeMap(m)) == m);
}

// Feature: frontend-compression-enhancements, Property 7: Serialization round-trip preserves all header fields
// Validates: Requirements 3.5
// For any tuple (method ∈ {"RLE","GLOBAL_HUFF","BLOCK_HUFF","NONE"}, entropy ∈ ℝ, codeMap: string, payload: string),
// serializing with serializeCompressedData then deserializing with deserializeCompressedData SHALL recover
// the original method, entropy, codeMap, and payload exactly.
RC_GTEST_PROP(FileIO, SerializationRoundTrip, ()) {
    // Generate a valid method tag from the allowed set
    auto methodIdx = *rc::gen::inRange<int>(0, 4);
    const std::string methods[] = {"RLE", "GLOBAL_HUFF", "BLOCK_HUFF", "NONE"};
    const std::string method = methods[methodIdx];

    // Generate entropy as a real number (use integer/100 to avoid floating-point
    // representation issues when comparing after stod(to_string(x)))
    auto ei = *rc::gen::inRange<int>(-100000, 100000);
    double entropy = ei / 100.0;

    // Generate arbitrary binary codeMap and payload strings
    auto codeMap = *rc::gen::arbitrary<std::string>();
    auto payload = *rc::gen::arbitrary<std::string>();

    // Serialize then deserialize
    std::string blob = serializeCompressedData(method, entropy, codeMap, payload);

    std::string outMethod, outCodeMap, outPayload;
    double outEntropy = 0.0;
    deserializeCompressedData(blob, outMethod, outEntropy, outCodeMap, outPayload);

    // All fields must be recovered exactly
    RC_ASSERT(outMethod == method);
    RC_ASSERT(outCodeMap == codeMap);
    RC_ASSERT(outPayload == payload);
    RC_ASSERT(std::abs(outEntropy - entropy) < 1e-6);
}

// pack -> unpack should preserve all fields.
RC_GTEST_PROP(FileIO, PackRoundTrip, ()) {
    auto meth = *rc::gen::map(
        rc::gen::arbitrary<std::string>(),
        [](std::string s) {
            s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
            return s;
        }
    );

    auto ei = *rc::gen::inRange<int>(0, 800);
    double ev = ei / 100.0;

    auto cms = *rc::gen::arbitrary<std::string>();
    auto pay = *rc::gen::arbitrary<std::string>();

    auto pk = packData(meth, ev, cms, pay);

    std::string rm, rc2, rp;
    double re = 0.0;
    unpackData(pk, rm, re, rc2, rp);

    RC_ASSERT(rm == meth);
    RC_ASSERT(rc2 == cms);
    RC_ASSERT(rp == pay);
    RC_ASSERT(std::abs(re - ev) < 1e-6);
}

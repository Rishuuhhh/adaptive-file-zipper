// Feature: adaptive-file-zipper, Property 10: CodeMap serialization round-trip
// Feature: adaptive-file-zipper, Property 11: File packing round-trip

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>

#include "file_io.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

// Property 10: CodeMap serialization round-trip
// Validates: Requirements 4.1, 4.2, 4.3, 4.4
RC_GTEST_PROP(FileIO, CodeMapSerializationRoundTrip, ()) {
    // Generate a random unordered_map<string, string>
    auto map = *rc::gen::container<std::unordered_map<std::string, std::string>>(
        rc::gen::arbitrary<std::string>(),
        rc::gen::arbitrary<std::string>()
    );

    auto serialized = serializeCodeMap(map);
    auto recovered = deserializeCodeMap(serialized);

    RC_ASSERT(recovered == map);
}

// Property 11: File packing round-trip
// Validates: Requirements 5.1, 5.2, 5.3, 5.4
RC_GTEST_PROP(FileIO, FilePackingRoundTrip, ()) {
    // Generate random method string (no newlines, to be a valid header line)
    auto method = *rc::gen::map(
        rc::gen::arbitrary<std::string>(),
        [](std::string s) {
            // Remove newlines so method is a single header line
            s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
            return s;
        }
    );

    // Generate entropy as a double in a reasonable range
    auto entropy = *rc::gen::inRange<int>(0, 800);
    double entropyVal = entropy / 100.0; // [0.0, 8.0)

    // Generate arbitrary codeMapSerialized (may contain |, ;, newlines)
    auto codeMapSerialized = *rc::gen::arbitrary<std::string>();

    // Generate arbitrary payload (may contain |, ;, newlines)
    auto payload = *rc::gen::arbitrary<std::string>();

    auto packed = packData(method, entropyVal, codeMapSerialized, payload);

    std::string recoveredMethod;
    double recoveredEntropy = 0.0;
    std::string recoveredCodeMap;
    std::string recoveredPayload;

    unpackData(packed, recoveredMethod, recoveredEntropy, recoveredCodeMap, recoveredPayload);

    RC_ASSERT(recoveredMethod == method);
    RC_ASSERT(recoveredCodeMap == codeMapSerialized);
    RC_ASSERT(recoveredPayload == payload);

    // Entropy round-trips through to_string/stod — check within tolerance
    RC_ASSERT(std::abs(recoveredEntropy - entropyVal) < 1e-6);
}

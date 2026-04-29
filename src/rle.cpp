#include "rle.h"
#include <string>
#include <vector>

using namespace std;

string rleCompress(const string &input) {
    if (input.empty()) return "";

    string compressed;
    int n = static_cast<int>(input.size());
    int i = 0;

    while (i < n) {
        char currentChar = input[i];
        int runLength = 1;

        // Count consecutive characters up to MAX_RUN_LENGTH
        while (i + runLength < n && 
               input[i + runLength] == currentChar && 
               runLength < MAX_RUN_LENGTH) {
            runLength++;
        }

        // Store the character followed by the 2-byte length
        compressed.push_back(currentChar);
        compressed.push_back(static_cast<char>((runLength >> 8) & 0xFF));
        compressed.push_back(static_cast<char>(runLength & 0xFF));

        i += runLength;
    }

    return compressed;
}

string rleDecompress(const string &compressed) {
    if (compressed.empty()) return "";

    // Validation: Each entry must be exactly 3 bytes (char + 2-byte length)
    if (compressed.size() % 3 != 0) {
        return ""; 
    }

    string result;
    // Pre-allocate memory to avoid multiple reallocations if the size is predictable
    // (Optional: can be estimated if you store the original size elsewhere)

    for (size_t i = 0; i + 2 < compressed.size(); i += 3) {
        char symbol = compressed[i];
        
        // Cast to unsigned char before shifting to avoid sign extension issues
        unsigned char high = static_cast<unsigned char>(compressed[i + 1]);
        unsigned char low  = static_cast<unsigned char>(compressed[i + 2]);
        
        int runLength = (static_cast<int>(high) << 8) | static_cast<int>(low);

        // Safety check to prevent massive allocations from corrupted files
        if (runLength > 0) {
            result.append(runLength, symbol);
        }
    }

    return result;
}

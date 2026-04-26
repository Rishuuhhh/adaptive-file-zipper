#include "rle.h"
#include <string>

using namespace std;

string rleCompress(const string &input) {
    if (input.empty()) return "";

    string compressed;
    int i = 0;

    while (i < (int)input.size()) {
        char currentChar = input[i];
        int runLength = 1;

        while (i + runLength < (int)input.size()
               && input[i + runLength] == currentChar
               && runLength < MAX_RUN_LENGTH) {
            runLength++;
        }

        compressed.push_back(currentChar);
        compressed.push_back(static_cast<char>((runLength >> 8) & 0xFF));
        compressed.push_back(static_cast<char>(runLength & 0xFF));

        i += runLength;
    }

    return compressed;
}

string rleDecompress(const string &compressed) {
    if (compressed.empty()) return "";

    if (compressed.size() % 3 != 0) {
        return "";
    }

    string result;

    for (int i = 0; i + 2 < (int)compressed.size(); i += 3) {
        char symbol = compressed[i];
        int high = static_cast<unsigned char>(compressed[i + 1]);
        int low  = static_cast<unsigned char>(compressed[i + 2]);
        int runLength = (high << 8) | low;

        result.append(runLength, symbol);
    }

    return result;
}

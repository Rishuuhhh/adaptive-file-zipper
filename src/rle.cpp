#include "rle.h"
#include <string>

using namespace std;

namespace {

void appendRunRecord(string &compressed, char symbol, int runLength) {
    compressed.push_back(symbol);
    compressed.push_back(static_cast<char>((runLength >> 8) & 0xFF));
    compressed.push_back(static_cast<char>(runLength & 0xFF));
}

} // namespace

string rleCompress(const string &inputData) {
    if (inputData.empty()) return "";

    string compressed;
    int runStart = 0;
    int inputSize = static_cast<int>(inputData.size());

    while (runStart < inputSize) {
        char currentSymbol = inputData[runStart];
        int cursor = runStart + 1;

        while (cursor < inputSize && inputData[cursor] == currentSymbol
               && (cursor - runStart) < MAX_RUN_LENGTH) {
            cursor++;
        }

        int runLength = cursor - runStart;
        appendRunRecord(compressed, currentSymbol, runLength);
        runStart = cursor;
    }

    return compressed;
}

string rleDecompress(const string &encodedData) {
    if (encodedData.empty()) return "";

    // Guard against malformed inputs; valid RLE payload is always 3-byte tuples.
    if (encodedData.size() % 3 != 0) {
        return "";
    }

    string result;
    int encodedSize = static_cast<int>(encodedData.size());

    for (int i = 0; i + 2 < encodedSize; i += 3) {
        char symbol = encodedData[i];
        int runLength = (static_cast<unsigned char>(encodedData[i + 1]) << 8)
                        | static_cast<unsigned char>(encodedData[i + 2]);
        result.append(runLength, symbol);
    }

    return result;
}
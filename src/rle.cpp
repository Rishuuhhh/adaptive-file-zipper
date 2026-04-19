#include "rle.h"
#include <string>

using namespace std;

string rleCompress(const string &data) {
    if (data.empty()) return "";

    string compressed;
    int i = 0;
    int n = (int)data.size();

    while (i < n) {
        char currentChar = data[i];
        int j = i + 1;

        while (j < n && data[j] == currentChar && (j - i) < MAX_RUN_LENGTH) {
            j++;
        }

        int runLength = j - i;
        compressed.push_back(currentChar);
        compressed.push_back((char)((runLength >> 8) & 0xFF));
        compressed.push_back((char)(runLength & 0xFF));
        i = j;
    }

    return compressed;
}

string rleDecompress(const string &data) {
    if (data.empty()) return "";

    string result;
    int n = (int)data.size();

    for (int i = 0; i + 2 < n; i += 3) {
        char currentChar = data[i];
        int runLength = ((unsigned char)data[i + 1] << 8) | (unsigned char)data[i + 2];
        result.append(runLength, currentChar);
    }

    return result;
}
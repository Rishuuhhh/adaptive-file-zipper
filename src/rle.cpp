#include "rle.h"
#include <string>

using namespace std;

// Binary RLE: each run = 1 char + 2 chars (big-endian count, max 65535 per run)

string rleCompress(const string &data) {
    if (data.empty()) return "";

    string out;
    int i = 0;
    int n = (int)data.size();

    while (i < n) {
        char c = data[i];
        int j = i + 1;
        while (j < n && data[j] == c && (j - i) < 65535) j++;
        int count = j - i;
        out.push_back(c);
        out.push_back((char)((count >> 8) & 0xFF));
        out.push_back((char)(count & 0xFF));
        i = j;
    }
    return out;
}

string rleDecompress(const string &data) {
    if (data.empty()) return "";

    string result;
    int n = (int)data.size();

    for (int i = 0; i + 2 < n; i += 3) {
        char c = data[i];
        int count = ((unsigned char)data[i+1] << 8) | (unsigned char)data[i+2];
        result.append(count, c);
    }
    return result;
}

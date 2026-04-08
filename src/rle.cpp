#include "rle.h"
#include <string>

using namespace std;

// Store each run as [char][count_hi][count_lo] (max run length: 65535).

string rleCompress(const string &d) {
    if (d.empty()) return "";

    string out;
    int i = 0, n = (int)d.size();

    while (i < n) {
        char c = d[i];
        int j = i + 1;
        // Count how many times this character repeats consecutively.
        while (j < n && d[j] == c && (j - i) < 65535) j++;
        int cnt = j - i;
        out.push_back(c);
        out.push_back((char)((cnt >> 8) & 0xFF)); // High byte of count.
        out.push_back((char)(cnt & 0xFF));        // Low byte of count.
        i = j;
    }
    return out;
}

string rleDecompress(const string &d) {
    if (d.empty()) return "";

    string res;
    int n = (int)d.size();

    for (int i = 0; i + 2 < n; i += 3) {
        char c   = d[i];
        int cnt  = ((unsigned char)d[i+1] << 8) | (unsigned char)d[i+2];
        res.append(cnt, c);
    }
    return res;
}

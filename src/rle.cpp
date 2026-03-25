#include "rle.h"
#include <string>

using namespace std;

// har run ko: [char][count_hi][count_lo] format mein store karo (max 65535 per run)

string rleCompress(const string &d) {
    if (d.empty()) return "";

    string out;
    int i = 0, n = (int)d.size();

    while (i < n) {
        char c = d[i];
        int j = i + 1;
        // same char kitni baar repeat ho raha hai
        while (j < n && d[j] == c && (j - i) < 65535) j++;
        int cnt = j - i;
        out.push_back(c);
        out.push_back((char)((cnt >> 8) & 0xFF)); // count ka high byte
        out.push_back((char)(cnt & 0xFF));         // count ka low byte
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

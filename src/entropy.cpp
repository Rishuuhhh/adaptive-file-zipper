#include "entropy.h"
#include <unordered_map>
#include <cmath>

using namespace std;

// har byte ki frequency count karo, phir shannon entropy nikalo
double calculateEntropy(const string &d) {
    if (d.empty()) return 0.0;

    unordered_map<char, int> fr;
    for (char c : d) fr[c]++;

    double h = 0.0;
    int n = d.size();
    for (auto &p : fr) {
        double prob = (double)p.second / n;
        h -= prob * log2(prob);
    }
    return h;
}

#include "entropy.h"
#include <unordered_map>
#include <cmath>

using namespace std;

// Calculates Shannon entropy
double calculateEntropy(const string &data) {
    if (data.empty()) return 0.0;

    unordered_map<char, int> freq;

    for (char c : data) {
        freq[c]++;
    }

    double entropy = 0.0;
    int n = data.size();

    for (auto &p : freq) {
        double prob = (double)p.second / n;
        entropy -= prob * log2(prob);
    }

    return entropy;
}
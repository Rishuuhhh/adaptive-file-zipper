#include "entropy.h"
#include <unordered_map>
#include <cmath>

using namespace std;

double calculateEntropy(const string &data) {
    if (data.empty()) return 0.0;
    unordered_map<char, int> frequencies;

    // Count how many times each byte value appears in the input.
    for (char c : data) frequencies[c]++;

    double entropy = 0.0;
    int inputSize = data.size();
    for (auto &entry : frequencies) {
        double probability = (double)entry.second / inputSize;
        entropy -= probability * log2(probability);
    }
    return entropy;
}

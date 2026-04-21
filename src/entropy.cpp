#include "entropy.h"
#include <unordered_map>
#include <cmath>

using namespace std;

static unordered_map<unsigned char, int> buildFrequencyTable(const string &inputData) {
    unordered_map<unsigned char, int> frequencies;
    for (unsigned char byteValue : inputData) {
        frequencies[byteValue]++;
    }
    return frequencies;
}

double calculateEntropy(const string &inputData) {
    if (inputData.empty()) return 0.0;

    unordered_map<unsigned char, int> frequencies = buildFrequencyTable(inputData);

    double entropy = 0.0;
    double totalSymbols = static_cast<double>(inputData.size());

    for (const auto &entry : frequencies) {
        double probability = static_cast<double>(entry.second) / totalSymbols;
        entropy -= probability * log2(probability);
    }

    return entropy;
}

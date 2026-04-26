#include "entropy.h"
#include <cmath>
#include <unordered_map>
using namespace std;
double calculateEntropy(const string &fileBytes) {
    if (fileBytes.empty()) return 0.0;
    unordered_map<unsigned char, int> freq;
    for (unsigned char b : fileBytes) {
        freq[b]++;
    }
    double entropy = 0.0;
    double totalBytes = static_cast<double>(fileBytes.size());
    for (auto &pair : freq) {
        double probability = pair.second / totalBytes;
        entropy -= probability * log2(probability);
    }
    return entropy;
}

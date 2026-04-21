#ifndef ENTROPY_H
#define ENTROPY_H

#include <string>

// Returns Shannon entropy in bits per symbol (range: 0 to 8 for byte streams).
double calculateEntropy(const std::string &data);

#endif
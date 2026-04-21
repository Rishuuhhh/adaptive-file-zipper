#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <string>
#include <unordered_map>

struct HuffmanResult {
    std::string encoded;
    // symbol (0-255) -> canonical bit-string code used for decoding
    std::unordered_map<int, std::string> codeMap;
};

HuffmanResult huffmanCompress(const std::string &data);
std::string   huffmanDecompress(const std::string &encoded,
                                const std::unordered_map<int, std::string> &codeMap);

#endif

#ifndef BLOCK_HUFFMAN_H
#define BLOCK_HUFFMAN_H

#include <string>
#include <unordered_map>

struct BlockResult {
    std::string encoded;
    std::unordered_map<std::string, std::string> codeMap;
};

// Adaptive: internally picks global vs block-split, whichever is smaller
BlockResult blockHuffmanCompress(const std::string &data);

// Exposed individually so the controller can compare all strategies
std::string globalHuffmanCompress(const std::string &data);
std::string blockSplitCompress(const std::string &data);

std::string blockHuffmanDecompress(
    const std::string &encoded,
    const std::unordered_map<std::string, std::string> &codeMap
);

#endif
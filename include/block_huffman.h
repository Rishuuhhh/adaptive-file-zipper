#ifndef BLOCK_HUFFMAN_H
#define BLOCK_HUFFMAN_H

#include <string>
#include <unordered_map>

struct BlockResult {
    std::string encoded;
    // Kept for backward compatibility with old decoder signatures.
    std::unordered_map<std::string, std::string> codeMap;
};

// Adaptive wrapper: chooses global or block-split Huffman based on output size.
BlockResult blockHuffmanCompress(const std::string &data);

// Exposed separately so controller can compare methods explicitly.
std::string globalHuffmanCompress(const std::string &data);
std::string blockSplitCompress(const std::string &data);

std::string blockHuffmanDecompress(
    const std::string &encoded,
    const std::unordered_map<std::string, std::string> &codeMap
);

#endif
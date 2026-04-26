#ifndef BLOCK_HUFFMAN_H
#define BLOCK_HUFFMAN_H
#include <string>
#include <unordered_map>
using namespace std;
struct BlockResult {
    string encoded;
    unordered_map<string, string> codeMap;
};
BlockResult blockHuffmanCompress(const string &data);
string globalHuffmanCompress(const string &data);
string blockSplitCompress(const string &data);
string blockHuffmanDecompress(const string &encoded, const unordered_map<string, string> & codeMap);
#endif

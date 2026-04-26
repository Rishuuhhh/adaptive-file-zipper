#ifndef HUFFMAN_H
#define HUFFMAN_H
#include <string>
#include <unordered_map>
using namespace std;
struct HuffmanResult {
     string encoded;
     unordered_map<int,  string> codeMap;
};
HuffmanResult huffmanCompress(const  string &data);
 string huffmanDecompress(const  string &encoded,
                              const  unordered_map<int,  string> &codeMap);
#endif

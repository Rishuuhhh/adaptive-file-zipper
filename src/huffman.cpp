#include "huffman.h"
#include <queue>
#include <vector>
using namespace std;

struct HuffNode {
    int symbol;
    int frequency;
    HuffNode *left;
    HuffNode *right;
    HuffNode(int sym, int freq) : symbol(sym), frequency(freq), left(nullptr), right(nullptr) {}
};

struct MinFrequency {
    bool operator()(HuffNode *a, HuffNode *b) const {
        return a->frequency > b->frequency;
    }
};

static void freeTree(HuffNode *node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

// Recursively builds Huffman codes for each symbol
static void buildCodes(HuffNode *node, string &prefix,
                       unordered_map<int, string> &codes) {
    if (!node) return;

    if (!node->left && !node->right) {
        codes[node->symbol] = prefix.empty() ? "0" : prefix;
        return;
    }

    prefix.push_back('0');
    buildCodes(node->left, prefix, codes);
    prefix.pop_back();

    prefix.push_back('1');
    buildCodes(node->right, prefix, codes);
    prefix.pop_back();
}

static string packBitString(const string &bits) {
    int padding = (8 - (int)bits.size() % 8) % 8;

    string packed;
    packed.push_back((char)padding);

    int currentByte = 0;
    int bitsInByte  = 0;

    for (char bit : bits) {
        currentByte = (currentByte << 1) | (bit == '1' ? 1 : 0);
        bitsInByte++;

        if (bitsInByte == 8) {
            packed.push_back((char)currentByte);
            currentByte = 0;
            bitsInByte  = 0;
        }
    }

    if (bitsInByte > 0) {
        packed.push_back((char)(currentByte << padding)); // add padding bits
    }

    return packed;
}

static string unpackBitString(const string &packed) {
    if (packed.empty()) return "";

    int padding = (unsigned char)packed[0];
    if (padding > 7) return "";

    string bits;

    for (int i = 1; i < (int)packed.size(); i++) {
        int byte = (unsigned char)packed[i];
        int validBits = (i == (int)packed.size() - 1) ? (8 - padding) : 8;

        for (int b = 7; b >= 8 - validBits; b--) {
            bits.push_back((byte >> b) & 1 ? '1' : '0');
        }
    }

    return bits;
}

HuffmanResult huffmanCompress(const string &input) {
    if (input.empty()) return {"", {}};

    int freq[256] = {};
    for (unsigned char b : input) freq[b]++;

    priority_queue<HuffNode*, vector<HuffNode*>, MinFrequency> heap;

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) heap.push(new HuffNode(i, freq[i]));
    }

    while (heap.size() > 1) {
        HuffNode *left  = heap.top(); heap.pop();
        HuffNode *right = heap.top(); heap.pop();

        HuffNode *parent = new HuffNode(-1, left->frequency + right->frequency);
        parent->left  = left;
        parent->right = right;

        heap.push(parent);
    }

    HuffNode *root = heap.top();

    unordered_map<int, string> codes;
    string prefix;

    buildCodes(root, prefix, codes);
    freeTree(root);

    string bitString;

    for (unsigned char b : input) {
        bitString += codes[b];
    }

    return {packBitString(bitString), codes};
}

string huffmanDecompress(const string &encoded, const unordered_map<int, string> &codes) {
    if (encoded.empty() || codes.empty()) return "";

    unordered_map<string, int> reverseMap;

    for (const auto &entry : codes) {
        reverseMap[entry.second] = entry.first;
    }

    string bits = unpackBitString(encoded);

    string result;
    string current;

    for (char bit : bits) {
        current.push_back(bit);

        auto it = reverseMap.find(current);

        if (it != reverseMap.end()) {
            result.push_back((char)it->second);
            current.clear();
        }
    }

    return result;
}

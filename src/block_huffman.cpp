#include "block_huffman.h"
#include <algorithm>
#include <queue>
#include <vector>

using namespace std;

static const int SMALL_FILE_THRESHOLD = 4 * 1024;
static const int BLOCK_SIZE           = 32 * 1024;

struct TreeNode {
    int symbol;
    int frequency;
    TreeNode *left;
    TreeNode *right;

    TreeNode(int sym, int freq) : symbol(sym), frequency(freq), left(nullptr), right(nullptr) {}
};

struct CompareByFrequency {
    bool operator()(TreeNode *a, TreeNode *b) const {
        return a->frequency > b->frequency;
    }
};

static TreeNode* buildHuffmanTree(int freq[256]) {
    priority_queue<TreeNode*, vector<TreeNode*>, CompareByFrequency> heap;

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            heap.push(new TreeNode(i, freq[i]));
        }
    }

    if (heap.empty()) return nullptr;

    while (heap.size() > 1) {
        TreeNode *left  = heap.top(); heap.pop();
        TreeNode *right = heap.top(); heap.pop();

        TreeNode *parent = new TreeNode(-1, left->frequency + right->frequency);
        parent->left  = left;
        parent->right = right;
        heap.push(parent);
    }

    return heap.top();
}

static void freeTree(TreeNode *node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

static void collectCodeLengths(TreeNode *node, int depth, int lengths[256]) {
    if (!node) return;

    if (node->symbol >= 0) {
        lengths[node->symbol] = (depth == 0) ? 1 : depth;
        return;
    }

    collectCodeLengths(node->left,  depth + 1, lengths);
    collectCodeLengths(node->right, depth + 1, lengths);
}

static void buildCanonicalCodes(int lengths[256], unsigned int codes[256]) {
    vector<pair<int, int>> symbolsByLength;
    for (int i = 0; i < 256; i++) {
        if (lengths[i] > 0) {
            symbolsByLength.push_back({lengths[i], i});
        }
    }
    sort(symbolsByLength.begin(), symbolsByLength.end());

    unsigned int code = 0;
    int prevLength = 0;

    for (auto &entry : symbolsByLength) {
        int len = entry.first;
        int sym = entry.second;

        code <<= (len - prevLength);
        codes[sym] = code;
        code++;
        prevLength = len;
    }
}

static void encodeToBitstream(const char *input, int inputLen,int lengths[256], unsigned int codes[256],vector<char> &bitstream, int &paddingBits) {
    int currentByte = 0;
    int bitsInByte  = 0;

    for (int i = 0; i < inputLen; i++) {
        int sym  = (unsigned char)input[i];
        int len  = lengths[sym];
        unsigned int code = codes[sym];

        for (int b = len - 1; b >= 0; b--) {
            currentByte = (currentByte << 1) | ((code >> b) & 1);
            bitsInByte++;

            if (bitsInByte == 8) {
                bitstream.push_back((char)currentByte);
                currentByte = 0;
                bitsInByte  = 0;
            }
        }
    }

    paddingBits = 0;
    if (bitsInByte > 0) {
        paddingBits = 8 - bitsInByte;
        bitstream.push_back((char)(currentByte << paddingBits));
    }
}

static string decodeFromBitstream(const string &data, int dataStart, int dataEnd,int paddingBits, int lengths[256], unsigned int codes[256]) {
    unordered_map<unsigned int, int> lookup;
    int maxLength = 0;

    for (int i = 0; i < 256; i++) {
        if (lengths[i] > 0) {
            unsigned int key = (codes[i] << 5) | (unsigned int)lengths[i];
            lookup[key] = i;
            if (lengths[i] > maxLength) maxLength = lengths[i];
        }
    }

    int totalBits = (dataEnd - dataStart) * 8 - paddingBits;
    int bitsRead  = 0;
    unsigned int buffer = 0;
    int bufferBits = 0;
    int readPos = dataStart;
    string result;

    while (bitsRead < totalBits) {
        while (bufferBits < maxLength && readPos < dataEnd) {
            buffer = (buffer << 8) | (unsigned char)data[readPos++];
            bufferBits += 8;
        }

        bool matched = false;
        int tryLen = min(bufferBits, maxLength);

        for (int len = 1; len <= tryLen; len++) {
            if (bitsRead + len > totalBits) break;

            unsigned int candidate = buffer >> (bufferBits - len);
            unsigned int key = (candidate << 5) | (unsigned int)len;

            auto it = lookup.find(key);
            if (it != lookup.end()) {
                result.push_back((char)it->second);
                bufferBits -= len;
                buffer &= (1u << bufferBits) - 1;
                bitsRead += len;
                matched = true;
                break;
            }
        }

        if (!matched) break;
    }

    return result;
}

static void writeBlockHeader(int lengths[256], int paddingBits, string &output) {
    vector<int> symbols;
    for (int i = 0; i < 256; i++) {
        if (lengths[i] > 0) symbols.push_back(i);
    }

    output.push_back((char)symbols.size());
    for (int sym : symbols) output.push_back((char)sym);
    for (int sym : symbols) output.push_back((char)lengths[sym]);
    output.push_back((char)paddingBits);
}

static int readBlockHeader(const string &data, int pos, int lengths[256], int &paddingBits) {
    if (pos >= (int)data.size()) return -1;

    int startPos = pos;
    int numSymbols = (unsigned char)data[pos++];

    if (numSymbols < 0 || numSymbols > 256) return -1;
    if (pos + numSymbols * 2 + 1 > (int)data.size()) return -1;

    int symbols[256] = {};
    for (int i = 0; i < numSymbols; i++) symbols[i] = (unsigned char)data[pos++];
    for (int i = 0; i < numSymbols; i++) lengths[symbols[i]] = (unsigned char)data[pos++];

    paddingBits = (unsigned char)data[pos++];
    if (paddingBits > 7) return -1;

    return pos - startPos;
}

static void writeInt32(string &output, int value) {
    output.push_back((char)((value >> 24) & 0xFF));
    output.push_back((char)((value >> 16) & 0xFF));
    output.push_back((char)((value >>  8) & 0xFF));
    output.push_back((char)( value        & 0xFF));
}

static int readInt32(const string &data, int pos) {
    if (pos < 0 || pos + 3 >= (int)data.size()) return -1;
    return ((unsigned char)data[pos]   << 24)
         | ((unsigned char)data[pos+1] << 16)
         | ((unsigned char)data[pos+2] <<  8)
         |  (unsigned char)data[pos+3];
}

string globalHuffmanCompress(const string &input) {
    int freq[256] = {};
    for (unsigned char b : input) freq[b]++;

    TreeNode *root = buildHuffmanTree(freq);
    if (!root) return "";

    int lengths[256] = {};
    collectCodeLengths(root, 0, lengths);
    freeTree(root);

    unsigned int codes[256] = {};
    buildCanonicalCodes(lengths, codes);

    vector<char> bitstream;
    int paddingBits = 0;
    encodeToBitstream(input.data(), (int)input.size(), lengths, codes, bitstream, paddingBits);

    string output;
    output.push_back('G');
    writeBlockHeader(lengths, paddingBits, output);
    for (char c : bitstream) output.push_back(c);

    return output;
}

string blockSplitCompress(const string &input) {
    int totalSize = (int)input.size();
    int numBlocks = (totalSize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    string output;
    output.push_back('B');
    writeInt32(output, numBlocks);

    for (int blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
        int blockStart  = blockIndex * BLOCK_SIZE;
        int blockLength = min(BLOCK_SIZE, totalSize - blockStart);

        int freq[256] = {};
        for (int i = blockStart; i < blockStart + blockLength; i++) {
            freq[(unsigned char)input[i]]++;
        }

        TreeNode *root = buildHuffmanTree(freq);
        int lengths[256] = {};
        collectCodeLengths(root, 0, lengths);
        freeTree(root);

        unsigned int codes[256] = {};
        buildCanonicalCodes(lengths, codes);

        vector<char> bitstream;
        int paddingBits = 0;
        encodeToBitstream(input.data() + blockStart, blockLength,
                          lengths, codes, bitstream, paddingBits);

        string blockData;
        writeBlockHeader(lengths, paddingBits, blockData);
        for (char c : bitstream) blockData.push_back(c);

        writeInt32(output, (int)blockData.size());
        output += blockData;
    }

    return output;
}

BlockResult blockHuffmanCompress(const string &input) {
    if (input.empty()) return {"", {}};

    bool allSame = true;
    for (int i = 1; i < (int)input.size(); i++) {
        if (input[i] != input[0]) { allSame = false; break; }
    }

    if (allSame) {
        int lengths[256] = {};
        lengths[(unsigned char)input[0]] = 1;
        unsigned int codes[256] = {};

        vector<char> bitstream;
        int paddingBits = 0;
        encodeToBitstream(input.data(), (int)input.size(), lengths, codes, bitstream, paddingBits);

        string output;
        output.push_back('G');
        writeBlockHeader(lengths, paddingBits, output);
        for (char c : bitstream) output.push_back(c);
        return {output, {}};
    }

    if ((int)input.size() < SMALL_FILE_THRESHOLD) {
        return {globalHuffmanCompress(input), {}};
    }

    string globalResult = globalHuffmanCompress(input);
    string blockResult  = blockSplitCompress(input);

    if ((int)globalResult.size() <= (int)blockResult.size()) {
        return {globalResult, {}};
    }
    return {blockResult, {}};
}

string blockHuffmanDecompress(const string &encoded,
                               const unordered_map<string, string> &) {
    if (encoded.empty()) return "";

    char mode = encoded[0];

    if (mode == 'G') {
        int pos = 1;
        int lengths[256] = {};
        int paddingBits = 0;

        int headerSize = readBlockHeader(encoded, pos, lengths, paddingBits);
        if (headerSize <= 0) return "";

        int dataStart = pos + headerSize;
        if (dataStart > (int)encoded.size()) return "";

        unsigned int codes[256] = {};
        buildCanonicalCodes(lengths, codes);

        int numSymbols = (unsigned char)encoded[1];
        if (numSymbols == 1) {
            int sym = (unsigned char)encoded[2];
            int pad = (unsigned char)encoded[4];
            int totalBits = ((int)encoded.size() - dataStart) * 8 - pad;
            if (totalBits < 0) return "";
            return string(totalBits, (char)sym);
        }

        return decodeFromBitstream(encoded, dataStart, (int)encoded.size(), paddingBits, lengths, codes);
    }

    if (mode == 'B') {
        int pos = 1;
        int numBlocks = readInt32(encoded, pos);
        pos += 4;

        if (numBlocks < 0) return "";

        string result;

        for (int b = 0; b < numBlocks; b++) {
            int blockSize = readInt32(encoded, pos);
            pos += 4;

            if (blockSize < 0) return "";

            int blockEnd = pos + blockSize;
            if (blockEnd > (int)encoded.size()) return "";

            int lengths[256] = {};
            int paddingBits = 0;

            int headerSize = readBlockHeader(encoded, pos, lengths, paddingBits);
            if (headerSize <= 0) return "";

            int dataStart = pos + headerSize;
            if (dataStart > blockEnd) return "";

            unsigned int codes[256] = {};
            buildCanonicalCodes(lengths, codes);

            result += decodeFromBitstream(encoded, dataStart, blockEnd, paddingBits, lengths, codes);
            pos = blockEnd;
        }

        return result;
    }

    return "";
}

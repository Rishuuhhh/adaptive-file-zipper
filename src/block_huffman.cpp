#include "block_huffman.h"
#include <queue>
#include <vector>
#include <algorithm>

using namespace std;

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

static const int SMALL_INPUT_THRESHOLD = 4 * 1024;      // 4 KB
static const int BLOCK_SIZE            = 32 * 1024;     // 32 KB per block

// ─────────────────────────────────────────────────────────────────────────────
// Tree node
// ─────────────────────────────────────────────────────────────────────────────

struct CNode {
    int symbol;  // -1 = internal, 0-255 = leaf
    int freq;
    CNode *left;
    CNode *right;
    CNode(int s, int f) : symbol(s), freq(f), left(0), right(0) {}
};

struct CNodeCmp {
    bool operator()(CNode *a, CNode *b) const { return a->freq > b->freq; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Tree construction
// ─────────────────────────────────────────────────────────────────────────────

static CNode* buildTree(int freq[256]) {
    priority_queue<CNode*, vector<CNode*>, CNodeCmp> pq;
    for (int i = 0; i < 256; i++)
        if (freq[i] > 0) pq.push(new CNode(i, freq[i]));
    if ((int)pq.size() == 0) return 0;
    while ((int)pq.size() > 1) {
        CNode *l = pq.top(); pq.pop();
        CNode *r = pq.top(); pq.pop();
        CNode *p = new CNode(-1, l->freq + r->freq);
        p->left = l; p->right = r;
        pq.push(p);
    }
    return pq.top();
}

static void freeTree(CNode *n) {
    if (!n) return;
    freeTree(n->left); freeTree(n->right);
    delete n;
}

// ─────────────────────────────────────────────────────────────────────────────
// Length extraction
// ─────────────────────────────────────────────────────────────────────────────

static void extractLengths(CNode *n, int depth, int lengths[256]) {
    if (!n) return;
    if (n->symbol >= 0) {
        lengths[n->symbol] = (depth == 0) ? 1 : depth;
        return;
    }
    extractLengths(n->left,  depth + 1, lengths);
    extractLengths(n->right, depth + 1, lengths);
}

// ─────────────────────────────────────────────────────────────────────────────
// Canonical code generation
// ─────────────────────────────────────────────────────────────────────────────

static void buildCanonicalCodes(int lengths[256], unsigned int codes[256]) {
    vector<pair<int,int>> syms;
    for (int i = 0; i < 256; i++)
        if (lengths[i] > 0) syms.push_back({lengths[i], i});
    sort(syms.begin(), syms.end());

    unsigned int code = 0;
    int prevLen = 0;
    for (int k = 0; k < (int)syms.size(); k++) {
        int len = syms[k].first;
        int sym = syms[k].second;
        code <<= (len - prevLen);
        codes[sym] = code;
        code++;
        prevLen = len;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Encoding — pack bits MSB-first into bytes
// ─────────────────────────────────────────────────────────────────────────────

static void encodeBits(const char *data, int dataLen,
                        int lengths[256], unsigned int codes[256],
                        vector<char> &packedBytes, int &padding) {
    int cur = 0, cnt = 0;
    for (int i = 0; i < dataLen; i++) {
        int sym = (unsigned char)data[i];
        int len = lengths[sym];
        unsigned int code = codes[sym];
        for (int b = len - 1; b >= 0; b--) {
            cur = (cur << 1) | ((code >> b) & 1);
            if (++cnt == 8) { packedBytes.push_back((char)cur); cur = 0; cnt = 0; }
        }
    }
    padding = 0;
    if (cnt > 0) {
        padding = 8 - cnt;
        packedBytes.push_back((char)(cur << padding));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Header: [1 byte N] [N bytes symbols] [N bytes lengths] [1 byte padding]
// ─────────────────────────────────────────────────────────────────────────────

static void writeHeader(int lengths[256], int padding, string &out) {
    vector<int> syms;
    for (int i = 0; i < 256; i++)
        if (lengths[i] > 0) syms.push_back(i);
    int N = (int)syms.size();
    out.push_back((char)N);
    for (int i = 0; i < N; i++) out.push_back((char)syms[i]);
    for (int i = 0; i < N; i++) out.push_back((char)lengths[syms[i]]);
    out.push_back((char)padding);
}

// Returns header byte count consumed; fills lengths[] and padding
static int readHeader(const string &src, int pos, int lengths[256], int &padding) {
    int start = pos;
    int N = (unsigned char)src[pos++];
    int symbols[256] = {};
    for (int i = 0; i < N; i++) symbols[i] = (unsigned char)src[pos++];
    for (int i = 0; i < N; i++) lengths[symbols[i]] = (unsigned char)src[pos++];
    padding = (unsigned char)src[pos++];
    return pos - start;
}

// ─────────────────────────────────────────────────────────────────────────────
// Decoding
// ─────────────────────────────────────────────────────────────────────────────

static string decodeBits(const string &src, int dataStart, int dataEnd,
                          int padding, int lengths[256], unsigned int codes[256]) {
    unordered_map<unsigned int, int> decodeTable;
    int maxLen = 0;
    for (int i = 0; i < 256; i++) {
        if (lengths[i] > 0) {
            unsigned int key = (codes[i] << 5) | (unsigned int)lengths[i];
            decodeTable[key] = i;
            if (lengths[i] > maxLen) maxLen = lengths[i];
        }
    }

    int totalBits = (dataEnd - dataStart) * 8 - padding;
    int bitsRead = 0;
    unsigned int buf = 0;
    int bufBits = 0;
    int srcIdx = dataStart;
    string result;

    while (bitsRead < totalBits) {
        while (bufBits < maxLen && srcIdx < dataEnd) {
            buf = (buf << 8) | (unsigned char)src[srcIdx++];
            bufBits += 8;
        }
        bool found = false;
        int limit = (bufBits < maxLen) ? bufBits : maxLen;
        for (int len = 1; len <= limit; len++) {
            if (bitsRead + len > totalBits) break;
            unsigned int candidate = buf >> (bufBits - len);
            unsigned int key = (candidate << 5) | (unsigned int)len;
            auto dit = decodeTable.find(key);
            if (dit != decodeTable.end()) {
                result.push_back((char)dit->second);
                bufBits -= len;
                buf &= (1u << bufBits) - 1;
                bitsRead += len;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Global Huffman: compress entire input as one unit
// ─────────────────────────────────────────────────────────────────────────────

string globalHuffmanCompress(const string &data) {
    int freq[256] = {};
    for (int i = 0; i < (int)data.size(); i++)
        freq[(unsigned char)data[i]]++;

    CNode *root = buildTree(freq);
    if (!root) return "";

    int lengths[256] = {};
    extractLengths(root, 0, lengths);
    freeTree(root);

    unsigned int codes[256] = {};
    buildCanonicalCodes(lengths, codes);

    vector<char> packedBytes;
    int padding = 0;
    encodeBits(data.data(), (int)data.size(), lengths, codes, packedBytes, padding);

    // Tag byte 'G' to identify global mode
    string out;
    out.push_back('G');
    writeHeader(lengths, padding, out);
    for (int i = 0; i < (int)packedBytes.size(); i++) out.push_back(packedBytes[i]);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Block-based Huffman: compress each BLOCK_SIZE chunk independently
//
// Format:
//   [1 byte 'B']
//   [4 bytes: number of blocks, big-endian]
//   For each block:
//     [4 bytes: compressed block size, big-endian]
//     [header + packed bits for that block]
// ─────────────────────────────────────────────────────────────────────────────

static void writeInt32(string &out, int v) {
    out.push_back((char)((v >> 24) & 0xFF));
    out.push_back((char)((v >> 16) & 0xFF));
    out.push_back((char)((v >>  8) & 0xFF));
    out.push_back((char)( v        & 0xFF));
}

static int readInt32(const string &src, int pos) {
    return ((unsigned char)src[pos]   << 24)
         | ((unsigned char)src[pos+1] << 16)
         | ((unsigned char)src[pos+2] <<  8)
         |  (unsigned char)src[pos+3];
}

string blockSplitCompress(const string &data) {
    int n = (int)data.size();
    int numBlocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE;

    string out;
    out.push_back('B');
    writeInt32(out, numBlocks);

    for (int b = 0; b < numBlocks; b++) {
        int blockStart = b * BLOCK_SIZE;
        int blockLen   = (blockStart + BLOCK_SIZE <= n) ? BLOCK_SIZE : (n - blockStart);

        int freq[256] = {};
        for (int i = blockStart; i < blockStart + blockLen; i++)
            freq[(unsigned char)data[i]]++;

        CNode *root = buildTree(freq);
        int lengths[256] = {};
        extractLengths(root, 0, lengths);
        freeTree(root);

        unsigned int codes[256] = {};
        buildCanonicalCodes(lengths, codes);

        vector<char> packedBytes;
        int padding = 0;
        encodeBits(data.data() + blockStart, blockLen, lengths, codes, packedBytes, padding);

        // Build this block's bytes
        string blockOut;
        writeHeader(lengths, padding, blockOut);
        for (int i = 0; i < (int)packedBytes.size(); i++) blockOut.push_back(packedBytes[i]);

        writeInt32(out, (int)blockOut.size());
        out += blockOut;
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API — compress
// ─────────────────────────────────────────────────────────────────────────────

BlockResult blockHuffmanCompress(const string &data) {
    if (data.empty()) return {"", {}};

    // Single-symbol edge case
    {
        bool allSame = true;
        for (int i = 1; i < (int)data.size(); i++)
            if (data[i] != data[0]) { allSame = false; break; }
        if (allSame) {
            // Store as global with trivial 1-bit code
            string out;
            out.push_back('G');
            int lengths[256] = {};
            lengths[(unsigned char)data[0]] = 1;
            unsigned int codes[256] = {};
            vector<char> packedBytes;
            int padding = 0;
            encodeBits(data.data(), (int)data.size(), lengths, codes, packedBytes, padding);
            writeHeader(lengths, padding, out);
            for (int i = 0; i < (int)packedBytes.size(); i++) out.push_back(packedBytes[i]);
            return {out, {}};
        }
    }

    int n = (int)data.size();

    if (n < SMALL_INPUT_THRESHOLD) {
        // Small input: global Huffman only (avoid block overhead)
        return {globalHuffmanCompress(data), {}};
    }

    // Large input: try both, pick smaller
    string global = globalHuffmanCompress(data);
    string blocked = blockSplitCompress(data);

    if ((int)global.size() <= (int)blocked.size())
        return {global, {}};
    else
        return {blocked, {}};
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API — decompress
// ─────────────────────────────────────────────────────────────────────────────

string blockHuffmanDecompress(const string &encoded,
                               const unordered_map<string, string> & /*codeMap*/) {
    if (encoded.empty()) return "";

    char mode = encoded[0];

    if (mode == 'G') {
        // Global Huffman
        int pos = 1;
        int lengths[256] = {};
        int padding = 0;
        int consumed = readHeader(encoded, pos, lengths, padding);
        int dataStart = pos + consumed;

        unsigned int codes[256] = {};
        buildCanonicalCodes(lengths, codes);

        // Single-symbol edge case
        int N = (unsigned char)encoded[1];
        if (N == 1) {
            int sym = (unsigned char)encoded[2];
            int pad = (unsigned char)encoded[4];
            int totalBits = ((int)encoded.size() - dataStart) * 8 - pad;
            return string(totalBits, (char)sym);
        }

        return decodeBits(encoded, dataStart, (int)encoded.size(), padding, lengths, codes);
    }

    if (mode == 'B') {
        // Block-based Huffman
        int pos = 1;
        int numBlocks = readInt32(encoded, pos); pos += 4;
        string result;

        for (int b = 0; b < numBlocks; b++) {
            int blockSize = readInt32(encoded, pos); pos += 4;
            int blockEnd  = pos + blockSize;

            int lengths[256] = {};
            int padding = 0;
            int consumed = readHeader(encoded, pos, lengths, padding);
            int dataStart = pos + consumed;

            unsigned int codes[256] = {};
            buildCanonicalCodes(lengths, codes);

            result += decodeBits(encoded, dataStart, blockEnd, padding, lengths, codes);
            pos = blockEnd;
        }
        return result;
    }

    return ""; // unknown mode
}

#include "huffman.h"
#include <queue>
#include <vector>

using namespace std;

// Huffman tree node.
struct HNode {
    int sym; // -1 = internal, 0-255 = leaf
    int frq;
    HNode *l, *r;
    HNode(int s, int f) : sym(s), frq(f), l(0), r(0) {}
};

struct HCmp {
    bool operator()(HNode *a, HNode *b) const { return a->frq > b->frq; }
};

// Traverse the tree and build symbol codes.
static void buildCodesFromTree(HNode *node,
                               string &currentPrefix,
                               unordered_map<int, string> &outCodeMap) {
    if (!node) return;

    if (!node->l && !node->r) {
        // A one-symbol alphabet still needs a non-empty code for decoding.
        outCodeMap[node->sym] = currentPrefix.empty() ? "0" : currentPrefix;
        return;
    }

    currentPrefix.push_back('0');
    buildCodesFromTree(node->l, currentPrefix, outCodeMap);
    currentPrefix.pop_back();

    currentPrefix.push_back('1');
    buildCodesFromTree(node->r, currentPrefix, outCodeMap);
    currentPrefix.pop_back();
}

static void freeTree(HNode *n) {
    if (!n) return;
    freeTree(n->l); freeTree(n->r);
    delete n;
}

// Pack bit string into bytes; first byte stores padding count.
static string packBits(const string &bits) {
    int pad = (8 - static_cast<int>(bits.size()) % 8) % 8;
    string out;
    out.push_back((char)pad);

    int currentByte = 0;
    int bitCount = 0;
    for (char b : bits) {
        currentByte = (currentByte << 1) | (b == '1' ? 1 : 0);
        bitCount++;

        if (bitCount == 8) {
            out.push_back((char)currentByte);
            currentByte = 0;
            bitCount = 0;
        }
    }

    if (bitCount > 0) {
        out.push_back((char)(currentByte << pad));
    }

    return out;
}

// Reconstruct bit string from packed bytes.
static string unpackBits(const string &pk) {
    if (pk.empty()) return "";

    int pad = (unsigned char)pk[0];
    if (pad < 0 || pad > 7) {
        return "";
    }

    string bits;
    int n = (int)pk.size();

    for (int i = 1; i < n; i++) {
        int by = (unsigned char)pk[i];
        int valid = (i == n - 1) ? (8 - pad) : 8;
        for (int b = 7; b >= 8 - valid; b--) {
            bits.push_back((by >> b) & 1 ? '1' : '0');
        }
    }

    return bits;
}

HuffmanResult huffmanCompress(const string &d) {
    if (d.empty()) return {"", {}};

    int fr[256] = {};
    for (int i = 0; i < (int)d.size(); i++)
        fr[(unsigned char)d[i]]++;

    priority_queue<HNode*, vector<HNode*>, HCmp> pq;
    for (int i = 0; i < 256; i++)
        if (fr[i] > 0) pq.push(new HNode(i, fr[i]));

    while ((int)pq.size() > 1) {
        HNode *a = pq.top(); pq.pop();
        HNode *b = pq.top(); pq.pop();
        HNode *p = new HNode(-1, a->frq + b->frq);
        p->l = a; p->r = b;
        pq.push(p);
    }

    HNode *root = pq.top();
    unordered_map<int, string> codes;
    string pre;
    buildCodesFromTree(root, pre, codes);
    freeTree(root);

    string bits;
    for (int i = 0; i < (int)d.size(); i++) {
        bits += codes[(unsigned char)d[i]];
    }

    return {packBits(bits), codes};
}

string huffmanDecompress(const string &enc, const unordered_map<int, string> &cm) {
    if (enc.empty()) return "";
    if (cm.empty()) return "";

    unordered_map<string, int> rev;
    for (const auto &kv : cm) rev[kv.second] = kv.first;

    string bits = unpackBits(enc);
    if (bits.empty() && enc.size() > 1) {
        return "";
    }

    string res, cur;
    for (char b : bits) {
        cur.push_back(b);
        auto it = rev.find(cur);
        if (it != rev.end()) {
            res.push_back((char)it->second);
            cur.clear();
        }
    }
    return res;
}

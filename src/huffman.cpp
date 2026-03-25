#include "huffman.h"
#include <queue>
#include <vector>

using namespace std;

// Huffman tree node
struct HNode {
    int symbol; // -1 for internal nodes
    int freq;
    HNode *left;
    HNode *right;
    HNode(int s, int f) : symbol(s), freq(f), left(0), right(0) {}
};

struct HCmp {
    bool operator()(HNode *a, HNode *b) const { return a->freq > b->freq; }
};

static void buildCodes(HNode *n, string &prefix, unordered_map<int, string> &out) {
    if (!n) return;
    if (!n->left && !n->right) {
        out[n->symbol] = prefix.empty() ? "0" : prefix;
        return;
    }
    prefix.push_back('0'); buildCodes(n->left,  prefix, out); prefix.pop_back();
    prefix.push_back('1'); buildCodes(n->right, prefix, out); prefix.pop_back();
}

static void freeTree(HNode *n) {
    if (!n) return;
    freeTree(n->left); freeTree(n->right);
    delete n;
}

// Pack bit-string into bytes. First byte = padding count.
static string packBits(const string &bits) {
    int padding = (8 - (int)bits.size() % 8) % 8;
    string out;
    out.push_back((char)padding);
    int cur = 0, cnt = 0;
    for (char b : bits) {
        cur = (cur << 1) | (b == '1' ? 1 : 0);
        if (++cnt == 8) { out.push_back((char)cur); cur = 0; cnt = 0; }
    }
    if (cnt > 0) out.push_back((char)(cur << padding));
    return out;
}

static string unpackBits(const string &packed) {
    if (packed.empty()) return "";
    int padding = (unsigned char)packed[0];
    string bits;
    int n = (int)packed.size();
    for (int i = 1; i < n; i++) {
        int byte = (unsigned char)packed[i];
        int valid = (i == n - 1) ? (8 - padding) : 8;
        for (int b = 7; b >= 8 - valid; b--)
            bits.push_back((byte >> b) & 1 ? '1' : '0');
    }
    return bits;
}

HuffmanResult huffmanCompress(const string &data) {
    if (data.empty()) return {"", {}};

    int freq[256] = {};
    for (int i = 0; i < (int)data.size(); i++)
        freq[(unsigned char)data[i]]++;

    priority_queue<HNode*, vector<HNode*>, HCmp> pq;
    for (int i = 0; i < 256; i++)
        if (freq[i] > 0) pq.push(new HNode(i, freq[i]));

    while ((int)pq.size() > 1) {
        HNode *l = pq.top(); pq.pop();
        HNode *r = pq.top(); pq.pop();
        HNode *p = new HNode(-1, l->freq + r->freq);
        p->left = l; p->right = r;
        pq.push(p);
    }

    HNode *root = pq.top();
    unordered_map<int, string> codes;
    string prefix;
    buildCodes(root, prefix, codes);
    freeTree(root);

    string bits;
    for (int i = 0; i < (int)data.size(); i++)
        bits += codes[(unsigned char)data[i]];

    return {packBits(bits), codes};
}

string huffmanDecompress(const string &encoded, const unordered_map<int, string> &codeMap) {
    if (encoded.empty()) return "";

    unordered_map<string, int> rev;
    for (auto &kv : codeMap) rev[kv.second] = kv.first;

    string bits = unpackBits(encoded);
    string result, cursor;
    for (char b : bits) {
        cursor.push_back(b);
        auto it = rev.find(cursor);
        if (it != rev.end()) {
            result.push_back((char)it->second);
            cursor.clear();
        }
    }
    return result;
}

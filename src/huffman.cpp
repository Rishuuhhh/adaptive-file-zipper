#include "huffman.h"
#include <queue>
#include <vector>

using namespace std;

// huffman tree ka node
struct HNode {
    int sym; // -1 = internal, 0-255 = leaf
    int frq;
    HNode *l, *r;
    HNode(int s, int f) : sym(s), frq(f), l(0), r(0) {}
};

struct HCmp {
    bool operator()(HNode *a, HNode *b) const { return a->frq > b->frq; }
};

// tree traverse karke har symbol ka code nikalo
static void getCodes(HNode *n, string &pre, unordered_map<int, string> &out) {
    if (!n) return;
    if (!n->l && !n->r) {
        out[n->sym] = pre.empty() ? "0" : pre;
        return;
    }
    pre.push_back('0'); getCodes(n->l, pre, out); pre.pop_back();
    pre.push_back('1'); getCodes(n->r, pre, out); pre.pop_back();
}

static void freeTree(HNode *n) {
    if (!n) return;
    freeTree(n->l); freeTree(n->r);
    delete n;
}

// bit string ko bytes mein pack karo, pehla byte = padding count
static string packBits(const string &bits) {
    int pad = (8 - (int)bits.size() % 8) % 8;
    string out;
    out.push_back((char)pad);
    int cur = 0, cnt = 0;
    for (char b : bits) {
        cur = (cur << 1) | (b == '1' ? 1 : 0);
        if (++cnt == 8) { out.push_back((char)cur); cur = 0; cnt = 0; }
    }
    if (cnt > 0) out.push_back((char)(cur << pad));
    return out;
}

// bytes se bit string wapas nikalo
static string unpackBits(const string &pk) {
    if (pk.empty()) return "";
    int pad = (unsigned char)pk[0];
    string bits;
    int n = (int)pk.size();
    for (int i = 1; i < n; i++) {
        int by = (unsigned char)pk[i];
        int valid = (i == n - 1) ? (8 - pad) : 8;
        for (int b = 7; b >= 8 - valid; b--)
            bits.push_back((by >> b) & 1 ? '1' : '0');
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
    getCodes(root, pre, codes);
    freeTree(root);

    string bits;
    for (int i = 0; i < (int)d.size(); i++)
        bits += codes[(unsigned char)d[i]];

    return {packBits(bits), codes};
}

string huffmanDecompress(const string &enc, const unordered_map<int, string> &cm) {
    if (enc.empty()) return "";

    unordered_map<string, int> rev;
    for (auto &kv : cm) rev[kv.second] = kv.first;

    string bits = unpackBits(enc);
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

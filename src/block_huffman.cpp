#include "block_huffman.h"
#include <queue>
#include <vector>
#include <algorithm>

using namespace std;

// Use global Huffman mode for inputs smaller than 4 KB.
static const int SML_THRESH = 4 * 1024;
// Block size used by block-split mode.
static const int BLK_SZ = 32 * 1024;

// A single Huffman tree node.
struct CNode {
    int sym;  // -1 = internal, 0-255 = leaf byte
    int frq;  // Symbol frequency
    CNode *l, *r;
    CNode(int s, int f) : sym(s), frq(f), l(0), r(0) {}
};

// Comparator for a min-heap by frequency.
struct CNodeCmp {
    bool operator()(CNode *a, CNode *b) const { return a->frq > b->frq; }
};

// Build Huffman tree from a frequency table.
static CNode* buildTree(int fr[256]) {
    priority_queue<CNode*, vector<CNode*>, CNodeCmp> pq;
    for (int i = 0; i < 256; i++)
        if (fr[i] > 0) pq.push(new CNode(i, fr[i]));
    if (pq.empty()) return 0;
    // Keep merging the two least-frequent nodes.
    while ((int)pq.size() > 1) {
        CNode *a = pq.top(); pq.pop();
        CNode *b = pq.top(); pq.pop();
        CNode *p = new CNode(-1, a->frq + b->frq);
        p->l = a; p->r = b;
        pq.push(p);
    }
    return pq.top();
}

// Free allocated Huffman tree memory.
static void freeTree(CNode *n) {
    if (!n) return;
    freeTree(n->l); freeTree(n->r);
    delete n;
}

// Traverse the tree and compute bit-length per symbol.
static void getLen(CNode *n, int dep, int ln[256]) {
    if (!n) return;
    if (n->sym >= 0) {
        ln[n->sym] = (dep == 0) ? 1 : dep; // Single-symbol alphabet still needs 1 bit.
        return;
    }
    getLen(n->l, dep + 1, ln);
    getLen(n->r, dep + 1, ln);
}

// Build canonical Huffman codes from code lengths.
static void mkCodes(int ln[256], unsigned int cd[256]) {
    vector<pair<int,int>> sv;
    for (int i = 0; i < 256; i++)
        if (ln[i] > 0) sv.push_back({ln[i], i});
    sort(sv.begin(), sv.end());

    unsigned int c = 0;
    int pl = 0;
    for (auto &x : sv) {
        int len = x.first, sym = x.second;
        c <<= (len - pl); // Shift when moving to longer code lengths.
        cd[sym] = c++;
        pl = len;
    }
}

    // Pack bytes into a bitstream (MSB first).
static void encBits(const char *d, int dlen,
                    int ln[256], unsigned int cd[256],
                    vector<char> &pb, int &pad) {
    int cur = 0, cnt = 0;
    for (int i = 0; i < dlen; i++) {
        int s = (unsigned char)d[i];
        int len = ln[s];
        unsigned int c = cd[s];
        for (int b = len - 1; b >= 0; b--) {
            cur = (cur << 1) | ((c >> b) & 1);
            if (++cnt == 8) { pb.push_back((char)cur); cur = 0; cnt = 0; }
        }
    }
    pad = 0;
    if (cnt > 0) {
        pad = 8 - cnt;
        pb.push_back((char)(cur << pad)); // Pad remaining bits with zeros.
    }
}

    // Write header format: [N][symbols][lengths][padding].
static void wHdr(int ln[256], int pad, string &out) {
    vector<int> sv;
    for (int i = 0; i < 256; i++)
        if (ln[i] > 0) sv.push_back(i);
    int N = (int)sv.size();
    out.push_back((char)N);
    for (int i = 0; i < N; i++) out.push_back((char)sv[i]);
    for (int i = 0; i < N; i++) out.push_back((char)ln[sv[i]]);
    out.push_back((char)pad);
}

// Parse header and fill lengths/padding.
static int rHdr(const string &s, int pos, int ln[256], int &pad) {
    if (pos >= (int)s.size()) return -1;

    int st = pos;
    int N = (unsigned char)s[pos++];

    if (N < 0 || N > 256) return -1;
    if (pos + N + N + 1 > (int)s.size()) return -1;

    int sym[256] = {};
    for (int i = 0; i < N; i++) sym[i] = (unsigned char)s[pos++];
    for (int i = 0; i < N; i++) ln[sym[i]] = (unsigned char)s[pos++];
    pad = (unsigned char)s[pos++];
    if (pad < 0 || pad > 7) return -1;

    return pos - st;
}

// Decode compressed bits back to original bytes.
static string decBits(const string &s, int ds, int de,
                       int pad, int ln[256], unsigned int cd[256]) {
    unordered_map<unsigned int, int> tbl;
    int mx = 0;
    for (int i = 0; i < 256; i++) {
        if (ln[i] > 0) {
            unsigned int k = (cd[i] << 5) | (unsigned int)ln[i];
            tbl[k] = i;
            if (ln[i] > mx) mx = ln[i];
        }
    }

    int tot = (de - ds) * 8 - pad; // Number of valid data bits.
    int br = 0;
    unsigned int buf = 0;
    int bb = 0, si = ds;
    string res;

    while (br < tot) {
        // Load more bits into the decode buffer.
        while (bb < mx && si < de) {
            buf = (buf << 8) | (unsigned char)s[si++];
            bb += 8;
        }
        bool ok = false;
        int lim = (bb < mx) ? bb : mx;
        for (int len = 1; len <= lim; len++) {
            if (br + len > tot) break;
            unsigned int cand = buf >> (bb - len);
            unsigned int k = (cand << 5) | (unsigned int)len;
            auto it = tbl.find(k);
            if (it != tbl.end()) {
                res.push_back((char)it->second);
                bb -= len;
                buf &= (1u << bb) - 1;
                br += len;
                ok = true;
                break;
            }
        }
        if (!ok) break; // No match found, likely corrupted data.
    }
    return res;
}

    // Compress complete input using a single Huffman model.
string globalHuffmanCompress(const string &d) {
    int fr[256] = {};
    for (int i = 0; i < (int)d.size(); i++)
        fr[(unsigned char)d[i]]++;

    CNode *root = buildTree(fr);
    if (!root) return "";

    int ln[256] = {};
    getLen(root, 0, ln);
    freeTree(root);

    unsigned int cd[256] = {};
    mkCodes(ln, cd);

    vector<char> pb;
    int pad = 0;
    encBits(d.data(), (int)d.size(), ln, cd, pb, pad);

    // 'G' marks global mode.
    string out;
    out.push_back('G');
    wHdr(ln, pad, out);
    for (char c : pb) out.push_back(c);
    return out;
}

// Write 32-bit integer as 4 bytes (big-endian).
static void wI32(string &out, int v) {
    out.push_back((char)((v >> 24) & 0xFF));
    out.push_back((char)((v >> 16) & 0xFF));
    out.push_back((char)((v >>  8) & 0xFF));
    out.push_back((char)( v        & 0xFF));
}

// Read 32-bit integer from 4 bytes (big-endian).
static int rI32(const string &s, int p) {
    if (p < 0 || p + 3 >= (int)s.size()) return -1;

    return ((unsigned char)s[p]   << 24)
         | ((unsigned char)s[p+1] << 16)
         | ((unsigned char)s[p+2] <<  8)
         |  (unsigned char)s[p+3];
}

// Split input into 32 KB blocks and compress each independently.
string blockSplitCompress(const string &d) {
    int n = (int)d.size();
    int nb = (n + BLK_SZ - 1) / BLK_SZ; // Number of blocks.

    string out;
    out.push_back('B'); // 'B' marks block mode.
    wI32(out, nb);

    for (int b = 0; b < nb; b++) {
        int bs = b * BLK_SZ;
        int bl = (bs + BLK_SZ <= n) ? BLK_SZ : (n - bs); // Last block can be shorter.

        int fr[256] = {};
        for (int i = bs; i < bs + bl; i++)
            fr[(unsigned char)d[i]]++;

        CNode *root = buildTree(fr);
        int ln[256] = {};
        getLen(root, 0, ln);
        freeTree(root);

        unsigned int cd[256] = {};
        mkCodes(ln, cd);

        vector<char> pb;
        int pad = 0;
        encBits(d.data() + bs, bl, ln, cd, pb, pad);

        string bk;
        wHdr(ln, pad, bk);
        for (char c : pb) bk.push_back(c);

        wI32(out, (int)bk.size());
        out += bk;
    }
    return out;
}

// Compress by selecting the best strategy for current input.
BlockResult blockHuffmanCompress(const string &d) {
    if (d.empty()) return {"", {}};

    // Fast path: all bytes are identical.
    {
        bool same = true;
        for (int i = 1; i < (int)d.size(); i++)
            if (d[i] != d[0]) { same = false; break; }
        if (same) {
            // Single symbol stream: a 1-bit code is sufficient.
            string out;
            out.push_back('G');
            int ln[256] = {};
            ln[(unsigned char)d[0]] = 1;
            unsigned int cd[256] = {};
            vector<char> pb;
            int pad = 0;
            encBits(d.data(), (int)d.size(), ln, cd, pb, pad);
            wHdr(ln, pad, out);
            for (char c : pb) out.push_back(c);
            return {out, {}};
        }
    }

    int n = (int)d.size();

    // Small input: global mode is usually better.
    if (n < SML_THRESH)
        return {globalHuffmanCompress(d), {}};

    // Try both and keep the smaller output.
    string g = globalHuffmanCompress(d);
    string bk = blockSplitCompress(d);

    if ((int)g.size() <= (int)bk.size())
        return {g, {}};
    return {bk, {}};
}

// Decompress by reading mode from the first byte.
string blockHuffmanDecompress(const string &enc,
                               const unordered_map<string, string> &) {
    if (enc.empty()) return "";

    char mode = enc[0]; // 'G' or 'B'

    if (mode == 'G') {
        int pos = 1;
        int ln[256] = {};
        int pad = 0;
        int used = rHdr(enc, pos, ln, pad);
        if (used <= 0) return "";
        int dstart = pos + used;
        if (dstart > (int)enc.size()) return "";

        unsigned int cd[256] = {};
        mkCodes(ln, cd);

        // Single-symbol edge case.
        if ((int)enc.size() < 5) return "";
        int N = (unsigned char)enc[1];
        if (N == 1) {
            int sym = (unsigned char)enc[2];
            int p   = (unsigned char)enc[4];
            int tb  = ((int)enc.size() - dstart) * 8 - p;
            if (tb < 0) return "";
            return string(tb, (char)sym);
        }

        return decBits(enc, dstart, (int)enc.size(), pad, ln, cd);
    }

    if (mode == 'B') {
        int pos = 1;
        int nb = rI32(enc, pos); pos += 4;
        if (nb < 0) return "";
        string res;

        for (int b = 0; b < nb; b++) {
            int bsz = rI32(enc, pos); pos += 4;
            if (bsz < 0) return "";
            int bend = pos + bsz;
            if (bend > (int)enc.size()) return "";

            int ln[256] = {};
            int pad = 0;
            int used = rHdr(enc, pos, ln, pad);
            if (used <= 0) return "";
            int dstart = pos + used;
            if (dstart > bend) return "";

            unsigned int cd[256] = {};
            mkCodes(ln, cd);

            res += decBits(enc, dstart, bend, pad, ln, cd);
            pos = bend;
        }
        return res;
    }

    return ""; // Unknown mode.
}

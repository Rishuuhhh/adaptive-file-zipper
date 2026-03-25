#include "block_huffman.h"
#include <queue>
#include <vector>
#include <algorithm>

using namespace std;

// 4KB se chhota input ho toh global huffman use karo
static const int SML_THRESH = 4 * 1024;
// har block 32KB ka hoga
static const int BLK_SZ = 32 * 1024;

// huffman tree ka ek node
struct CNode {
    int sym;  // -1 = internal, 0-255 = leaf byte
    int frq;  // kitni baar aaya
    CNode *l, *r;
    CNode(int s, int f) : sym(s), frq(f), l(0), r(0) {}
};

// min-heap ke liye: kam frequency wala pehle
struct CNodeCmp {
    bool operator()(CNode *a, CNode *b) const { return a->frq > b->frq; }
};

// frequency array se huffman tree banao
static CNode* buildTree(int fr[256]) {
    priority_queue<CNode*, vector<CNode*>, CNodeCmp> pq;
    for (int i = 0; i < 256; i++)
        if (fr[i] > 0) pq.push(new CNode(i, fr[i]));
    if (pq.empty()) return 0;
    // do chhote nodes ko merge karte raho
    while ((int)pq.size() > 1) {
        CNode *a = pq.top(); pq.pop();
        CNode *b = pq.top(); pq.pop();
        CNode *p = new CNode(-1, a->frq + b->frq);
        p->l = a; p->r = b;
        pq.push(p);
    }
    return pq.top();
}

// tree ki memory free karo
static void freeTree(CNode *n) {
    if (!n) return;
    freeTree(n->l); freeTree(n->r);
    delete n;
}

// tree traverse karke har symbol ki bit length nikalo
static void getLen(CNode *n, int dep, int ln[256]) {
    if (!n) return;
    if (n->sym >= 0) {
        ln[n->sym] = (dep == 0) ? 1 : dep; // single symbol ho toh 1 bit
        return;
    }
    getLen(n->l, dep + 1, ln);
    getLen(n->r, dep + 1, ln);
}

// lengths se canonical huffman codes banao
static void mkCodes(int ln[256], unsigned int cd[256]) {
    vector<pair<int,int>> sv;
    for (int i = 0; i < 256; i++)
        if (ln[i] > 0) sv.push_back({ln[i], i});
    sort(sv.begin(), sv.end());

    unsigned int c = 0;
    int pl = 0;
    for (auto &x : sv) {
        int len = x.first, sym = x.second;
        c <<= (len - pl); // length badhi toh shift karo
        cd[sym] = c++;
        pl = len;
    }
}

// data ko bits mein pack karo (MSB first)
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
        pb.push_back((char)(cur << pad)); // bacha hua byte, zeros se fill
    }
}

// header likho: [N][symbols][lengths][padding]
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

// header padho, lengths aur padding fill karo
static int rHdr(const string &s, int pos, int ln[256], int &pad) {
    int st = pos;
    int N = (unsigned char)s[pos++];
    int sym[256] = {};
    for (int i = 0; i < N; i++) sym[i] = (unsigned char)s[pos++];
    for (int i = 0; i < N; i++) ln[sym[i]] = (unsigned char)s[pos++];
    pad = (unsigned char)s[pos++];
    return pos - st;
}

// compressed bits ko wapas original bytes mein convert karo
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

    int tot = (de - ds) * 8 - pad; // valid bits ki count
    int br = 0;
    unsigned int buf = 0;
    int bb = 0, si = ds;
    string res;

    while (br < tot) {
        // buffer mein bits load karo
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
        if (!ok) break; // match nahi mila, data kharab ho sakta hai
    }
    return res;
}

// poore input ko ek huffman tree se compress karo
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

    // 'G' = global mode
    string out;
    out.push_back('G');
    wHdr(ln, pad, out);
    for (char c : pb) out.push_back(c);
    return out;
}

// int ko 4 bytes mein likho (big-endian)
static void wI32(string &out, int v) {
    out.push_back((char)((v >> 24) & 0xFF));
    out.push_back((char)((v >> 16) & 0xFF));
    out.push_back((char)((v >>  8) & 0xFF));
    out.push_back((char)( v        & 0xFF));
}

// 4 bytes se int padho (big-endian)
static int rI32(const string &s, int p) {
    return ((unsigned char)s[p]   << 24)
         | ((unsigned char)s[p+1] << 16)
         | ((unsigned char)s[p+2] <<  8)
         |  (unsigned char)s[p+3];
}

// input ko 32KB blocks mein tod ke har block alag compress karo
string blockSplitCompress(const string &d) {
    int n = (int)d.size();
    int nb = (n + BLK_SZ - 1) / BLK_SZ; // kitne blocks

    string out;
    out.push_back('B'); // 'B' = block mode
    wI32(out, nb);

    for (int b = 0; b < nb; b++) {
        int bs = b * BLK_SZ;
        int bl = (bs + BLK_SZ <= n) ? BLK_SZ : (n - bs); // last block chhota ho sakta hai

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

// compress karo — input size ke hisaab se best method choose karo
BlockResult blockHuffmanCompress(const string &d) {
    if (d.empty()) return {"", {}};

    // sab bytes same hain kya
    {
        bool same = true;
        for (int i = 1; i < (int)d.size(); i++)
            if (d[i] != d[0]) { same = false; break; }
        if (same) {
            // sirf ek symbol, 1 bit kaafi hai
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

    // chhota input, global hi theek hai
    if (n < SML_THRESH)
        return {globalHuffmanCompress(d), {}};

    // dono try karo, jo chhota ho woh lo
    string g = globalHuffmanCompress(d);
    string bk = blockSplitCompress(d);

    if ((int)g.size() <= (int)bk.size())
        return {g, {}};
    return {bk, {}};
}

// decompress karo — pehla byte dekh ke mode pata karo
string blockHuffmanDecompress(const string &enc,
                               const unordered_map<string, string> &) {
    if (enc.empty()) return "";

    char mode = enc[0]; // 'G' ya 'B'

    if (mode == 'G') {
        int pos = 1;
        int ln[256] = {};
        int pad = 0;
        int used = rHdr(enc, pos, ln, pad);
        int dstart = pos + used;

        unsigned int cd[256] = {};
        mkCodes(ln, cd);

        // single symbol edge case
        int N = (unsigned char)enc[1];
        if (N == 1) {
            int sym = (unsigned char)enc[2];
            int p   = (unsigned char)enc[4];
            int tb  = ((int)enc.size() - dstart) * 8 - p;
            return string(tb, (char)sym);
        }

        return decBits(enc, dstart, (int)enc.size(), pad, ln, cd);
    }

    if (mode == 'B') {
        int pos = 1;
        int nb = rI32(enc, pos); pos += 4;
        string res;

        for (int b = 0; b < nb; b++) {
            int bsz = rI32(enc, pos); pos += 4;
            int bend = pos + bsz;

            int ln[256] = {};
            int pad = 0;
            int used = rHdr(enc, pos, ln, pad);
            int dstart = pos + used;

            unsigned int cd[256] = {};
            mkCodes(ln, cd);

            res += decBits(enc, dstart, bend, pad, ln, cd);
            pos = bend;
        }
        return res;
    }

    return ""; // unknown mode
}

#include "file_io.h"
#include <fstream>
#include <sstream>

using namespace std;

// Read a file in binary mode.
string readFile(const string &fn) {
    ifstream f(fn, ios::binary);
    stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

// Write a file in binary mode.
void writeFile(const string &fn, const string &d) {
    ofstream f(fn, ios::binary);
    f << d;
}

// Serialize codeMap to a text representation.
string serializeCodeMap(const unordered_map<string, string> &cm) {
    string res;
    for (auto &e : cm) {
        int kl = (int)e.first.size();
        int vl = (int)e.second.size();
        res += to_string(kl) + ":" + e.first + " "
             + to_string(vl) + ":" + e.second + "\n";
    }
    return res;
}

// Rebuild codeMap from serialized text.
unordered_map<string, string> deserializeCodeMap(const string &s) {
    unordered_map<string, string> cm;
    int pos = 0, n = (int)s.size();

    while (pos < n) {
        int cl = (int)s.find(':', pos);
        if (cl == (int)string::npos) break;
        int kl = stoi(s.substr(pos, cl - pos));
        pos = cl + 1;
        string k = s.substr(pos, kl);
        pos += kl;

        if (pos >= n || s[pos] != ' ') break;
        pos++;

        cl = (int)s.find(':', pos);
        if (cl == (int)string::npos) break;
        int vl = stoi(s.substr(pos, cl - pos));
        pos = cl + 1;
        string v = s.substr(pos, vl);
        pos += vl;

        if (pos < n && s[pos] == '\n') pos++;
        cm[k] = v;
    }
    return cm;
}

// Pack compressed payload with metadata header.
string packData(const string &meth, double ent,
                const string &cms, const string &pay) {
    return meth + "\n"
         + to_string(ent) + "\n"
         + to_string((int)cms.size()) + "\n"
         + cms
         + pay;
}

// Unpack method, entropy, codemap, and payload from packed data.
void unpackData(const string &in, string &meth, double &ent,
                string &cms, string &pay) {
    int pos = 0, n = (int)in.size();

    int nl = (int)in.find('\n', pos);
    if (nl == (int)string::npos) return;
    meth = in.substr(pos, nl - pos);
    pos = nl + 1;

    nl = (int)in.find('\n', pos);
    if (nl == (int)string::npos) return;
    ent = stod(in.substr(pos, nl - pos));
    pos = nl + 1;

    nl = (int)in.find('\n', pos);
    if (nl == (int)string::npos) return;
    int cml = stoi(in.substr(pos, nl - pos));
    pos = nl + 1;

    cms = in.substr(pos, cml);
    pos += cml;

    pay = in.substr(pos);
}

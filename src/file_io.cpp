#include "file_io.h"
#include <fstream>
#include <sstream>
#include <iomanip>
using namespace std;
string readFile(const string &fn) {
    ifstream f(fn, ios::binary);
    stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}
void writeFile(const string &fn, const string &d) {
    ofstream f(fn, ios::binary);
    f << d;
}
string serializeCodeMap(const unordered_map<string, string> &cm) {
    string res;
    for (auto &e : cm) {
        size_t kl = e.first.size();
        size_t vl = e.second.size();
        res += to_string(kl) + ":" + e.first + " "
             + to_string(vl) + ":" + e.second + "\n";
    }
    return res;
}
unordered_map<string, string> deserializeCodeMap(const string &s) {
    unordered_map<string, string> cm;
    size_t pos = 0, n = s.size();
    while (pos < n) {
        size_t cl = s.find(':', pos);
        if (cl == string::npos) break;
        if (cl >= n) break;
        int kl = stoi(s.substr(pos, cl - pos));
        if (kl < 0) break;
        pos = cl + 1;
        if (pos + kl > n) break;
        string k = s.substr(pos, kl);
        pos += kl;
        if (pos >= n || s[pos] != ' ') break;
        pos++;
        cl = s.find(':', pos);
        if (cl == string::npos) break;
        if (cl >= n) break;
        int vl = stoi(s.substr(pos, cl - pos));
        if (vl < 0) break;
        pos = cl + 1;
        if (pos + vl > n) break;
        string v = s.substr(pos, vl);
        pos += vl;
        if (pos < n && s[pos] == '\n') pos++;
        cm[k] = v;
    }
    return cm;
}
string packData(const string &meth, double ent,
                const string &cms, const string &pay) {
    ostringstream oss;
    oss << setprecision(17) << ent;
    return meth + "\n"
         + oss.str() + "\n"
         + to_string((int)cms.size()) + "\n"
         + cms
         + pay;
}
void unpackData(const string &in, string &meth, double &ent,
                string &cms, string &pay) {
    size_t pos = 0, n = in.size();
    size_t nl = in.find('\n', pos);
    if (nl == string::npos) return;
    meth = in.substr(pos, nl - pos);
    pos = nl + 1;
    if (pos > n) return;
    nl = in.find('\n', pos);
    if (nl == string::npos) return;
    if (nl >= n) return;
    ent = stod(in.substr(pos, nl - pos));
    pos = nl + 1;
    if (pos > n) return;
    nl = in.find('\n', pos);
    if (nl == string::npos) return;
    if (nl >= n) return;
    int cml = stoi(in.substr(pos, nl - pos));
    if (cml < 0) return;
    pos = nl + 1;
    if (pos > n) return;
    if (pos + (size_t)cml > n) return;
    cms = in.substr(pos, cml);
    pos += cml;
    pay = in.substr(pos);
}
void deserializeCompressedData(const string &input, string &method, double &entropy,
                              string &codeMapData, string &payload) {
    string ignored;
    deserializeCompressedData(input, method, entropy, codeMapData, payload, ignored);
}

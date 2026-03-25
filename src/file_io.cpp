#include "file_io.h"
#include <fstream>
#include <sstream>

using namespace std;

string readFile(const string &filename) {
    ifstream file(filename, ios::binary);
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const string &filename, const string &data) {
    ofstream file(filename, ios::binary);
    file << data;
}

// Serialize codeMap (string->string) to text format
string serializeCodeMap(const unordered_map<string, string> &codeMap) {
    string result;
    for (auto &entry : codeMap) {
        int klen = (int)entry.first.size();
        int vlen = (int)entry.second.size();
        result += to_string(klen) + ":" + entry.first + " "
                + to_string(vlen) + ":" + entry.second + "\n";
    }
    return result;
}

unordered_map<string, string> deserializeCodeMap(const string &serialized) {
    unordered_map<string, string> codeMap;
    int pos = 0;
    int n = (int)serialized.size();

    while (pos < n) {
        // Read key length
        int colon = (int)serialized.find(':', pos);
        if (colon == (int)string::npos) break;
        int keyLen = stoi(serialized.substr(pos, colon - pos));
        pos = colon + 1;
        string key = serialized.substr(pos, keyLen);
        pos += keyLen;

        if (pos >= n || serialized[pos] != ' ') break;
        pos++;

        // Read value length
        colon = (int)serialized.find(':', pos);
        if (colon == (int)string::npos) break;
        int valLen = stoi(serialized.substr(pos, colon - pos));
        pos = colon + 1;
        string value = serialized.substr(pos, valLen);
        pos += valLen;

        if (pos < n && serialized[pos] == '\n') pos++;
        codeMap[key] = value;
    }
    return codeMap;
}

// Pack compressed data with header: method, entropy, codeMap size, codeMap, payload
string packData(const string &method, double entropy,
                const string &codeMapSerialized, const string &payload) {
    return method + "\n"
         + to_string(entropy) + "\n"
         + to_string((int)codeMapSerialized.size()) + "\n"
         + codeMapSerialized
         + payload;
}

void unpackData(const string &input, string &method, double &entropy,
                string &codeMapSerialized, string &payload) {
    int pos = 0;
    int n = (int)input.size();

    // method
    int nl = (int)input.find('\n', pos);
    if (nl == (int)string::npos) return;
    method = input.substr(pos, nl - pos);
    pos = nl + 1;

    // entropy
    nl = (int)input.find('\n', pos);
    if (nl == (int)string::npos) return;
    entropy = stod(input.substr(pos, nl - pos));
    pos = nl + 1;

    // codeMap length
    nl = (int)input.find('\n', pos);
    if (nl == (int)string::npos) return;
    int codeMapLen = stoi(input.substr(pos, nl - pos));
    pos = nl + 1;

    codeMapSerialized = input.substr(pos, codeMapLen);
    pos += codeMapLen;

    payload = input.substr(pos);
}

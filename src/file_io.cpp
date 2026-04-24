#include "file_io.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace std;

string readFile(const string &filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Could not open file for reading: " + filename);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const string &filename, const string &content) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Could not open file for writing: " + filename);
    }

    file << content;
}

string serializeCodeMap(const unordered_map<string, string> &codeMap) {
    string result;
    for (const auto &entry : codeMap) {
        result += to_string(entry.first.size()) + ":" + entry.first + " ";
        result += to_string(entry.second.size()) + ":" + entry.second + "\n";
    }
    return result;
}

unordered_map<string, string> deserializeCodeMap(const string &serialized) {
    unordered_map<string, string> codeMap;
    int pos = 0;
    int total = (int)serialized.size();

    while (pos < total) {
        int colon = (int)serialized.find(':', pos);
        if (colon == (int)string::npos) break;

        int keyLen = stoi(serialized.substr(pos, colon - pos));
        pos = colon + 1;

        string key = serialized.substr(pos, keyLen);
        pos += keyLen;

        if (pos >= total || serialized[pos] != ' ') break;
        pos++;

        colon = (int)serialized.find(':', pos);
        if (colon == (int)string::npos) break;

        int valLen = stoi(serialized.substr(pos, colon - pos));
        pos = colon + 1;

        string value = serialized.substr(pos, valLen);
        pos += valLen;

        if (pos < total && serialized[pos] == '\n') pos++;

        codeMap[key] = value;
    }

    return codeMap;
}

static string sanitizeFilename(const string &name) {
    string safe = name;

    size_t lastSlash = safe.find_last_of("/\\");
    if (lastSlash != string::npos) {
        safe = safe.substr(lastSlash + 1);
    }

    while (!safe.empty() && safe[0] == '.') {
        safe.erase(safe.begin());
    }

    safe.erase(remove(safe.begin(), safe.end(), '\n'), safe.end());

    return safe;
}

string serializeCompressedData(const string &method, double entropy,
                               const string &codeMapData,
                               const string &payload,
                               const string &originalFilename) {
    string safeName = sanitizeFilename(originalFilename);

    return method + "\n"
         + to_string(entropy) + "\n"
         + to_string((int)codeMapData.size()) + "\n"
         + safeName + "\n"
         + codeMapData
         + payload;
}

void deserializeCompressedData(const string &blob, string &method,
                               double &entropy, string &codeMapData,
                               string &payload, string &originalFilename) {
    int pos = 0;
    int blobSize = (int)blob.size();

    auto readLine = [&](const string &fieldName) -> string {
        int nl = (int)blob.find('\n', pos);
        if (nl == (int)string::npos) {
            throw runtime_error("Malformed .z file: missing " + fieldName);
        }
        string line = blob.substr(pos, nl - pos);
        pos = nl + 1;
        return line;
    };

    method = readLine("method tag");

    string entropyStr = readLine("entropy");
    if (entropyStr.empty()) {
        throw runtime_error("Malformed .z file: entropy field is empty");
    }
    try {
        entropy = stod(entropyStr);
    } catch (...) {
        throw runtime_error("Malformed .z file: entropy is not a number: " + entropyStr);
    }

    string codeMapLenStr = readLine("codeMapLength");
    int codeMapLen = 0;
    try {
        codeMapLen = stoi(codeMapLenStr);
    } catch (...) {
        throw runtime_error("Malformed .z file: codeMapLength is not a number");
    }
    if (codeMapLen < 0) {
        throw runtime_error("Malformed .z file: codeMapLength is negative");
    }

    originalFilename = "";
    int filenameNewline = (int)blob.find('\n', pos);
    if (filenameNewline != (int)string::npos) {
        int afterFilename = filenameNewline + 1;
        if (afterFilename + codeMapLen <= blobSize) {
            originalFilename = blob.substr(pos, filenameNewline - pos);
            pos = afterFilename;
        }
    }

    if (pos + codeMapLen > blobSize) {
        throw runtime_error("Malformed .z file: code map extends past end of file");
    }
    codeMapData = blob.substr(pos, codeMapLen);
    pos += codeMapLen;

    payload = blob.substr(pos);
}

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

string serializeCodeMap(const unordered_map<string, string> &codeMap) {
    string result;
    for (const auto &entry : codeMap) {
        int keyLength = (int)entry.first.size();
        int valueLength = (int)entry.second.size();
        result += to_string(keyLength) + ":" + entry.first + " "
                + to_string(valueLength) + ":" + entry.second + "\n";
    }
    return result;
}

unordered_map<string, string> deserializeCodeMap(const string &serialized) {
    unordered_map<string, string> codeMap;
    int position = 0;
    int n = (int)serialized.size();

    while (position < n) {
        int colonPos = (int)serialized.find(':', position);
        if (colonPos == (int)string::npos) break;
        int keyLength = stoi(serialized.substr(position, colonPos - position));
        position = colonPos + 1;

        string key = serialized.substr(position, keyLength);
        position += keyLength;

        if (position >= n || serialized[position] != ' ') break;
        position++;

        colonPos = (int)serialized.find(':', position);
        if (colonPos == (int)string::npos) break;
        int valueLength = stoi(serialized.substr(position, colonPos - position));
        position = colonPos + 1;

        string value = serialized.substr(position, valueLength);
        position += valueLength;

        if (position < n && serialized[position] == '\n') position++;

        codeMap[key] = value;
    }

    return codeMap;
}

string serializeCompressedData(const string &method, double entropy,
                              const string &serializedCodeMap,
                              const string &payload) {
    return method + "\n"
         + to_string(entropy) + "\n"
         + to_string((int)serializedCodeMap.size()) + "\n"
         + serializedCodeMap
         + payload;
}

void deserializeCompressedData(const string &input, string &method,
                              double &entropy, string &serializedCodeMap,
                              string &payload) {
    int position = 0;

    int newlinePos = (int)input.find('\n', position);
    if (newlinePos == (int)string::npos) return;
    method = input.substr(position, newlinePos - position);
    position = newlinePos + 1;

    newlinePos = (int)input.find('\n', position);
    if (newlinePos == (int)string::npos) return;
    entropy = stod(input.substr(position, newlinePos - position));
    position = newlinePos + 1;

    newlinePos = (int)input.find('\n', position);
    if (newlinePos == (int)string::npos) return;
    int codeMapLength = stoi(input.substr(position, newlinePos - position));
    position = newlinePos + 1;

    serializedCodeMap = input.substr(position, codeMapLength);
    position += codeMapLength;
    payload = input.substr(position);
}
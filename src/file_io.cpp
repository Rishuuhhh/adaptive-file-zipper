#include "file_io.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

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
                              const string &payload,
                              const string &originalFilename) {
    // Sanitize originalFilename: strip any path components and leading slashes
    // to prevent path traversal when the filename is later used for output.
    string safeName = originalFilename;
    // Remove any directory separators
    auto lastSlash = safeName.find_last_of("/\\");
    if (lastSlash != string::npos)
        safeName = safeName.substr(lastSlash + 1);
    // Strip leading dots that could form relative paths like "../foo"
    while (!safeName.empty() && safeName[0] == '.')
        safeName = safeName.substr(1);
    // Remove any embedded newlines (they would break the header format)
    safeName.erase(std::remove(safeName.begin(), safeName.end(), '\n'), safeName.end());

    return method + "\n"
         + to_string(entropy) + "\n"
         + to_string((int)serializedCodeMap.size()) + "\n"
         + safeName + "\n"
         + serializedCodeMap
         + payload;
}

void deserializeCompressedData(const string &input, string &method,
                              double &entropy, string &serializedCodeMap,
                              string &payload, string &originalFilename) {
    int position = 0;
    int inputSize = (int)input.size();

    // Parse method tag
    int newlinePos = (int)input.find('\n', position);
    if (newlinePos == (int)string::npos)
        throw std::runtime_error("Malformed Serialized_Blob: missing method tag newline");
    method = input.substr(position, newlinePos - position);
    position = newlinePos + 1;

    // Parse entropy
    newlinePos = (int)input.find('\n', position);
    if (newlinePos == (int)string::npos)
        throw std::runtime_error("Malformed Serialized_Blob: missing entropy newline");
    {
        string entropyStr = input.substr(position, newlinePos - position);
        if (entropyStr.empty())
            throw std::runtime_error("Malformed Serialized_Blob: entropy field is empty");
        try {
            entropy = stod(entropyStr);
        } catch (const std::exception &) {
            throw std::runtime_error("Malformed Serialized_Blob: entropy field is not a valid number: \"" + entropyStr + "\"");
        }
    }
    position = newlinePos + 1;

    // Parse codeMapLength
    newlinePos = (int)input.find('\n', position);
    if (newlinePos == (int)string::npos)
        throw std::runtime_error("Malformed Serialized_Blob: missing codeMapLength newline");
    int codeMapLength = 0;
    {
        string codeMapLenStr = input.substr(position, newlinePos - position);
        if (codeMapLenStr.empty())
            throw std::runtime_error("Malformed Serialized_Blob: codeMapLength field is empty");
        try {
            codeMapLength = stoi(codeMapLenStr);
        } catch (const std::exception &) {
            throw std::runtime_error("Malformed Serialized_Blob: codeMapLength field is not a valid integer: \"" + codeMapLenStr + "\"");
        }
        if (codeMapLength < 0)
            throw std::runtime_error("Malformed Serialized_Blob: codeMapLength is negative");
    }
    position = newlinePos + 1;

    // Try to read the 4th header line as originalFilename (new format).
    // If consuming it still leaves enough bytes for the codeMap, use it.
    // Otherwise treat originalFilename as "" and stay at current position (old format).
    originalFilename = "";
    int posAfterFilename = position;
    int filenameNewline = (int)input.find('\n', position);
    if (filenameNewline != (int)string::npos) {
        int posCandidate = filenameNewline + 1;
        // Check that the codeMap would fit after this candidate position
        if (posCandidate + codeMapLength <= inputSize) {
            originalFilename = input.substr(position, filenameNewline - position);
            posAfterFilename = posCandidate;
        }
        // else: old 3-line format — leave originalFilename="" and posAfterFilename=position
    }
    position = posAfterFilename;

    // Verify the code map region is within bounds before slicing
    if (position + codeMapLength > inputSize)
        throw std::runtime_error("Malformed Serialized_Blob: codeMapLength (" +
                                 to_string(codeMapLength) +
                                 ") extends beyond end of input (available: " +
                                 to_string(inputSize - position) + " bytes)");

    serializedCodeMap = input.substr(position, codeMapLength);
    position += codeMapLength;
    payload = input.substr(position);
}
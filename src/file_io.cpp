#include "file_io.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace {

int findNextLineBreak(const string &input, int position, const string &fieldName) {
    int newlinePos = static_cast<int>(input.find('\n', position));
    if (newlinePos == static_cast<int>(string::npos)) {
        throw runtime_error("Malformed Serialized_Blob: missing " + fieldName + " newline");
    }
    return newlinePos;
}

string readHeaderLine(const string &input, int &position, const string &fieldName) {
    int newlinePos = findNextLineBreak(input, position, fieldName);
    string value = input.substr(position, newlinePos - position);
    position = newlinePos + 1;
    return value;
}

double parseEntropyValue(const string &entropyString) {
    if (entropyString.empty()) {
        throw runtime_error("Malformed Serialized_Blob: entropy field is empty");
    }

    try {
        return stod(entropyString);
    } catch (const exception &) {
        throw runtime_error(
            "Malformed Serialized_Blob: entropy field is not a valid number: \""
            + entropyString + "\""
        );
    }
}

int parseCodeMapLength(const string &lengthString) {
    if (lengthString.empty()) {
        throw runtime_error("Malformed Serialized_Blob: codeMapLength field is empty");
    }

    try {
        int parsedLength = stoi(lengthString);
        if (parsedLength < 0) {
            throw runtime_error("Malformed Serialized_Blob: codeMapLength is negative");
        }
        return parsedLength;
    } catch (const runtime_error &) {
        throw;
    } catch (const exception &) {
        throw runtime_error(
            "Malformed Serialized_Blob: codeMapLength field is not a valid integer: \""
            + lengthString + "\""
        );
    }
}

string sanitizeFilenameForHeader(const string &originalFilename) {
    string safeName = originalFilename;

    // Keep only basename so extraction never recreates a path outside result dir.
    auto lastSlash = safeName.find_last_of("/\\");
    if (lastSlash != string::npos) {
        safeName = safeName.substr(lastSlash + 1);
    }

    // Trim leading dots to avoid accidental hidden/relative names.
    while (!safeName.empty() && safeName[0] == '.') {
        safeName.erase(safeName.begin());
    }

    // Newlines would break our one-line header format.
    safeName.erase(remove(safeName.begin(), safeName.end(), '\n'), safeName.end());
    return safeName;
}

} // namespace

string readFile(const string &filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Could not open file for reading: " + filename);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const string &filename, const string &data) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Could not open file for writing: " + filename);
    }

    file << data;
    if (!file.good()) {
        throw runtime_error("Failed while writing file: " + filename);
    }
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
    string safeName = sanitizeFilenameForHeader(originalFilename);

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
    int inputSize = static_cast<int>(input.size());

    method = readHeaderLine(input, position, "method tag");
    entropy = parseEntropyValue(readHeaderLine(input, position, "entropy"));
    int codeMapLength = parseCodeMapLength(readHeaderLine(input, position, "codeMapLength"));

    // Try to read the 4th header line as originalFilename (new format).
    // If consuming it still leaves enough bytes for the codeMap, use it.
    // Otherwise treat originalFilename as "" and stay at current position (old format).
    originalFilename = "";
    int posAfterFilename = position;
    int filenameNewline = static_cast<int>(input.find('\n', position));
    if (filenameNewline != static_cast<int>(string::npos)) {
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
    if (position + codeMapLength > inputSize) {
        throw runtime_error("Malformed Serialized_Blob: codeMapLength (" +
                            to_string(codeMapLength) +
                            ") extends beyond end of input (available: " +
                            to_string(inputSize - position) + " bytes)");
    }

    serializedCodeMap = input.substr(position, codeMapLength);
    position += codeMapLength;
    payload = input.substr(position);
}
#ifndef FILE_IO_H
#define FILE_IO_H

#include <string>
#include <unordered_map>

/* Read the entire contents of a file in binary mode and return them as a string. */
std::string readFile(const std::string &filename);

/* Write data to a file in binary mode, overwriting any existing content. */
void writeFile(const std::string &filename, const std::string &data);

/* Serialize a code map (symbol → bit-string) to a human-readable text format.
   Each entry is written as: keyLen:key valueLen:value\n */
std::string serializeCodeMap(const std::unordered_map<std::string, std::string> &codeMap);

/* Deserialize a code map previously produced by serializeCodeMap. */
std::unordered_map<std::string, std::string> deserializeCodeMap(const std::string &serialized);

/* Pack a compressed payload together with its metadata (method tag, entropy value,
   optional serialized code map, and optional original filename) into a single binary
   blob suitable for writing to a .z file. */
std::string serializeCompressedData(const std::string &method, double entropy,
                                    const std::string &serializedCodeMap,
                                    const std::string &payload,
                                    const std::string &originalFilename = "");

/* Unpack a binary blob produced by serializeCompressedData, recovering the method
   tag, entropy value, serialized code map, compressed payload, and original filename.
   Backward compatible: old blobs without the originalFilename line return "". */
void deserializeCompressedData(const std::string &input, std::string &method,
                               double &entropy, std::string &serializedCodeMap,
                               std::string &payload,
                               std::string &originalFilename);

/* Overload without originalFilename for backward compatibility. */
inline void deserializeCompressedData(const std::string &input, std::string &method,
                                      double &entropy, std::string &serializedCodeMap,
                                      std::string &payload) {
    std::string ignored;
    deserializeCompressedData(input, method, entropy, serializedCodeMap, payload, ignored);
}

/* Backward-compatible aliases retained so existing test binaries continue to link. */
inline std::string packData(const std::string &method, double entropy,
                            const std::string &serializedCodeMap,
                            const std::string &payload) {
    return serializeCompressedData(method, entropy, serializedCodeMap, payload, "");
}

inline void unpackData(const std::string &input, std::string &method,
                       double &entropy, std::string &serializedCodeMap,
                       std::string &payload) {
    std::string ignored;
    deserializeCompressedData(input, method, entropy, serializedCodeMap, payload, ignored);
}

#endif

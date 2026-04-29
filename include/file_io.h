#ifndef FILE_IO_H
#define FILE_IO_H

#include <string>
#include <unordered_map>

std::string readFile(const std::string &filename);
void writeFile(const std::string &filename, const std::string &data);

std::string serializeCodeMap(const std::unordered_map<std::string, std::string> &codeMap);
std::unordered_map<std::string, std::string> deserializeCodeMap(const std::string &serialized);

std::string serializeCompressedData(const std::string &method, double entropy,
                                    const std::string &codeMapData,
                                    const std::string &payload,
                                    const std::string &originalFilename = "");

void deserializeCompressedData(const std::string &input, std::string &method,
                               double &entropy, std::string &codeMapData,
                               std::string &payload, std::string &originalFilename);

void deserializeCompressedData(const std::string &input, std::string &method,
                               double &entropy, std::string &codeMapData,
                               std::string &payload);

std::string packData(const std::string &method, double entropy,
                     const std::string &codeMapData,
                     const std::string &payload);

void unpackData(const std::string &input, std::string &method,
                double &entropy, std::string &codeMapData,
                std::string &payload);

#endif

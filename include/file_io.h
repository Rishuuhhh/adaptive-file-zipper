#ifndef FILE_IO_H
#define FILE_IO_H
#include <string>
#include <unordered_map>
using namespace std;
 string readFile(const  string &filename);
void writeFile(const  string &filename, const  string &data);
 string serializeCodeMap(const  unordered_map< string,  string> &codeMap);
 unordered_map< string,  string> deserializeCodeMap(const  string &serialized);
 string serializeCompressedData(const  string &method, double entropy, const  string &codeMapData, const  string &payload, const  string &originalFilename = "");
void deserializeCompressedData(const  string &input,  string &method, double &entropy,  string &codeMapData, string &payload,  string &originalFilename);
void deserializeCompressedData(const  string &input,  string &method, double &entropy,  string &codeMapData, string &payload);
 string packData(const  string &method, double entropy, const  string &codeMapData, const  string &payload);
void unpackData(const  string &input,  string &method, double &entropy,  string &codeMapData, string &payload);
#endif

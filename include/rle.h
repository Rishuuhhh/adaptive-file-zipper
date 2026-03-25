#ifndef RLE_H
#define RLE_H

#include <string>

std::string rleCompress(const std::string &data);
std::string rleDecompress(const std::string &data);

#endif
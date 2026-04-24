#ifndef RLE_H
#define RLE_H

#include <string>

static const int MAX_RUN_LENGTH = 65535;

std::string rleCompress(const std::string &data);
std::string rleDecompress(const std::string &data);

#endif

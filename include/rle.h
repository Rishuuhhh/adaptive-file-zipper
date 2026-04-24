#ifndef RLE_H
#define RLE_H

#include <string>
using namespace std;
static const int MAX_RUN_LENGTH = 65535;

 string rleCompress(const  string &data);
 string rleDecompress(const  string &data);

#endif

#ifndef RLE_H
#define RLE_H

#include <string>

// Maximum number of consecutive identical characters that can be encoded in a single run.
// The count is stored as two bytes (big-endian), so the maximum value is 65535.
static const int MAX_RUN_LENGTH = 65535;

/**
 * Compresses the input string using Run-Length Encoding.
 * Each run is stored as a 3-byte record: [character][countHigh][countLow].
 * Runs longer than MAX_RUN_LENGTH are split into multiple records.
 *
 * @param data  The raw input bytes to compress.
 * @return      The RLE-compressed byte string.
 */
std::string rleCompress(const std::string &data);

/**
 * Decompresses an RLE-compressed byte string produced by rleCompress.
 * Reads 3-byte records [character][countHigh][countLow] and expands each run.
 *
 * @param data  The RLE-compressed bytes to decompress.
 * @return      The original uncompressed byte string.
 */
std::string rleDecompress(const std::string &data);

#endif

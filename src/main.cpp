#include "controller.h"
#include "file_io.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace std;
static void printUsage() {
    cerr << "\n";
    cerr << "  Adaptive File Zipper\n";
    cerr << "  --------------------\n";
    cerr << "  Usage:\n";
    cerr << "    ./zipper compress   <input_file>  <output_file.z>\n";
    cerr << "    ./zipper decompress <input_file.z> <output_file>\n";
    cerr << "\n";
    cerr << "  Automatically picks the best method for each file.\n";
    cerr << "\n";
}
static int compressFile(const string &inputPath, const string &outputPath) {
    if (!filesystem::exists(inputPath)) {
        cout << "{\"error\": \"Input file not found: " << inputPath << "\"}\n";
        return 1;
    }
    string fileContents = readFile(inputPath);
    size_t originalSize = fileContents.size();
    string originalFilename = filesystem::path(inputPath).filename().string();
    CompressionResult result = runAdaptiveCompression(fileContents, originalFilename);
    writeFile(outputPath, result.compressedData);
    cout << "{\n";
    cout << "  \"entropy\": "            << result.entropy               << ",\n";
    cout << "  \"adaptiveMethod\": \""   << result.method                << "\",\n";
    cout << "  \"adaptiveRatio\": "      << result.adaptiveRatio         << ",\n";
    cout << "  \"huffmanRatio\": "       << result.huffmanRatio          << ",\n";
    cout << "  \"time\": "               << result.timeTaken             << ",\n";
    cout << "  \"originalSize\": "       << originalSize                 << ",\n";
    cout << "  \"compressedSize\": "     << result.compressedData.size() << ",\n";
    cout << "  \"originalFilename\": \"" << result.originalFilename      << "\"\n";
    cout << "}\n";
    return 0;
}
static int decompressFile(const string &inputPath, const string &outputPath) {
    if (!filesystem::exists(inputPath)) {
        cout << "{\"error\": \"Input file not found: " << inputPath << "\"}\n";
        return 1;
    }
    string blobContents = readFile(inputPath);
    DecompressionResult result;
    try {
        result = runDecompression(blobContents);
    } catch (const exception &ex) {
        cout << "{\"error\": \"" << ex.what() << "\"}\n";
        return 1;
    }
    writeFile(outputPath, result.data);
    cout << "{\"status\": \"done\", \"originalFilename\": \""
         << result.originalFilename << "\"}\n";
    return 0;
}
int main(int argc, char *argv[]) {
    if (argc < 4) {
        printUsage();
        return 1;
    }
    string mode       = argv[1];
    string inputPath  = argv[2];
    string outputPath = argv[3];
    if (mode == "compress") {
        return compressFile(inputPath, outputPath);
    }
    if (mode == "decompress") {
        return decompressFile(inputPath, outputPath);
    }
    cerr << "Unknown mode: " << mode << "\n";
    printUsage();
    return 1;
}

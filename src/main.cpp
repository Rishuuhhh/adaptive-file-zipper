#include "controller.h"
#include "file_io.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace std;

// Usage: ./zipper compress input.txt output.z
//        ./zipper decompress input.z output.txt

static bool isValidMode(const string &mode) {
    return mode == "compress" || mode == "decompress";
}

static void printUsage() {
    cout << "========================================\n";
    cout << "Adaptive File Zipper - CLI\n";
    cout << "========================================\n";
    cout << "Usage:\n";
    cout << "  ./zipper compress <input_file> <output_file>\n";
    cout << "  ./zipper decompress <input_file> <output_file>\n";
}

static bool validateInputPath(const string &inputPath) {
    if (inputPath.empty()) {
        cout << "{\"error\": \"Input path is empty\"}\n";
        return false;
    }

    if (!filesystem::exists(inputPath)) {
        cout << "{\"error\": \"Could not read file\"}\n";
        return false;
    }

    return true;
}

static int runCompressFlow(const string &inputPath, const string &outputPath) {
    if (!validateInputPath(inputPath)) {
        return 1;
    }

    string originalFilename = filesystem::path(inputPath).filename().string();
    string inputData = readFile(inputPath);
    size_t originalSize = inputData.size();

    CompressionResult result = runAdaptiveCompression(inputData, originalFilename);
    size_t compressedSize = result.compressedData.size();
    writeFile(outputPath, result.compressedData);

    // Keep this JSON shape stable because backend/server.js parses it directly.
    cout << "{\n";
    cout << "\"entropy\": " << result.entropy << ",\n";
    cout << "\"adaptiveMethod\": \"" << result.method << "\",\n";
    cout << "\"adaptiveRatio\": " << result.adaptiveRatio << ",\n";
    cout << "\"huffmanRatio\": " << result.huffmanRatio << ",\n";
    cout << "\"time\": " << result.timeTaken << ",\n";
    cout << "\"originalSize\": " << originalSize << ",\n";
    cout << "\"compressedSize\": " << compressedSize << ",\n";
    cout << "\"originalFilename\": \"" << result.originalFilename << "\"\n";
    cout << "}\n";

    return 0;
}

static int runDecompressFlow(const string &inputPath, const string &outputPath) {
    if (!validateInputPath(inputPath)) {
        return 1;
    }

    string packedInput = readFile(inputPath);
    DecompressionResult result;
    try {
        result = runDecompression(packedInput);
    } catch (const exception &ex) {
        cout << "{\"error\": \"" << ex.what() << "\"}\n";
        return 1;
    }

    writeFile(outputPath, result.data);
    cout << "{ \"status\": \"done\", \"originalFilename\": \""
         << result.originalFilename << "\" }\n";

    return 0;
}

int main(int argc, char *argv[]) {

    if (argc < 4) {
        printUsage();
        return 1;
    }

    string mode = argv[1];
    string inputPath = argv[2];
    string outputPath = argv[3];

    if (!isValidMode(mode)) {
        printUsage();
        return 1;
    }

    if (mode == "compress") {
        return runCompressFlow(inputPath, outputPath);
    }

    return runDecompressFlow(inputPath, outputPath);
}

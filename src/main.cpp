#include "controller.h"
#include "file_io.h"

#include <filesystem>
#include <iostream>

using namespace std;

/*
Usage:
./zipper compress input.txt output.z
./zipper decompress input.z output.txt
*/

int main(int argc, char *argv[]) {

    if (argc < 4) {
        cout << "Invalid arguments\n";
        return 1;
    }

    string mode = argv[1];
    string inputFile = argv[2];
    string outputFile = argv[3];

    if (mode == "compress") {

        if (!filesystem::exists(inputFile)) {
            cout << "{\"error\": \"Could not read file\"}\n";
            return 1;
        }

        string data = readFile(inputFile);

        CompressionResult res = runAdaptiveCompression(data);

        writeFile(outputFile, res.compressedData);

        // JSON output (important for Node.js)
        cout << "{\n";
        cout << "\"entropy\": " << res.entropy << ",\n";
        cout << "\"adaptiveMethod\": \"" << res.method << "\",\n";
        cout << "\"adaptiveRatio\": " << res.adaptiveRatio << ",\n";
        cout << "\"huffmanRatio\": " << res.huffmanRatio << ",\n";
        cout << "\"time\": " << res.timeTaken << "\n";
        cout << "}\n";

    } else if (mode == "decompress") {

        if (!filesystem::exists(inputFile)) {
            cout << "{\"error\": \"Could not read file\"}\n";
            return 1;
        }

        string packed = readFile(inputFile);
        string original = runDecompression(packed);

        writeFile(outputFile, original);

        cout << "{ \"status\": \"done\" }\n";
    }

    return 0;
}
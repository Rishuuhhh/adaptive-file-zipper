#include "controller.h"
#include "file_io.h"

#include <filesystem>
#include <iostream>

using namespace std;

// Usage: ./zipper compress input.txt output.z
//        ./zipper decompress input.z output.txt

int main(int argc, char *argv[]) {

    if (argc < 4) {
        cout << "Invalid arguments\n";
        return 1;
    }

    string mode = argv[1];
    string fin  = argv[2];
    string fout = argv[3];

    if (mode == "compress") {

        if (!filesystem::exists(fin)) {
            cout << "{\"error\": \"Could not read file\"}\n";
            return 1;
        }

        string d = readFile(fin);
        CompressionResult r = runAdaptiveCompression(d);
        writeFile(fout, r.compressedData);

        // Node.js backend parses this JSON output.
        cout << "{\n";
        cout << "\"entropy\": "        << r.entropy       << ",\n";
        cout << "\"adaptiveMethod\": \"" << r.method      << "\",\n";
        cout << "\"adaptiveRatio\": "  << r.adaptiveRatio << ",\n";
        cout << "\"huffmanRatio\": "   << r.huffmanRatio  << ",\n";
        cout << "\"time\": "           << r.timeTaken     << "\n";
        cout << "}\n";

    } else if (mode == "decompress") {

        if (!filesystem::exists(fin)) {
            cout << "{\"error\": \"Could not read file\"}\n";
            return 1;
        }

        string pk  = readFile(fin);
        string org = runDecompression(pk);
        writeFile(fout, org);

        cout << "{ \"status\": \"done\" }\n";
    }

    return 0;
}

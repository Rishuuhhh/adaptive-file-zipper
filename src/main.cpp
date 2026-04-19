#include "controller.h"
#include "file_io.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

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
        size_t originalSize = d.size();
        CompressionResult r = runAdaptiveCompression(d);
        size_t compressedSize = r.compressedData.size();
        writeFile(fout, r.compressedData);

        // Node.js backend parses this JSON output.
        cout << "{\n";
        cout << "\"entropy\": "        << r.entropy       << ",\n";
        cout << "\"adaptiveMethod\": \"" << r.method      << "\",\n";
        cout << "\"adaptiveRatio\": "  << r.adaptiveRatio << ",\n";
        cout << "\"huffmanRatio\": "   << r.huffmanRatio  << ",\n";
        cout << "\"time\": "           << r.timeTaken     << ",\n";
        cout << "\"originalSize\": "   << originalSize    << ",\n";
        cout << "\"compressedSize\": " << compressedSize  << "\n";
        cout << "}\n";

    } else if (mode == "decompress") {

        if (!filesystem::exists(fin)) {
            cout << "{\"error\": \"Could not read file\"}\n";
            return 1;
        }

        string pk = readFile(fin);
        string org;
        try {
            org = runDecompression(pk);
        } catch (const std::exception &e) {
            cout << "{\"error\": \"" << e.what() << "\"}\n";
            return 1;
        }
        writeFile(fout, org);

        cout << "{ \"status\": \"done\" }\n";
    }

    return 0;
}

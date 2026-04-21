#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

// Run command and return combined output with exit code.
static std::pair<std::string, int> runCliCommand(const std::string &command) {
    std::string commandOutput;
    std::array<char, 256> buffer;

    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) return {"", -1};

    while (fgets(buffer.data(), buffer.size(), pipe)) {
        commandOutput += buffer.data();
    }

    int exitStatus = pclose(pipe);
    return {commandOutput, WEXITSTATUS(exitStatus)};
}

// Create a temp file and return its path.
static std::string createTempInputFile(const std::string &content,
                                       const std::string &extension = ".txt") {
    std::string filePath = "/tmp/zipper_test" + extension;
    std::ofstream file(filePath);
    file << content;
    return filePath;
}

// Fewer than 4 args should exit with code 1.
TEST(CLI, TooFewArgsExits1) {
    auto [out, code] = runCliCommand("./zipper compress 2>&1");
    EXPECT_EQ(code, 1);
}

TEST(CLI, NoArgsExits1) {
    auto [out, code] = runCliCommand("./zipper 2>&1");
    EXPECT_EQ(code, 1);
}

// Missing input file should return error and exit code 1.
TEST(CLI, MissingFileExits1) {
    auto [out, code] = runCliCommand("./zipper compress /tmp/nope_xyz.txt /tmp/out.z 2>&1");
    EXPECT_EQ(code, 1);
    EXPECT_NE(out.find("error"), std::string::npos);
}

// Compress should output valid JSON fields.
TEST(CLI, CompressOutputsJSON) {
    std::string inputPath = createTempInputFile("hello world aaa bbb ccc repeated content");
    std::string outputPath = "/tmp/zipper_test_out.z";
    auto [o, code] = runCliCommand("./zipper compress " + inputPath + " " + outputPath);

    EXPECT_EQ(code, 0) << o;
    EXPECT_NE(o.find("entropy"),        std::string::npos);
    EXPECT_NE(o.find("adaptiveMethod"), std::string::npos);
    EXPECT_NE(o.find("adaptiveRatio"),  std::string::npos);
    EXPECT_NE(o.find("huffmanRatio"),   std::string::npos);
    EXPECT_NE(o.find("time"),           std::string::npos);

    std::ifstream compressedFile(outputPath);
    EXPECT_TRUE(compressedFile.good());
}

// Decompress should output status=done.
TEST(CLI, DecompressOutputsDone) {
    std::string inputPath = createTempInputFile("hello world decompress test");
    std::string compressedPath = "/tmp/zipper_test_decomp.z";
    std::string decompressedPath = "/tmp/zipper_test_decomp_out.txt";

    auto [co, cc] = runCliCommand("./zipper compress " + inputPath + " " + compressedPath);
    ASSERT_EQ(cc, 0) << co;

    auto [o, code] = runCliCommand("./zipper decompress " + compressedPath + " " + decompressedPath);
    EXPECT_EQ(code, 0) << o;
    EXPECT_NE(o.find("status"), std::string::npos);
    EXPECT_NE(o.find("done"),   std::string::npos);
}

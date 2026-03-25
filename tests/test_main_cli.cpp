// Unit tests for the zipper CLI interface
// Validates: Requirements 8.1, 8.2, 8.3, 8.4

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

// Helper: run a shell command and capture stdout + exit code
static std::pair<std::string, int> runCommand(const std::string &cmd) {
    std::string output;
    std::array<char, 256> buf;

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {"", -1};

    while (fgets(buf.data(), buf.size(), pipe)) {
        output += buf.data();
    }

    int status = pclose(pipe);
    int exitCode = WEXITSTATUS(status);
    return {output, exitCode};
}

// Helper: write a temp file with given content, return its path
static std::string writeTempFile(const std::string &content, const std::string &suffix = ".txt") {
    std::string path = "/tmp/zipper_test_input" + suffix;
    std::ofstream f(path);
    f << content;
    return path;
}

// Req 8.3: fewer than 4 args → exit code 1
TEST(ZipperCLI, FewerThan4ArgsExitsWithCode1) {
    auto [out, code] = runCommand("./zipper compress 2>&1");
    EXPECT_EQ(code, 1);
}

TEST(ZipperCLI, NoArgsExitsWithCode1) {
    auto [out, code] = runCommand("./zipper 2>&1");
    EXPECT_EQ(code, 1);
}

// Req 8.4: missing input file → JSON error + exit code 1
TEST(ZipperCLI, MissingInputFileExitsWithCode1) {
    auto [out, code] = runCommand("./zipper compress /tmp/nonexistent_file_xyz.txt /tmp/out.z 2>&1");
    EXPECT_EQ(code, 1);
    EXPECT_NE(out.find("error"), std::string::npos);
}

// Req 8.1: compress outputs valid JSON with all required fields
TEST(ZipperCLI, CompressOutputsValidJSONWithRequiredFields) {
    std::string inputPath = writeTempFile("hello world this is a test file with some repeated content aaa bbb ccc");
    std::string outputPath = "/tmp/zipper_test_output.z";

    auto [out, code] = runCommand("./zipper compress " + inputPath + " " + outputPath);

    EXPECT_EQ(code, 0) << "zipper compress failed with output: " << out;

    // Check all required JSON fields are present
    EXPECT_NE(out.find("entropy"), std::string::npos)       << "Missing 'entropy' in: " << out;
    EXPECT_NE(out.find("adaptiveMethod"), std::string::npos) << "Missing 'adaptiveMethod' in: " << out;
    EXPECT_NE(out.find("adaptiveRatio"), std::string::npos)  << "Missing 'adaptiveRatio' in: " << out;
    EXPECT_NE(out.find("huffmanRatio"), std::string::npos)   << "Missing 'huffmanRatio' in: " << out;
    EXPECT_NE(out.find("time"), std::string::npos)           << "Missing 'time' in: " << out;

    // Check output file was created
    std::ifstream outFile(outputPath);
    EXPECT_TRUE(outFile.good()) << "Output .z file was not created";
}

// Req 8.2: decompress outputs {"status": "done"}
TEST(ZipperCLI, DecompressOutputsStatusDone) {
    // First compress a file to get a valid .z file
    std::string inputPath = writeTempFile("hello world decompression test");
    std::string compressedPath = "/tmp/zipper_test_decomp.z";
    std::string decompressedPath = "/tmp/zipper_test_decomp_out.txt";

    auto [compOut, compCode] = runCommand("./zipper compress " + inputPath + " " + compressedPath);
    ASSERT_EQ(compCode, 0) << "Compress step failed: " << compOut;

    auto [out, code] = runCommand("./zipper decompress " + compressedPath + " " + decompressedPath);

    EXPECT_EQ(code, 0) << "zipper decompress failed with output: " << out;
    EXPECT_NE(out.find("status"), std::string::npos)  << "Missing 'status' in: " << out;
    EXPECT_NE(out.find("done"), std::string::npos)    << "Missing 'done' in: " << out;
}

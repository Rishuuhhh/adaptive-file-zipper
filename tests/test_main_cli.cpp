#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

// command run karo aur output + exit code wapas lo
static std::pair<std::string, int> run(const std::string &cmd) {
    std::string out;
    std::array<char, 256> buf;
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return {"", -1};
    while (fgets(buf.data(), buf.size(), p)) out += buf.data();
    int st = pclose(p);
    return {out, WEXITSTATUS(st)};
}

// temp file banao aur path return karo
static std::string tmpFile(const std::string &content, const std::string &ext = ".txt") {
    std::string path = "/tmp/zipper_test" + ext;
    std::ofstream f(path);
    f << content;
    return path;
}

// 4 se kam args pe exit code 1 aana chahiye
TEST(CLI, TooFewArgsExits1) {
    auto [out, code] = run("./zipper compress 2>&1");
    EXPECT_EQ(code, 1);
}

TEST(CLI, NoArgsExits1) {
    auto [out, code] = run("./zipper 2>&1");
    EXPECT_EQ(code, 1);
}

// file na mile toh error aur exit code 1
TEST(CLI, MissingFileExits1) {
    auto [out, code] = run("./zipper compress /tmp/nope_xyz.txt /tmp/out.z 2>&1");
    EXPECT_EQ(code, 1);
    EXPECT_NE(out.find("error"), std::string::npos);
}

// compress pe valid JSON aana chahiye
TEST(CLI, CompressOutputsJSON) {
    std::string in  = tmpFile("hello world aaa bbb ccc repeated content");
    std::string out = "/tmp/zipper_test_out.z";
    auto [o, code]  = run("./zipper compress " + in + " " + out);

    EXPECT_EQ(code, 0) << o;
    EXPECT_NE(o.find("entropy"),        std::string::npos);
    EXPECT_NE(o.find("adaptiveMethod"), std::string::npos);
    EXPECT_NE(o.find("adaptiveRatio"),  std::string::npos);
    EXPECT_NE(o.find("huffmanRatio"),   std::string::npos);
    EXPECT_NE(o.find("time"),           std::string::npos);

    std::ifstream f(out);
    EXPECT_TRUE(f.good());
}

// decompress pe status done aana chahiye
TEST(CLI, DecompressOutputsDone) {
    std::string in   = tmpFile("hello world decompress test");
    std::string cmp  = "/tmp/zipper_test_decomp.z";
    std::string dcmp = "/tmp/zipper_test_decomp_out.txt";

    auto [co, cc] = run("./zipper compress " + in + " " + cmp);
    ASSERT_EQ(cc, 0) << co;

    auto [o, code] = run("./zipper decompress " + cmp + " " + dcmp);
    EXPECT_EQ(code, 0) << o;
    EXPECT_NE(o.find("status"), std::string::npos);
    EXPECT_NE(o.find("done"),   std::string::npos);
}

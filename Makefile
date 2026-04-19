CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
# Support both /usr/local (Linux / manual install) and /opt/homebrew (Apple Silicon)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /usr/local)
INCLUDES := -I include -I $(BREW_PREFIX)/include
LDFLAGS  := -L $(BREW_PREFIX)/lib -lgtest -lgtest_main -lrapidcheck -pthread

SRC_FILES := src/controller.cpp src/file_io.cpp src/entropy.cpp \
             src/rle.cpp src/block_huffman.cpp src/huffman.cpp

TEST_FILES := tests/test_modules.cpp tests/test_controller.cpp tests/test_file_io.cpp

# ── Main binary ──────────────────────────────────────────────────────────────
zipper: src/main.cpp $(SRC_FILES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o zipper src/main.cpp $(SRC_FILES)

# ── Test runner ──────────────────────────────────────────────────────────────
# Requires: googletest and rapidcheck installed at /usr/local
#   brew install googletest
#   brew install rapidcheck   (or build from source)
tests/test_runner: $(TEST_FILES) $(SRC_FILES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o tests/test_runner \
		$(TEST_FILES) $(SRC_FILES) \
		$(LDFLAGS)

# ── Convenience targets ──────────────────────────────────────────────────────
.PHONY: test build clean check

build: zipper

test: tests/test_runner
	./tests/test_runner

# Compile-only check (no linking) — useful when gtest/rapidcheck are not installed
check:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c tests/test_modules.cpp   -o /dev/null
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c tests/test_controller.cpp -o /dev/null
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c tests/test_file_io.cpp    -o /dev/null
	@echo "All test files compile cleanly."

clean:
	rm -f zipper tests/test_runner tests/*.o src/*.o

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
# Works for both Intel/Homebrew and Apple Silicon/Homebrew installs.
BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /usr/local)
INCLUDES := -I include -I $(BREW_PREFIX)/include

SRC_FILES := src/controller.cpp src/file_io.cpp src/entropy.cpp \
             src/rle.cpp src/block_huffman.cpp src/huffman.cpp

# Main executable used by CLI and backend.
zipper: src/main.cpp $(SRC_FILES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o zipper src/main.cpp $(SRC_FILES)

# Convenience targets.
.PHONY: build clean

build: zipper

clean:
	rm -f zipper src/*.o

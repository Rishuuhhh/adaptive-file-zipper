# Adaptive File Zipper

A file compression system built in three layers: a C++ compression engine, a Node.js/Express backend, and a browser frontend.

The C++ engine runs three compression strategies on every input and picks the one that produces the smallest output:

- **RLE** — run-length encoding, best for highly repetitive data
- **Global Huffman** — canonical Huffman over the entire file, best for uniform distributions
- **Block Huffman** — canonical Huffman applied per 32KB block, best for files with varying local distributions

For small inputs (< 4KB), only global Huffman is used to avoid block overhead. Near-random data (entropy >= 7.8 bits/symbol) is stored as-is.

---

## Prerequisites

- macOS or Linux
- `g++` with C++17 support (`xcode-select --install` on macOS)
- Node.js 18+

---

## Quick Start

### 1. Build the C++ binary

```bash
make build
```

Or manually:

```bash
g++ -std=c++17 -O2 -o zipper \
  src/main.cpp src/controller.cpp src/file_io.cpp \
  src/entropy.cpp src/rle.cpp src/huffman.cpp src/block_huffman.cpp \
  -I include
```

### 2. Install backend dependencies

```bash
cd backend && npm install
```

### 3. Start the server

```bash
cd backend && npm start
```

Server runs on `http://localhost:3000`. Keep this terminal open while using the app.

### 4. Open the app

Go to `http://localhost:3000` in your browser.

> Do not open `frontend/index.html` directly as a `file://` URL — it won't reach the API.

---

## Using the App

1. Select any file and click **Compress**
2. The results panel shows entropy, method chosen, adaptive ratio vs standard Huffman ratio, and a download link for the `.z` file
3. To decompress: select a `.z` file and click **Decompress** — the original file downloads automatically

---

## CLI Usage

```bash
# Compress
./zipper compress input.txt output.z

# Decompress
./zipper decompress output.z recovered.txt
```

Compress prints JSON to stdout:

```json
{
  "entropy": 3.856,
  "adaptiveMethod": "GLOBAL_HUFF",
  "adaptiveRatio": 0.490,
  "huffmanRatio": 0.482,
  "time": 0.55
}
```

`adaptiveRatio` is the actual compressed size / original size. Lower is better.  
`huffmanRatio` is the Shannon entropy lower bound — the theoretical best standard Huffman can do.

---

## Compression Methods

| Method tag | Strategy | When it wins |
|---|---|---|
| `RLE` | Run-length encoding | Long repeated byte runs |
| `GLOBAL_HUFF` | Canonical Huffman, whole file | Uniform symbol distribution |
| `BLOCK_HUFF` | Canonical Huffman, 32KB blocks | Files with varying local distributions |
| `NONE` | No compression | Near-random data (entropy >= 7.8) |

### Huffman header format

Each Huffman segment stores only the symbols that actually appear:

```
[1 byte]  N — number of unique symbols
[N bytes] symbol values
[N bytes] code lengths
[1 byte]  padding bit count
[rest]    bit-packed encoded data
```

This keeps header overhead proportional to the number of unique symbols rather than a fixed 256 bytes.

---

## Running Tests

### C++ tests

```bash
make test
```

Requires `googletest` and `rapidcheck`:

```bash
brew install googletest

git clone https://github.com/emil-e/rapidcheck.git /tmp/rapidcheck
cd /tmp/rapidcheck && mkdir build && cd build
cmake .. && make && sudo make install
```

### Backend tests (Jest + Supertest)

```bash
cd backend && npm test
```

### Frontend tests (Jest + jsdom)

```bash
cd frontend && npm install && npm test
```

---

## Project Structure

```
.
├── src/
│   ├── main.cpp           # CLI entry point
│   ├── controller.cpp     # Runs all strategies, picks best, dispatches decompress
│   ├── block_huffman.cpp  # Global + block-split canonical Huffman
│   ├── rle.cpp            # Run-length encoding
│   ├── entropy.cpp        # Shannon entropy
│   ├── file_io.cpp        # Pack/unpack file format
│   └── huffman.cpp        # Standard Huffman (unused in pipeline, kept for reference)
├── include/               # C++ headers
├── tests/                 # C++ unit + property tests
├── backend/
│   ├── server.js          # Express API (/compress, /decompress, /download)
│   └── tests/
├── frontend/
│   ├── index.html
│   ├── script.js
│   ├── style.css
│   └── tests/
├── data/                  # Uploaded files (auto-created)
├── results/               # Output files (auto-created)
├── Makefile
└── zipper                 # Compiled binary
```

---

## Makefile Targets

| Target | Description |
|---|---|
| `make build` | Compile the `zipper` binary |
| `make test` | Build and run C++ tests |
| `make check` | Compile-check test files without running |
| `make clean` | Remove compiled artifacts |

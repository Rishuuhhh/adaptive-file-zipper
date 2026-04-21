# Adaptive File Zipper

This project is used to compress the text files as well as binary files you can download it and decompress the file. It uses adaptive algorithm used.

It has three layers:

1. C++ core for compression/decompression logic
2. Node.js backend that calls the C++ binary
3. Browser frontend for easy upload/download usage

## How Compression Is Chosen

For each input file, the controller tries multiple methods and picks the best result:

1. `RLE` for repeated bytes
2. `GLOBAL_HUFF` with one Huffman model for the full file
3. `BLOCK_HUFF` with per-block Huffman models

If entropy is very high (near-random data), it stores data as `NONE` to avoid useless compression work.

## Requirements

1. macOS or Linux
2. g++ with C++17 support
3. Node.js 18+

On macOS you can install compiler tools with:

```bash
xcode-select --install
```

## Setup

### 1. Build the C++ binary

```bash
make build
```

### 2. Install backend dependencies

```bash
cd backend
npm install
```

### 3. Run the backend

```bash
cd backend
npm start
```

Server starts on `http://localhost:3000`.

### 4. Open the app

Open `http://localhost:3000` in browser.

Note:
Do not open `frontend/index.html` directly with `file://` because API calls need the backend server.

## CLI Usage

### Compress

```bash
./zipper compress input.txt output.z
```

### Decompress

```bash
./zipper decompress output.z recovered.txt
```

The compress command prints JSON stats like:

```json
{
  "entropy": 3.856,
  "adaptiveMethod": "GLOBAL_HUFF",
  "adaptiveRatio": 0.490,
  "huffmanRatio": 0.482,
  "time": 0.55
}
```

Meaning:
1. `adaptiveRatio = compressedSize / originalSize` (lower is better)
2. `huffmanRatio` is entropy-based theoretical estimate

## Method Tags

| Tag | Meaning | Typical case |
|---|---|---|
| `RLE` | Run-Length Encoding | long repeated runs |
| `GLOBAL_HUFF` | one Huffman model | consistent symbol distribution |
| `BLOCK_HUFF` | block-wise Huffman | mixed-content files |
| `NONE` | no compression | near-random input |

## Huffman Header Layout

Only used symbols are stored:

```text
[1 byte]  N = number of unique symbols
[N bytes] symbol values
[N bytes] code lengths
[1 byte]  padding bits in final byte
[rest]    bit-packed payload
```

## Running Tests

### C++ tests

```bash
make test
```

### Backend tests

```bash
cd backend
npm test -- --runInBand
```

### Frontend tests

```bash
cd frontend
npm test -- --runInBand
```

## Useful Make Targets

| Command | Purpose |
|---|---|
| `make build` | compile `zipper` binary |
| `make test` | build + run C++ tests |
| `make check` | compile-only check for tests |
| `make clean` | remove generated build artifacts |

## Project Structure

```text
.
├── src/                  # C++ implementation
├── include/              # C++ headers
├── tests/                # C++ tests
├── backend/              # Express server + tests
├── frontend/             # browser client + tests
├── data/                 # uploaded temp files
├── results/              # generated output files
├── Makefile
└── zipper                # compiled binary
```

# Adaptive File Zipper

C++ compression engine with a Node.js backend and browser frontend.

The C++ part tries three methods on every file and keeps the smallest result:

- RLE — good for files with lots of repeated bytes
- Global Huffman — one huffman tree for the whole file
- Block Huffman — separate huffman tree per 32KB chunk, good for mixed files

Files under 4KB skip block mode. Files with very high entropy (>= 7.8 bits/symbol) are stored as-is since compression won't help.

---

## Requirements

- macOS or Linux
- g++ with C++17 (`xcode-select --install` on macOS)
- Node.js 18+

---

## Setup

### 1. Build

```bash
make build
```

or manually:

```bash
g++ -std=c++17 -O2 -o zipper \
  src/main.cpp src/controller.cpp src/file_io.cpp \
  src/entropy.cpp src/rle.cpp src/huffman.cpp src/block_huffman.cpp \
  -I include
```

### 2. Backend

```bash
cd backend && npm install
```

### 3. Run server

```bash
cd backend && npm start
```

Opens at `http://localhost:3000`. Keep this terminal running.

### 4. Open browser

Go to `http://localhost:3000`.

> Don't open `frontend/index.html` directly as a file:// URL, it won't work.

---

## How to use

1. Pick a file and click Compress
2. You'll see entropy, which method was picked, compression ratio, and a download link for the `.z` file
3. To decompress: pick a `.z` file and click Decompress

---

## CLI

```bash
# compress
./zipper compress input.txt output.z

# decompress
./zipper decompress output.z recovered.txt
```

Compress prints JSON:

```json
{
  "entropy": 3.856,
  "adaptiveMethod": "GLOBAL_HUFF",
  "adaptiveRatio": 0.490,
  "huffmanRatio": 0.482,
  "time": 0.55
}
```

`adaptiveRatio` = compressed size / original size. Lower is better.  
`huffmanRatio` = theoretical best huffman can do (Shannon entropy bound).

---

## Methods

| Tag | What it does | Best for |
|---|---|---|
| `RLE` | run-length encoding | files with long repeated runs |
| `GLOBAL_HUFF` | huffman on whole file | uniform data |
| `BLOCK_HUFF` | huffman per 32KB block | files that change a lot |
| `NONE` | no compression | near-random data |

### Huffman header layout

Only symbols that actually appear are stored:

```
[1 byte]  N = number of unique symbols
[N bytes] symbol values
[N bytes] code lengths
[1 byte]  padding bits in last byte
[rest]    bit-packed data
```

---

## Tests

### C++

```bash
make test
```

Needs googletest and rapidcheck:

```bash
brew install googletest

git clone https://github.com/emil-e/rapidcheck.git /tmp/rapidcheck
cd /tmp/rapidcheck && mkdir build && cd build
cmake .. && make && sudo make install
```

### Backend

```bash
cd backend && npm test
```

### Frontend

```bash
cd frontend && npm install && npm test
```

---

## Project layout

```
.
├── src/
│   ├── main.cpp           # CLI entry
│   ├── controller.cpp     # picks best method, handles decompress
│   ├── block_huffman.cpp  # global + block huffman
│   ├── rle.cpp            # run-length encoding
│   ├── entropy.cpp        # entropy calculation
│   ├── file_io.cpp        # file format pack/unpack
│   └── huffman.cpp        # basic huffman (kept for reference)
├── include/               # headers
├── tests/                 # C++ tests
├── backend/
│   ├── server.js          # Express API
│   └── tests/
├── frontend/
│   ├── index.html
│   ├── script.js
│   ├── style.css
│   └── tests/
├── data/                  # uploaded files
├── results/               # output files
├── Makefile
└── zipper                 # compiled binary
```

---

## Makefile

| Target | What it does |
|---|---|
| `make build` | compile zipper |
| `make test` | build and run C++ tests |
| `make check` | compile check only |
| `make clean` | remove build files |

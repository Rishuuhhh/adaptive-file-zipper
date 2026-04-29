# Adaptive File Zipper

A file compression tool with a C++ compression engine, a Node.js backend, and a browser-based frontend.

The compressor tries three algorithms on every file and keeps whichever produces the smallest output:
- **RLE** — good for files with long repeated byte sequences
- **Global Huffman** — one Huffman tree for the whole file
- **Block Huffman** — separate Huffman tree per 32 KB block, better for mixed-content files

If the file is already compressed (JPEG, MP3, ZIP, etc.) or contains near-random data, it is stored as-is to avoid making it larger.

---

## Requirements

- macOS or Linux
- g++ with C++17 support (`xcode-select --install` on macOS)
- Node.js 18+

---

## Setup

**1. Build the C++ binary**
```bash
make build
```

**2. Install backend dependencies**
```bash
cd backend && npm install
```

**3. Start the server**
```bash
cd backend && npm start
```

Open `http://localhost:3000` in your browser.

> Do not open `frontend/index.html` directly — the API calls need the backend server running.

---

## CLI Usage

```bash
# Compress a file
./zipper compress input.txt output.z

# Decompress a .z file
./zipper decompress output.z recovered.txt
```

The compress command prints JSON stats to stdout:

```json
{
  "entropy": 3.856,
  "adaptiveMethod": "GLOBAL_HUFF",
  "adaptiveRatio": 0.490,
  "huffmanRatio": 0.482,
  "time": 0.55,
  "originalSize": 4096,
  "compressedSize": 2007,
  "originalFilename": "input.txt"
}
```

- `adaptiveRatio` = compressedSize / originalSize (lower is better)
- `huffmanRatio` = theoretical lower bound based on Shannon entropy

---

## .z File Format

Each `.z` file has a small text header followed by the compressed payload:

```
<method>\n
<entropy>\n
<codeMapLength>\n
<originalFilename>\n
<codeMap bytes>
<payload bytes>
```

The original filename is stored in the header so decompression can restore the correct file extension automatically.

---

## Method Tags

| Tag | Description |
|---|---|
| `RLE` | Run-Length Encoding |
| `GLOBAL_HUFF` | Single Huffman tree for the whole file |
| `BLOCK_HUFF` | Per-block Huffman (32 KB blocks) |
| `NONE` | No compression (data stored as-is) |

---

## Project Structure

```
.
├── src/           C++ source files
├── include/       C++ headers
├── backend/       Node.js Express server
├── frontend/      Browser UI
├── data/          Temporary upload directory
├── results/       Temporary output directory
├── Makefile
└── zipper         Compiled binary
```

---

## Make Targets

| Command | Purpose |
|---|---|
| `make build` | Compile the zipper binary |
| `make clean` | Remove build artifacts |

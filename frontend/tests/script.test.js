/**
 * @jest-environment jsdom
 */

// Feature: adaptive-file-zipper, Property 15: Stats formatting produces correct output

const fs = require('fs');
const path = require('path');
const fc = require('fast-check');

// Load script.js into the jsdom global scope via eval so globals are accessible
const scriptSource = fs.readFileSync(path.join(__dirname, '../script.js'), 'utf8');

// Helper to set up the minimal DOM required by script.js
function setupDOM() {
  document.body.innerHTML = `
    <input type="file" id="fileInput">
    <button id="compressBtn">Compress</button>
    <button id="decompressBtn">Decompress</button>
    <div id="results" class="hidden">
      <p id="error" class="hidden"></p>
      <p id="status" class="hidden"></p>
      <span id="entropy"></span>
      <span id="method"></span>
      <span id="adaptive"></span>
      <span id="huffman"></span>
      <span id="time"></span>
      <a id="downloadLink" class="hidden"></a>
    </div>
  `;
}

// Evaluate script.js once with DOM set up so globals land on `global`
setupDOM();
eval(scriptSource); // eslint-disable-line no-eval

// ─── Task 8.9: Property 15 — Stats formatting produces correct output ──────────

describe('Property 15: Stats formatting produces correct output', () => {
  // Validates: Requirements 11.2

  test('formatEntropy matches /^\\d+\\.\\d{4} bits\\/char$/ for values in [0, 8]', () => {
    fc.assert(
      fc.property(
        fc.float({ min: 0, max: 8, noNaN: true }),
        (val) => {
          const result = formatEntropy(val);
          expect(result).toMatch(/^\d+\.\d{4} bits\/char$/);
        }
      ),
      { numRuns: 200 }
    );
  });

  test('formatRatio matches /^\\d+\\.\\d%$/ for values in [0, 1]', () => {
    fc.assert(
      fc.property(
        fc.float({ min: 0, max: 1, noNaN: true }),
        (val) => {
          const result = formatRatio(val);
          expect(result).toMatch(/^\d+\.\d%$/);
        }
      ),
      { numRuns: 200 }
    );
  });

  test('formatTime matches /^\\d+\\.\\d{2} ms$/ for non-negative values', () => {
    fc.assert(
      fc.property(
        fc.float({ min: 0, max: 100000, noNaN: true }),
        (val) => {
          const result = formatTime(val);
          expect(result).toMatch(/^\d+\.\d{2} ms$/);
        }
      ),
      { numRuns: 200 }
    );
  });
});

// ─── Task 8.10: Unit tests for frontend UI behavior ────────────────────────────

describe('Frontend UI behavior', () => {
  beforeEach(() => {
    setupDOM();
    // Re-evaluate script so functions reference the fresh DOM
    eval(scriptSource); // eslint-disable-line no-eval
    jest.resetAllMocks();
    global.fetch = jest.fn();
    // Suppress window.URL.createObjectURL not implemented in jsdom
    global.URL.createObjectURL = jest.fn(() => 'blob:mock-url');
  });

  // Helper: create a fake File object for the file input
  function attachFile(name = 'test.txt') {
    const file = new File(['hello world'], name, { type: 'text/plain' });
    const input = document.getElementById('fileInput');
    Object.defineProperty(input, 'files', {
      value: [file],
      writable: true,
      configurable: true
    });
  }

  // ── Req 11.3: Successful compress response shows download link ──────────────

  test('Req 11.3: successful compress response shows visible download link', async () => {
    attachFile('test.txt');

    const mockResponse = {
      ok: true,
      json: jest.fn().mockResolvedValue({
        entropy: 3.1415,
        adaptiveMethod: 'BLOCK_HUFFMAN',
        adaptiveRatio: 0.723,
        huffmanRatio: 0.681,
        time: 1.23,
        download: '/download?path=%2Fresults%2Fout.z'
      })
    };
    global.fetch.mockResolvedValue(mockResponse);

    await compressFile();

    const link = document.getElementById('downloadLink');
    expect(link.classList.contains('hidden')).toBe(false);
    expect(link.href).toContain('/download');
  });

  // ── Req 11.4: Server error response shows visible error in results area ─────

  test('Req 11.4: server error response shows visible error in results area', async () => {
    attachFile('test.txt');

    const mockResponse = {
      ok: false,
      json: jest.fn().mockResolvedValue({ error: 'Compression failed' })
    };
    global.fetch.mockResolvedValue(mockResponse);

    await compressFile();

    const results = document.getElementById('results');
    const error = document.getElementById('error');
    expect(results.classList.contains('hidden')).toBe(false);
    expect(error.classList.contains('hidden')).toBe(false);
    expect(error.innerText).toBeTruthy();
  });

  // ── Req 12.2: Successful decompress shows "Decompression successful" ────────

  test('Req 12.2: successful decompress shows "Decompression successful" status', async () => {
    attachFile('test.z');

    const mockBlob = new Blob(['decompressed content'], { type: 'text/plain' });
    const mockResponse = {
      ok: true,
      blob: jest.fn().mockResolvedValue(mockBlob)
    };
    global.fetch.mockResolvedValue(mockResponse);

    // Mock anchor click to avoid jsdom navigation issues
    const origCreate = document.createElement.bind(document);
    jest.spyOn(document, 'createElement').mockImplementation((tag) => {
      const el = origCreate(tag);
      if (tag === 'a') el.click = jest.fn();
      return el;
    });

    await decompressFile();

    const status = document.getElementById('status');
    const results = document.getElementById('results');
    expect(results.classList.contains('hidden')).toBe(false);
    expect(status.classList.contains('hidden')).toBe(false);
    expect(status.innerText).toBe('Decompression successful');
  });

  // ── Req 12.3: Server error during decompress shows visible error ────────────

  test('Req 12.3: server error during decompress shows visible error in results area', async () => {
    attachFile('test.z');

    const mockResponse = {
      ok: false,
      json: jest.fn().mockResolvedValue({ error: 'Decompression failed' })
    };
    global.fetch.mockResolvedValue(mockResponse);

    await decompressFile();

    const results = document.getElementById('results');
    const error = document.getElementById('error');
    expect(results.classList.contains('hidden')).toBe(false);
    expect(error.classList.contains('hidden')).toBe(false);
    expect(error.innerText).toBeTruthy();
  });
});

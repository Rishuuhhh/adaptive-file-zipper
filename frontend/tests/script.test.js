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
    <div id="dropZone"></div>
    <span id="fileName"></span>
    <div id="selectedFile" class="hidden"></div>
    <button id="compressBtn">Compress</button>
    <button id="decompressBtn">Decompress</button>
    <div id="results" class="hidden">
      <p id="error" class="hidden"></p>
      <p id="status" class="hidden"></p>
      <div id="stats" class="hidden">
        <span id="entropy"></span>
        <span id="method"></span>
        <span id="adaptive"></span>
        <span id="huffman"></span>
        <span id="time"></span>
        <span id="originalSizeDisplay">—</span>
        <span id="compressedSizeDisplay">—</span>
        <span id="sizeReduction">—</span>
      </div>
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
    global.URL.revokeObjectURL = jest.fn();
  });

  afterEach(() => {
    jest.restoreAllMocks();
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

  // ── Req 11.3: Successful compress response triggers blob download ──────────

  test('Req 11.3: successful compress response triggers blob download and hides downloadLink', async () => {
    attachFile('test.txt');

    const mockBlob = new Blob([new Uint8Array([0x01, 0x02, 0x03])], { type: 'application/octet-stream' });
    const statsObj = {
      entropy: 3.1415,
      adaptiveMethod: 'BLOCK_HUFF',
      adaptiveRatio: 0.723,
      huffmanRatio: 0.681,
      time: 1.23,
      originalSize: 11,
      compressedSize: 8
    };
    const mockHeaders = new Headers({
      'Content-Type': 'application/octet-stream',
      'X-Compression-Stats': JSON.stringify(statsObj)
    });
    const mockResponse = {
      ok: true,
      headers: mockHeaders,
      blob: jest.fn().mockResolvedValue(mockBlob)
    };
    global.fetch.mockResolvedValue(mockResponse);

    // Spy on anchor click to avoid jsdom navigation issues
    const origCreate = document.createElement.bind(document);
    jest.spyOn(document, 'createElement').mockImplementation((tag) => {
      const el = origCreate(tag);
      if (tag === 'a') el.click = jest.fn();
      return el;
    });

    await compressFile();

    // URL.createObjectURL should have been called with the blob
    expect(URL.createObjectURL).toHaveBeenCalledWith(mockBlob);
    // URL.revokeObjectURL should have been called
    expect(URL.revokeObjectURL).toHaveBeenCalled();
    // downloadLink should remain hidden (blob-driven download)
    const link = document.getElementById('downloadLink');
    expect(link.classList.contains('hidden')).toBe(true);
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
    expect(status.innerText).toBe('Decompression done — file downloaded');
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

// ─── Task 5.2–5.5: Properties 1–4 — formatSize and formatReduction ────────────

// Feature: frontend-compression-enhancements, Property 1: formatSize byte range
describe('Property 1: formatSize byte range', () => {
  // Validates: Requirements 1.3

  test('integers in [0, 1023] produce a string ending with " B" and no decimal point', () => {
    fc.assert(
      fc.property(
        fc.integer({ min: 0, max: 1023 }),
        (n) => {
          const result = formatSize(n);
          expect(result.endsWith(' B')).toBe(true);
          expect(result).not.toContain('.');
        }
      ),
      { numRuns: 200 }
    );
  });
});

// Feature: frontend-compression-enhancements, Property 2: formatSize kilobyte range
describe('Property 2: formatSize kilobyte range', () => {
  // Validates: Requirements 1.4

  test('integers in [1024, 1048575] produce a string matching /^\\d+\\.\\d KB$/', () => {
    fc.assert(
      fc.property(
        fc.integer({ min: 1024, max: 1048575 }),
        (n) => {
          const result = formatSize(n);
          expect(result).toMatch(/^\d+\.\d KB$/);
        }
      ),
      { numRuns: 200 }
    );
  });
});

// Feature: frontend-compression-enhancements, Property 3: formatSize megabyte range
describe('Property 3: formatSize megabyte range', () => {
  // Validates: Requirements 1.5

  test('integers in [1048576, 1e10] produce a string matching /^\\d+\\.\\d{2} MB$/', () => {
    fc.assert(
      fc.property(
        fc.integer({ min: 1048576, max: 1e10 }),
        (n) => {
          const result = formatSize(n);
          expect(result).toMatch(/^\d+\.\d{2} MB$/);
        }
      ),
      { numRuns: 200 }
    );
  });
});

// Feature: frontend-compression-enhancements, Property 4: formatReduction formula correctness
describe('Property 4: formatReduction formula correctness', () => {
  // Validates: Requirements 1.6, 1.7

  test('formatReduction(0, 0) returns "0.0%" (zero-division guard)', () => {
    expect(formatReduction(0, 0)).toBe('0.0%');
  });

  test('for orig > 0 and comp <= orig, result matches ((1 - comp/orig) * 100).toFixed(1) + "%"', () => {
    fc.assert(
      fc.property(
        fc.tuple(
          fc.integer({ min: 1, max: 1e9 }),
          fc.integer({ min: 0, max: 1e9 })
        ).filter(([orig, comp]) => comp <= orig),
        ([orig, comp]) => {
          const expected = ((1 - comp / orig) * 100).toFixed(1) + '%';
          expect(formatReduction(orig, comp)).toBe(expected);
        }
      ),
      { numRuns: 200 }
    );
  });
});

// ─── Task 7.4: Unit tests for updated compressFile() ─────────────────────────

describe('Task 7.4: compressFile() blob download and Stats_Panel', () => {
  // Requirements: 1.2, 2.2, 2.3, 2.4

  beforeEach(() => {
    setupDOM();
    eval(scriptSource); // eslint-disable-line no-eval
    jest.resetAllMocks();
    global.fetch = jest.fn();
    global.URL.createObjectURL = jest.fn(() => 'blob:mock-url');
    global.URL.revokeObjectURL = jest.fn();
  });

  afterEach(() => {
    jest.restoreAllMocks();
  });

  // Helper: attach a file to the file input
  function attachFile(name = 'document.txt') {
    const file = new File(['hello world'], name, { type: 'text/plain' });
    const input = document.getElementById('fileInput');
    Object.defineProperty(input, 'files', {
      value: [file],
      writable: true,
      configurable: true
    });
  }

  // Helper: build a mock successful fetch response with stats header and blob body
  function makeMockResponse(statsObj, blobContent = [0x01, 0x02]) {
    const blob = new Blob([new Uint8Array(blobContent)], { type: 'application/octet-stream' });
    const headers = new Headers({
      'Content-Type': 'application/octet-stream',
      'X-Compression-Stats': JSON.stringify(statsObj)
    });
    return {
      ok: true,
      headers,
      blob: jest.fn().mockResolvedValue(blob)
    };
  }

  // Spy on anchor creation to capture the download <a> element
  function spyOnAnchorCreate() {
    const anchors = [];
    const origCreate = document.createElement.bind(document);
    jest.spyOn(document, 'createElement').mockImplementation((tag) => {
      const el = origCreate(tag);
      if (tag === 'a') {
        el.click = jest.fn();
        anchors.push(el);
      }
      return el;
    });
    return anchors;
  }

  const sampleStats = {
    entropy: 3.85,
    adaptiveMethod: 'BLOCK_HUFF',
    adaptiveRatio: 0.72,
    huffmanRatio: 0.68,
    time: 1.23,
    originalSize: 11,
    compressedSize: 8
  };

  // ── 2.2 / 2.3: URL.createObjectURL is called with the blob ─────────────────

  test('URL.createObjectURL is called with the response blob', async () => {
    attachFile('report.txt');
    const mockBlob = new Blob([new Uint8Array([0xAB])], { type: 'application/octet-stream' });
    const headers = new Headers({
      'Content-Type': 'application/octet-stream',
      'X-Compression-Stats': JSON.stringify(sampleStats)
    });
    global.fetch.mockResolvedValue({
      ok: true,
      headers,
      blob: jest.fn().mockResolvedValue(mockBlob)
    });
    spyOnAnchorCreate();

    await compressFile();

    expect(URL.createObjectURL).toHaveBeenCalledWith(mockBlob);
  });

  // ── 2.2: download <a> has a download attribute ending in ".z" ──────────────

  test('download <a> element has download attribute ending in ".z"', async () => {
    attachFile('myfile.txt');
    const anchors = spyOnAnchorCreate();
    global.fetch.mockResolvedValue(makeMockResponse(sampleStats));

    await compressFile();

    const downloadAnchor = anchors.find(a => a.download);
    expect(downloadAnchor).toBeDefined();
    expect(downloadAnchor.download).toBe('myfile.txt.z');
  });

  // ── 2.3: URL.revokeObjectURL is called after click ─────────────────────────

  test('URL.revokeObjectURL is called after the anchor click', async () => {
    attachFile('data.bin');
    spyOnAnchorCreate();
    global.fetch.mockResolvedValue(makeMockResponse(sampleStats));

    await compressFile();

    expect(URL.revokeObjectURL).toHaveBeenCalledWith('blob:mock-url');
  });

  // ── 1.2 / 2.4: Stats_Panel fields are populated from X-Compression-Stats ───

  test('all Stats_Panel fields are populated after successful compress', async () => {
    attachFile('test.txt');
    spyOnAnchorCreate();
    global.fetch.mockResolvedValue(makeMockResponse(sampleStats));

    await compressFile();

    expect(document.getElementById('entropy').innerText).not.toBe('');
    expect(document.getElementById('method').innerText).toBe('BLOCK_HUFF');
    expect(document.getElementById('adaptive').innerText).not.toBe('');
    expect(document.getElementById('huffman').innerText).not.toBe('');
    expect(document.getElementById('time').innerText).not.toBe('');
  });

  // ── 1.2: originalSizeDisplay, compressedSizeDisplay, sizeReduction populated

  test('Size_Card fields (originalSizeDisplay, compressedSizeDisplay, sizeReduction) are populated', async () => {
    attachFile('test.txt');
    spyOnAnchorCreate();
    global.fetch.mockResolvedValue(makeMockResponse(sampleStats));

    await compressFile();

    const origDisplay = document.getElementById('originalSizeDisplay').innerText;
    const compDisplay = document.getElementById('compressedSizeDisplay').innerText;
    const reduction   = document.getElementById('sizeReduction').innerText;

    // originalSize=11 → "11 B", compressedSize=8 → "8 B"
    expect(origDisplay).toBe('11 B');
    expect(compDisplay).toBe('8 B');
    // reduction = (1 - 8/11) * 100 = 27.3%
    expect(reduction).toBe('27.3%');
  });

  // ── 2.2 / 2.3: Non-OK response shows error alert ───────────────────────────

  test('non-OK response shows error alert and does not trigger download', async () => {
    attachFile('test.txt');
    global.fetch.mockResolvedValue({
      ok: false,
      json: jest.fn().mockResolvedValue({ error: 'Compression failed: bad input' })
    });

    await compressFile();

    const error = document.getElementById('error');
    const results = document.getElementById('results');
    expect(results.classList.contains('hidden')).toBe(false);
    expect(error.classList.contains('hidden')).toBe(false);
    expect(error.innerText).toBe('Compression failed: bad input');
    expect(URL.createObjectURL).not.toHaveBeenCalled();
  });

  // ── Graceful fallback when X-Compression-Stats header is missing ────────────

  test('missing X-Compression-Stats header leaves size fields as "—"', async () => {
    attachFile('test.txt');
    spyOnAnchorCreate();
    const blob = new Blob([new Uint8Array([0x01])], { type: 'application/octet-stream' });
    global.fetch.mockResolvedValue({
      ok: true,
      headers: new Headers({ 'Content-Type': 'application/octet-stream' }),
      blob: jest.fn().mockResolvedValue(blob)
    });

    await compressFile();

    expect(document.getElementById('originalSizeDisplay').innerText).toBe('—');
    expect(document.getElementById('compressedSizeDisplay').innerText).toBe('—');
    expect(document.getElementById('sizeReduction').innerText).toBe('—');
    // Download should still proceed
    expect(URL.createObjectURL).toHaveBeenCalled();
  });
});

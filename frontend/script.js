const API = "";

// Number-format helper functions.
function fmtEnt(v)  { return `${Number(v).toFixed(4)} bits/char`; }
function fmtRat(v)  { return `${(Number(v) * 100).toFixed(1)}%`; }
function fmtTime(v) { return `${Number(v).toFixed(2)} ms`; }

// Aliases for test compatibility.
function formatEntropy(v) { return fmtEnt(v); }
function formatRatio(v)   { return fmtRat(v); }
function formatTime(v)    { return fmtTime(v); }

// Format a byte count as "512 B", "12.3 KB", or "3.45 MB".
function formatSize(bytes) {
    if (bytes < 1024)        return `${bytes} B`;
    if (bytes < 1048576)     return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / 1048576).toFixed(2)} MB`;
}

// Format size reduction as a percentage with one decimal place.
// Returns "0.0%" when originalSize === 0 to avoid division by zero.
function formatReduction(originalSize, compressedSize) {
    if (originalSize === 0) return "0.0%";
    return `${((1 - compressedSize / originalSize) * 100).toFixed(1)}%`;
}

// Show error message and hide unrelated sections.
function showErr(msg) {
    const el = document.getElementById("error");
    el.innerText = msg;
    el.classList.remove("hidden");
    document.getElementById("status").classList.add("hidden");
    document.getElementById("stats").classList.add("hidden");
}

// Show status message.
function showStatus(msg) {
    const el = document.getElementById("status");
    el.innerText = msg;
    el.classList.remove("hidden");
    document.getElementById("error").classList.add("hidden");
}

// Hide status and error messages.
function clearMsgs() {
    document.getElementById("error").classList.add("hidden");
    document.getElementById("status").classList.add("hidden");
}

function showResults() {
    document.getElementById("results").classList.remove("hidden");
}

// Handle file selection and drag-drop input.
document.getElementById("fileInput").addEventListener("change", function () {
    setFile(this.files[0]);
});

const dz = document.getElementById("dropZone");

dz.addEventListener("click", () => document.getElementById("fileInput").click());

dz.addEventListener("dragover", (e) => {
    e.preventDefault();
    dz.classList.add("drag-over");
});

dz.addEventListener("dragleave", () => dz.classList.remove("drag-over"));

dz.addEventListener("drop", (e) => {
    e.preventDefault();
    dz.classList.remove("drag-over");
    const f = e.dataTransfer.files[0];
    if (f) {
        const dt = new DataTransfer();
        dt.items.add(f);
        document.getElementById("fileInput").files = dt.files;
        setFile(f);
    }
});

function setFile(f) {
    if (!f) return;
    document.getElementById("fileName").innerText = f.name;
    document.getElementById("selectedFile").classList.remove("hidden");
}

// Compress button handler.
async function compressFile() {
    const fi = document.getElementById("fileInput");
    if (!fi.files.length) { alert("Please select a file first"); return; }

    const btn = document.getElementById("compressBtn");
    btn.disabled = true;
    btn.innerHTML = '<span class="btn-icon">⏳</span> Compressing...';

    const originalFilename = fi.files[0].name;
    const fd = new FormData();
    fd.append("file", fi.files[0]);

    try {
        const res = await fetch('/compress', { method: "POST", body: fd });

        showResults();

        if (!res.ok) {
            let errMsg = "Compression failed";
            try { errMsg = (await res.json()).error || errMsg; } catch (_) {}
            showErr(errMsg);
            return;
        }

        // Parse stats from response header; fall back to {} on parse failure.
        let stats = {};
        try {
            const statsHeader = res.headers.get('X-Compression-Stats');
            if (statsHeader) stats = JSON.parse(statsHeader);
        } catch (_) {}

        // Read the binary body as a Blob.
        const blob = await res.blob();

        // Trigger programmatic download using a temporary object URL.
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = originalFilename + '.z';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);

        clearMsgs();

        // Populate Stats_Panel from the header stats.
        document.getElementById("entropy").innerText  = fmtEnt(stats.entropy);
        document.getElementById("method").innerText   = stats.adaptiveMethod || "—";
        document.getElementById("adaptive").innerText = fmtRat(stats.adaptiveRatio);
        document.getElementById("huffman").innerText  = fmtRat(stats.huffmanRatio);
        document.getElementById("time").innerText     = fmtTime(stats.time);

        // Populate Size_Card.
        const origSize = stats.originalSize;
        const compSize = stats.compressedSize;
        document.getElementById("originalSizeDisplay").innerText =
            (origSize !== undefined) ? formatSize(origSize) : "—";
        document.getElementById("compressedSizeDisplay").innerText =
            (compSize !== undefined) ? formatSize(compSize) : "—";
        document.getElementById("sizeReduction").innerText =
            (origSize !== undefined && compSize !== undefined)
                ? formatReduction(origSize, compSize)
                : "—";

        document.getElementById("stats").classList.remove("hidden");

        // Hide the legacy downloadLink element (download is now blob-driven).
        document.getElementById("downloadLink").classList.add("hidden");

    } catch (e) {
        showResults();
        showErr("Could not reach the server. Is it running on port 3000?");
    } finally {
        btn.disabled = false;
        btn.innerHTML = '<span class="btn-icon">🗜️</span> Compress';
    }
}

// Decompress button handler.
async function decompressFile() {
    const fi = document.getElementById("fileInput");
    if (!fi.files.length) { alert("Please select a file first"); return; }

    const btn = document.getElementById("decompressBtn");
    btn.disabled = true;
    btn.innerHTML = '<span class="btn-icon">⏳</span> Decompressing...';

    const fd = new FormData();
    fd.append("file", fi.files[0]);

    try {
        const res = await fetch('/decompress', { method: "POST", body: fd });

        showResults();

        if (!res.ok) {
            let msg = "Decompression failed";
            try {
                const body = await res.json();
                msg = body.detail || body.error || msg;
            } catch (_) {}
            showErr(msg);
            return;
        }

        const blob = await res.blob();
        const url  = URL.createObjectURL(blob);
        const a    = document.createElement("a");
        a.href     = url;
        a.download = "output.txt";
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);

        clearMsgs();
        document.getElementById("stats").classList.add("hidden");
        document.getElementById("downloadLink").classList.add("hidden");
        showStatus("Decompression done — file downloaded");

    } catch (e) {
        showResults();
        showErr("Could not reach the server. Is it running on port 3000?");
    } finally {
        btn.disabled = false;
        btn.innerHTML = '<span class="btn-icon">📂</span> Decompress';
    }
}

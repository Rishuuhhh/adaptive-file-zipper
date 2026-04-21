const API = "";

// Number-format helper functions.
function fmtEnt(v)  { return `${Number(v).toFixed(4)} bits/char`; }
function fmtRat(v)  { return `${(Number(v) * 100).toFixed(1)}%`; }
function fmtTime(v) { return `${Number(v).toFixed(2)} ms`; }

// Aliases for test compatibility.
function formatEntropy(v) { return fmtEnt(v); }
function formatRatio(v)   { return fmtRat(v); }
function formatTime(v)    { return fmtTime(v); }

function getById(id) {
    return document.getElementById(id);
}

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
    const el = getById("error");
    el.innerText = msg;
    el.classList.remove("hidden");
    getById("status").classList.add("hidden");
    getById("stats").classList.add("hidden");
}

// Show status message.
function showStatus(msg) {
    const el = getById("status");
    el.innerText = msg;
    el.classList.remove("hidden");
    getById("error").classList.add("hidden");
}

// Hide status and error messages.
function clearMsgs() {
    getById("error").classList.add("hidden");
    getById("status").classList.add("hidden");
}

function showResults() {
    getById("results").classList.remove("hidden");
}

function setActionButtonState(buttonId, isBusy, busyLabel, idleLabel) {
    const button = getById(buttonId);
    button.disabled = isBusy;
    button.innerHTML = isBusy ? busyLabel : idleLabel;
}

function requestBodyForSelectedFile(fileInputElement) {
    const selectedFile = fileInputElement.files[0];
    const formData = new FormData();
    formData.append("file", selectedFile);
    return formData;
}

async function parseErrorMessage(response, fallbackMessage) {
    try {
        const responseBody = await response.json();
        return responseBody.detail || responseBody.error || fallbackMessage;
    } catch (_) {
        return fallbackMessage;
    }
}

function parseDownloadNameFromDisposition(dispositionHeader, fallbackName) {
    if (!dispositionHeader) return fallbackName;
    const match = dispositionHeader.match(/filename="?([^"]+)"?/);
    return match ? match[1] : fallbackName;
}

function triggerBlobDownload(blob, filename) {
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = filename;

    document.body.appendChild(anchor);
    anchor.click();
    document.body.removeChild(anchor);

    URL.revokeObjectURL(url);
}

function updateCompressionStats(stats) {
    getById("entropy").innerText = fmtEnt(stats.entropy);
    getById("method").innerText = stats.adaptiveMethod || "—";
    getById("adaptive").innerText = fmtRat(stats.adaptiveRatio);
    getById("huffman").innerText = fmtRat(stats.huffmanRatio);
    getById("time").innerText = fmtTime(stats.time);

    const originalSize = stats.originalSize;
    const compressedSize = stats.compressedSize;

    getById("originalSizeDisplay").innerText =
        (originalSize !== undefined) ? formatSize(originalSize) : "—";
    getById("compressedSizeDisplay").innerText =
        (compressedSize !== undefined) ? formatSize(compressedSize) : "—";
    getById("sizeReduction").innerText =
        (originalSize !== undefined && compressedSize !== undefined)
            ? formatReduction(originalSize, compressedSize)
            : "—";

    getById("stats").classList.remove("hidden");
    getById("downloadLink").classList.add("hidden");
}

function statsFromResponseHeaders(headers) {
    try {
        const statsHeader = headers.get("X-Compression-Stats");
        return statsHeader ? JSON.parse(statsHeader) : {};
    } catch (_) {
        return {};
    }
}

// Handle file selection and drag-drop input.
getById("fileInput").addEventListener("change", function () {
    setFile(this.files[0]);
});

const dz = getById("dropZone");

dz.addEventListener("click", () => getById("fileInput").click());

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
        getById("fileInput").files = dt.files;
        setFile(f);
    }
});

function setFile(f) {
    if (!f) return;
    getById("fileName").innerText = f.name;
    getById("selectedFile").classList.remove("hidden");
}

// Compress button handler.
async function compressFile() {
    const fi = getById("fileInput");
    if (!fi.files.length) {
        alert("Please choose a file first.");
        return;
    }

    setActionButtonState(
        "compressBtn",
        true,
        '<span class="btn-icon">⏳</span> Compressing...',
        '<span class="btn-icon">🗜️</span> Compress'
    );

    const originalFilename = fi.files[0].name;
    const fd = requestBodyForSelectedFile(fi);

    try {
        const res = await fetch(`${API}/compress`, { method: "POST", body: fd });

        showResults();

        if (!res.ok) {
            showErr(await parseErrorMessage(res, "Compression failed"));
            return;
        }

        const stats = statsFromResponseHeaders(res.headers);
        const blob = await res.blob();
        triggerBlobDownload(blob, originalFilename + ".z");

        clearMsgs();
        updateCompressionStats(stats);

    } catch (e) {
        showResults();
        showErr("Could not reach the server. Is it running on port 3000?");
    } finally {
        setActionButtonState(
            "compressBtn",
            false,
            '<span class="btn-icon">⏳</span> Compressing...',
            '<span class="btn-icon">🗜️</span> Compress'
        );
    }
}

// Decompress button handler.
async function decompressFile() {
    const fi = getById("fileInput");
    if (!fi.files.length) {
        alert("Please choose a file first.");
        return;
    }

    setActionButtonState(
        "decompressBtn",
        true,
        '<span class="btn-icon">⏳</span> Decompressing...',
        '<span class="btn-icon">📂</span> Decompress'
    );

    const fd = requestBodyForSelectedFile(fi);

    try {
        const res = await fetch(`${API}/decompress`, { method: "POST", body: fd });

        showResults();

        if (!res.ok) {
            showErr(await parseErrorMessage(res, "Decompression failed"));
            return;
        }

        const blob = await res.blob();

        const fallbackName = fi.files[0].name.endsWith(".z")
            ? fi.files[0].name.slice(0, -2)
            : fi.files[0].name;
        const downloadName = parseDownloadNameFromDisposition(
            res.headers && res.headers.get ? res.headers.get("Content-Disposition") : null,
            fallbackName
        );

        triggerBlobDownload(blob, downloadName);

        clearMsgs();
        getById("stats").classList.add("hidden");
        getById("downloadLink").classList.add("hidden");
        showStatus("Decompression done — file downloaded");

    } catch (e) {
        showResults();
        showErr("Could not reach the server. Is it running on port 3000?");
    } finally {
        setActionButtonState(
            "decompressBtn",
            false,
            '<span class="btn-icon">⏳</span> Decompressing...',
            '<span class="btn-icon">📂</span> Decompress'
        );
    }
}

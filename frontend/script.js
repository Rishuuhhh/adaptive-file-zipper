const API = "";

// Number-format helper functions.
function fmtEnt(v)  { return `${Number(v).toFixed(4)} bits/char`; }
function fmtRat(v)  { return `${(Number(v) * 100).toFixed(1)}%`; }
function fmtTime(v) { return `${Number(v).toFixed(2)} ms`; }

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

    const fd = new FormData();
    fd.append("file", fi.files[0]);

    try {
        const res  = await fetch('/compress', { method: "POST", body: fd });
        const data = await res.json();

        showResults();

        if (!res.ok) { showErr(data.error || "Compression failed"); return; }

        clearMsgs();

        document.getElementById("entropy").innerText  = fmtEnt(data.entropy);
        document.getElementById("method").innerText   = data.adaptiveMethod;
        document.getElementById("adaptive").innerText = fmtRat(data.adaptiveRatio);
        document.getElementById("huffman").innerText  = fmtRat(data.huffmanRatio);
        document.getElementById("time").innerText     = fmtTime(data.time);

        document.getElementById("stats").classList.remove("hidden");

        const lnk = document.getElementById("downloadLink");
        lnk.href = data.download;
        lnk.classList.remove("hidden");

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
            try { msg = (await res.json()).error || msg; } catch (_) {}
            showErr(msg);
            return;
        }

        const blob = await res.blob();
        const url  = window.URL.createObjectURL(blob);
        const a    = document.createElement("a");
        a.href     = url;
        a.download = "output.txt";
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);

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

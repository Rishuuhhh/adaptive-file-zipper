const API = "";

// ── Formatting helpers ────────────────────────────────────────────────────────

function formatEntropy(val) {
    return `${Number(val).toFixed(4)} bits/char`;
}

function formatRatio(val) {
    return `${(Number(val) * 100).toFixed(1)}%`;
}

function formatTime(val) {
    return `${Number(val).toFixed(2)} ms`;
}

// ── UI helpers ────────────────────────────────────────────────────────────────

function showError(msg) {
    const err = document.getElementById("error");
    err.innerText = msg;
    err.classList.remove("hidden");
    document.getElementById("status").classList.add("hidden");
    document.getElementById("stats").classList.add("hidden");
}

function showStatus(msg) {
    const status = document.getElementById("status");
    status.innerText = msg;
    status.classList.remove("hidden");
    document.getElementById("error").classList.add("hidden");
}

function clearMessages() {
    document.getElementById("error").classList.add("hidden");
    document.getElementById("status").classList.add("hidden");
}

function showResults() {
    document.getElementById("results").classList.remove("hidden");
}

// ── File selection & drag-drop ────────────────────────────────────────────────

document.getElementById("fileInput").addEventListener("change", function () {
    updateSelectedFile(this.files[0]);
});

const dropZone = document.getElementById("dropZone");

dropZone.addEventListener("click", () => document.getElementById("fileInput").click());

dropZone.addEventListener("dragover", (e) => {
    e.preventDefault();
    dropZone.classList.add("drag-over");
});

dropZone.addEventListener("dragleave", () => dropZone.classList.remove("drag-over"));

dropZone.addEventListener("drop", (e) => {
    e.preventDefault();
    dropZone.classList.remove("drag-over");
    const file = e.dataTransfer.files[0];
    if (file) {
        // Assign to the file input so the rest of the code can read it
        const dt = new DataTransfer();
        dt.items.add(file);
        document.getElementById("fileInput").files = dt.files;
        updateSelectedFile(file);
    }
});

function updateSelectedFile(file) {
    if (!file) return;
    const el = document.getElementById("selectedFile");
    document.getElementById("fileName").innerText = file.name;
    el.classList.remove("hidden");
}

// ── Compress ──────────────────────────────────────────────────────────────────

async function compressFile() {
    const fileInput = document.getElementById("fileInput");

    if (!fileInput.files.length) {
        alert("Please select a file first");
        return;
    }

    const btn = document.getElementById("compressBtn");
    btn.disabled = true;
    btn.innerHTML = `<span class="btn-icon">⏳</span> Compressing...`;

    const formData = new FormData();
    formData.append("file", fileInput.files[0]);

    try {
        const res = await fetch(`/compress`, { method: "POST", body: formData });
        const data = await res.json();

        showResults();

        if (!res.ok) {
            showError(data.error || "Compression failed");
            return;
        }

        clearMessages();

        document.getElementById("entropy").innerText = formatEntropy(data.entropy);
        document.getElementById("method").innerText  = data.adaptiveMethod;
        document.getElementById("adaptive").innerText = formatRatio(data.adaptiveRatio);
        document.getElementById("huffman").innerText  = formatRatio(data.huffmanRatio);
        document.getElementById("time").innerText     = formatTime(data.time);

        document.getElementById("stats").classList.remove("hidden");

        const link = document.getElementById("downloadLink");
        link.href = data.download;
        link.classList.remove("hidden");

    } catch (err) {
        showResults();
        showError("Could not reach the server. Is it running on port 3000?");
    } finally {
        btn.disabled = false;
        btn.innerHTML = `<span class="btn-icon">🗜️</span> Compress`;
    }
}

// ── Decompress ────────────────────────────────────────────────────────────────

async function decompressFile() {
    const fileInput = document.getElementById("fileInput");

    if (!fileInput.files.length) {
        alert("Please select a file first");
        return;
    }

    const btn = document.getElementById("decompressBtn");
    btn.disabled = true;
    btn.innerHTML = `<span class="btn-icon">⏳</span> Decompressing...`;

    const formData = new FormData();
    formData.append("file", fileInput.files[0]);

    try {
        const res = await fetch(`/decompress`, { method: "POST", body: formData });

        showResults();

        if (!res.ok) {
            let msg = "Decompression failed";
            try { msg = (await res.json()).error || msg; } catch (_) {}
            showError(msg);
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

        clearMessages();
        document.getElementById("stats").classList.add("hidden");
        document.getElementById("downloadLink").classList.add("hidden");
        showStatus("✅ Decompression successful — file downloaded");

    } catch (err) {
        showResults();
        showError("Could not reach the server. Is it running on port 3000?");
    } finally {
        btn.disabled = false;
        btn.innerHTML = `<span class="btn-icon">📂</span> Decompress`;
    }
}

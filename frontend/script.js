function el(id) {
    return document.getElementById(id);
}

function formatSize(bytes) {
    if (bytes < 1024) return bytes + " B";
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB";
    return (bytes / 1048576).toFixed(2) + " MB";
}

function formatEntropy(v) { return Number(v).toFixed(4) + " bits/char"; }
function formatRatio(v)   { return (Number(v) * 100).toFixed(1) + "%"; }
function formatTime(v)    { return Number(v).toFixed(2) + " ms"; }

el("fileInput").addEventListener("change", function() {
    if (this.files[0]) {
        el("fileName").innerText = this.files[0].name;
        el("fileLabel").innerText = "Change file";
    }
});

function showError(msg) {
    el("error").innerText = msg;
    el("error").classList.remove("hidden");
    el("status").classList.add("hidden");
}

function showStatus(msg) {
    el("status").innerText = msg;
    el("status").classList.remove("hidden");
    el("error").classList.add("hidden");
}

function clearMessages() {
    el("error").classList.add("hidden");
    el("status").classList.add("hidden");
}

async function compressFile() {
    var fileInput = el("fileInput");
    if (!fileInput.files.length) {
        alert("Please select a file first.");
        return;
    }

    var btn = el("compressBtn");
    btn.disabled = true;
    btn.innerText = "Compressing...";

    var filename = fileInput.files[0].name;
    var fd = new FormData();
    fd.append("file", fileInput.files[0]);

    try {
        var res = await fetch("/compress", { method: "POST", body: fd });

        if (!res.ok) {
            var errData = await res.json().catch(function() { return {}; });
            showError(errData.error || "Compression failed.");
            return;
        }

        var statsHeader = res.headers.get("X-Compression-Stats");
        var stats = statsHeader ? JSON.parse(statsHeader) : {};

        var blob = await res.blob();
        var url = URL.createObjectURL(blob);

        var downloadBtn = el("downloadBtn");
        downloadBtn.href = url;
        downloadBtn.download = filename + ".z";
        downloadBtn.innerText = "Download " + filename + ".z";
        el("result").classList.remove("hidden");

        clearMessages();
        showStats(stats);

    } catch (e) {
        showError("Could not connect to server.");
    } finally {
        btn.disabled = false;
        btn.innerText = "Compress";
    }
}

async function decompressFile() {
    var fileInput = el("fileInput");
    if (!fileInput.files.length) {
        alert("Please select a file first.");
        return;
    }

    var btn = el("decompressBtn");
    btn.disabled = true;
    btn.innerText = "Decompressing...";

    var fd = new FormData();
    fd.append("file", fileInput.files[0]);

    try {
        var res = await fetch("/decompress", { method: "POST", body: fd });

        if (!res.ok) {
            var errData = await res.json().catch(function() { return {}; });
            showError(errData.detail || errData.error || "Decompression failed.");
            return;
        }

        var blob = await res.blob();
        var url = URL.createObjectURL(blob);

        var disposition = res.headers.get("Content-Disposition") || "";
        var match = disposition.match(/filename="?([^"]+)"?/);
        var downloadName = match ? match[1] : fileInput.files[0].name.replace(/\.z$/i, "");

        var downloadBtn = el("downloadBtn");
        downloadBtn.href = url;
        downloadBtn.download = downloadName;
        downloadBtn.innerText = "Download " + downloadName;
        el("result").classList.remove("hidden");

        clearMessages();
        el("stats").classList.add("hidden");
        showStatus("Decompression complete.");

    } catch (e) {
        showError("Could not connect to server.");
    } finally {
        btn.disabled = false;
        btn.innerText = "Decompress";
    }
}

function showStats(stats) {
    el("method").innerText  = stats.adaptiveMethod || "—";
    el("entropy").innerText = stats.entropy !== undefined ? formatEntropy(stats.entropy) : "—";
    el("origSize").innerText = stats.originalSize !== undefined ? formatSize(stats.originalSize) : "—";
    el("compSize").innerText = stats.compressedSize !== undefined ? formatSize(stats.compressedSize) : "—";
    el("time").innerText    = stats.time !== undefined ? formatTime(stats.time) : "—";

    if (stats.originalSize && stats.compressedSize) {
        var saved = ((1 - stats.compressedSize / stats.originalSize) * 100).toFixed(1);
        el("reduction").innerText = saved + "%";
    } else {
        el("reduction").innerText = "—";
    }

    if (stats.adaptiveMethod === "NONE") {
        el("noneNote").classList.remove("hidden");
    } else {
        el("noneNote").classList.add("hidden");
    }

    el("stats").classList.remove("hidden");
}

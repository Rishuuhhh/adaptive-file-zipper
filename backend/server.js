"use strict";
const express      = require("express");
const multer       = require("multer");
const cors         = require("cors");
const { execFile } = require("child_process");
const path         = require("path");
const fs           = require("fs");
const app = express();
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, "../frontend")));
app.get("/", (req, res) => {
    res.sendFile(path.join(__dirname, "../frontend/index.html"));
});
const zipperCandidates = [
    process.env.ZIPPER_PATH,
    path.join(__dirname, "../zipper"),
    path.join(__dirname, "./zipper")
].filter(Boolean);
const zipperPath = zipperCandidates.find(p => fs.existsSync(p));
const uploadDir = path.join(__dirname, "../data");
const resultDir = path.join(__dirname, "../results");
if (!fs.existsSync(uploadDir)) fs.mkdirSync(uploadDir, { recursive: true });
if (!fs.existsSync(resultDir)) fs.mkdirSync(resultDir, { recursive: true });
if (!zipperPath) {
    console.error("WARNING: zipper binary not found. Checked:", zipperCandidates);
} else {
    console.log("Using zipper binary:", zipperPath);
}
const storage = multer.diskStorage({
    destination: (req, file, cb) => cb(null, uploadDir),
    filename:    (req, file, cb) => cb(null, Date.now() + "_" + file.originalname)
});
const upload = multer({
    storage,
    limits: { fileSize: 50 * 1024 * 1024 }
});
function deleteTempFile(filePath) {
    if (!filePath) return;
    fs.unlink(filePath, () => {});
}
function tryParseJson(text) {
    try {
        return JSON.parse(text);
    } catch (_) {
        return null;
    }
}
function getZipperErrorMessage(err, stderr, stdout) {
    const parsed = tryParseJson(stdout);
    if (parsed && parsed.error) return parsed.error;
    return (stderr || err.message || "Unknown error").trim();
}
function getRestoredFilename(stdout, uploadedFilename) {
    const parsed = tryParseJson(stdout);
    if (parsed && parsed.originalFilename) {
        return parsed.originalFilename;
    }
    if (uploadedFilename.endsWith(".z")) {
        return uploadedFilename.slice(0, -2);
    }
    return uploadedFilename;
}
app.post("/compress", upload.single("file"), (req, res) => {
    if (!req.file) {
        return res.status(400).json({ error: "No file uploaded" });
    }
    if (!zipperPath) {
        return res.status(500).json({ error: "Server misconfiguration: zipper binary not found" });
    }
    const inputPath        = req.file.path;
    const outputPath       = path.join(resultDir, "compressed_" + req.file.filename + ".z");
    const originalFilename = req.file.originalname;
    execFile(zipperPath, ["compress", inputPath, outputPath], (err, stdout, stderr) => {
        deleteTempFile(inputPath);
        if (err) {
            const detail = getZipperErrorMessage(err, stderr, stdout);
            console.error("Compression failed:", detail);
            return res.status(500).json({ error: "Compression failed", detail });
        }
        const stats = tryParseJson(stdout);
        if (!stats) {
            console.error("Could not parse compressor output:", stdout);
            return res.status(500).json({ error: "Invalid output from compressor" });
        }
        let compressedBuffer;
        try {
            compressedBuffer = fs.readFileSync(outputPath);
        } catch (readErr) {
            console.error("Could not read compressed output:", readErr.message);
            return res.status(500).json({ error: "Could not read compressed file" });
        }
        const compressionStats = {
            entropy:        stats.entropy,
            adaptiveMethod: stats.adaptiveMethod,
            adaptiveRatio:  stats.adaptiveRatio,
            huffmanRatio:   stats.huffmanRatio,
            time:           stats.time,
            originalSize:   req.file.size,
            compressedSize: compressedBuffer.length
        };
        res.set("Content-Type", "application/octet-stream");
        res.set("Content-Disposition", `attachment; filename="${originalFilename}.z"`);
        res.set("X-Compression-Stats", JSON.stringify(compressionStats));
        res.send(compressedBuffer);
        deleteTempFile(outputPath);
    });
});
app.post("/decompress", upload.single("file"), (req, res) => {
    if (!req.file) {
        return res.status(400).json({ error: "No file uploaded" });
    }
    if (!zipperPath) {
        return res.status(500).json({ error: "Server misconfiguration: zipper binary not found" });
    }
    const inputPath  = req.file.path;
    const outputPath = path.join(resultDir, "decompressed_" + req.file.filename);
    execFile(zipperPath, ["decompress", inputPath, outputPath], (err, stdout, stderr) => {
        deleteTempFile(inputPath);
        if (err) {
            const detail = getZipperErrorMessage(err, stderr, stdout);
            console.error("Decompression failed:", detail);
            return res.status(500).json({ error: "Decompression failed", detail });
        }
        if (!fs.existsSync(outputPath)) {
            return res.status(500).json({ error: "Decompressed file not found" });
        }
        const downloadName = getRestoredFilename(stdout, req.file.originalname);
        res.download(outputPath, downloadName, (downloadErr) => {
            deleteTempFile(outputPath);
            if (downloadErr && !res.headersSent) {
                res.status(500).json({ error: "Failed to send decompressed file" });
            }
        });
    });
});
app.get("/download", (req, res) => {
    const filePath = req.query.path;
    if (!filePath || typeof filePath !== "string" || filePath.includes("\0")) {
        return res.status(400).send("Invalid path");
    }
    if (!fs.existsSync(filePath)) {
        return res.status(404).send("File not found");
    }
    res.download(filePath);
});
const PORT = Number(process.env.PORT) || 3000;
app.listen(PORT, () => {
    console.log(`Server running at http://localhost:${PORT}`);
});
module.exports = app;

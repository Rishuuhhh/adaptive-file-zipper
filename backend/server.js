const express = require("express");
const multer  = require("multer");
const cors    = require("cors");
const { execFile } = require("child_process");
const path    = require("path");
const fs      = require("fs");

const app = express();
app.use(cors());
app.use(express.json());

// Serve frontend static files.
app.use(express.static(path.join(__dirname, "../frontend")));

app.get("/", (req, res) => {
    res.sendFile(path.join(__dirname, "../frontend/index.html"));
});

const zipperCandidates = [
    process.env.ZIPPER_PATH,
    path.join(__dirname, "../zipper"),
    path.join(__dirname, "./zipper")
].filter(Boolean);

const zipperPath = zipperCandidates.find((p) => fs.existsSync(p));
const uploadDir  = path.join(__dirname, "../data");
const resultDir  = path.join(__dirname, "../results");

const MAX_FILE_SIZE = 50 * 1024 * 1024;

// Create data/result directories if they do not exist.
if (!fs.existsSync(uploadDir)) fs.mkdirSync(uploadDir, { recursive: true });
if (!fs.existsSync(resultDir)) fs.mkdirSync(resultDir, { recursive: true });

if (!zipperPath) {
    console.error("Zipper binary not found. Checked:", zipperCandidates);
} else {
    console.log("Using zipper binary:", zipperPath);
}

// Keep uploaded filenames unique.
const storage = multer.diskStorage({
    destination: (req, file, cb) => cb(null, uploadDir),
    filename:    (req, file, cb) => cb(null, Date.now() + "_" + file.originalname)
});

const upload = multer({
    storage,
    limits: { fileSize: MAX_FILE_SIZE }
});

function ensureUploadPresent(req, res) {
    if (req.file) return true;
    res.status(400).json({ error: "No file uploaded" });
    return false;
}

function ensureZipperConfigured(res) {
    if (zipperPath) return true;
    res.status(500).json({ error: "Server misconfiguration: zipper binary not found" });
    return false;
}

function cleanTempFile(filePath) {
    if (!filePath) return;
    fs.unlink(filePath, () => {});
}

function parseJsonSafely(rawText) {
    try {
        return JSON.parse(rawText);
    } catch (_) {
        return null;
    }
}

function getExecutionErrorDetail(err, stderr, stdout) {
    const parsed = parseJsonSafely(stdout);
    if (parsed && parsed.error) return parsed.error;
    return (stderr || err.message || "Unknown execution error").trim();
}

function buildCompressionStats(rawStats, uploadedSize, compressedSize) {
    return {
        entropy: rawStats && rawStats.entropy,
        adaptiveMethod: rawStats && rawStats.adaptiveMethod,
        adaptiveRatio: rawStats && rawStats.adaptiveRatio,
        huffmanRatio: rawStats && rawStats.huffmanRatio,
        time: rawStats && rawStats.time,
        originalSize: uploadedSize,
        compressedSize
    };
}

function readOutputFileOrRespond(filePath, res) {
    try {
        return fs.readFileSync(filePath);
    } catch (error) {
        console.error("Could not read compressed file", { path: filePath, error: error.message });
        res.status(500).json({ error: "Could not read compressed file", detail: error.message });
        return null;
    }
}

function respondCompressionSuccess(res, originalFilename, stats, compressedBuffer) {
    res.set("Content-Type", "application/octet-stream");
    res.set("Content-Disposition", `attachment; filename="${originalFilename}.z"`);
    res.set("X-Compression-Stats", JSON.stringify(stats));
    res.send(compressedBuffer);
}

function createOutputPaths(fileRecord) {
    return {
        compressOutputPath: path.join(resultDir, "compressed_" + fileRecord.filename + ".z"),
        decompressOutputPath: path.join(resultDir, "decompressed_" + fileRecord.filename)
    };
}

function sendDecompressedFile(res, outputPath, downloadName) {
    res.download(outputPath, downloadName, (downloadErr) => {
        cleanTempFile(outputPath);
        if (downloadErr && !res.headersSent) {
            res.status(500).json({ error: "Failed to send decompressed file" });
        }
    });
}

function resolveDecompressDownloadName(stdout, uploadedFilename) {
    const parsed = parseJsonSafely(stdout);
    if (parsed && parsed.originalFilename) {
        return parsed.originalFilename;
    }

    return uploadedFilename.endsWith(".z")
        ? uploadedFilename.slice(0, -2)
        : uploadedFilename;
}

function isValidDownloadPath(targetPath) {
    if (typeof targetPath !== "string" || targetPath.trim().length === 0) {
        return false;
    }

    // Reject obvious malformed input, but keep backward compatibility for absolute paths.
    return !targetPath.includes("\0");
}

// Compress endpoint.
app.post("/compress", upload.single("file"), (req, res) => {
    if (!ensureUploadPresent(req, res)) return;
    if (!ensureZipperConfigured(res)) return;

    const inputPath = req.file.path;
    const originalFilename = req.file.originalname;
    const { compressOutputPath } = createOutputPaths(req.file);

    execFile(zipperPath, ["compress", inputPath, compressOutputPath], (err, stdout, stderr) => {
        // Always clean up the uploaded input file.
        cleanTempFile(inputPath);

        if (err) {
            const detail = getExecutionErrorDetail(err, stderr, stdout);
            console.error("Compression failed", {
                message: err.message,
                code: err.code,
                stderr
            });
            return res.status(500).json({ error: "Compression failed", detail });
        }

        const compressorResponse = parseJsonSafely(stdout);
        if (!compressorResponse) {
            console.error("Invalid compressor JSON", { stdout, stderr });
            return res.status(500).json({ error: "Invalid JSON from C++" });
        }

        const compressedBuffer = readOutputFileOrRespond(compressOutputPath, res);
        if (!compressedBuffer) return;

        const stats = buildCompressionStats(
            compressorResponse,
            req.file.size,
            compressedBuffer.length
        );

        respondCompressionSuccess(res, originalFilename, stats, compressedBuffer);

        // Clean up the compressed output file after sending.
        cleanTempFile(compressOutputPath);
    });
});

// Decompress endpoint.
app.post("/decompress", upload.single("file"), (req, res) => {
    if (!ensureUploadPresent(req, res)) return;
    if (!ensureZipperConfigured(res)) return;

    const inputPath = req.file.path;
    const { decompressOutputPath } = createOutputPaths(req.file);

    execFile(zipperPath, ["decompress", inputPath, decompressOutputPath], (err, stdout, stderr) => {
        // Always clean up the uploaded input file.
        cleanTempFile(inputPath);

        if (err) {
            const detail = getExecutionErrorDetail(err, stderr, stdout);
            console.error("Decompression failed", { message: err.message, code: err.code, detail });
            return res.status(500).json({ error: "Decompression failed", detail });
        }

        // Verify the output file exists before attempting to send it.
        if (!fs.existsSync(decompressOutputPath)) {
            return res.status(500).json({ error: "Decompressed file not found" });
        }

        const downloadName = resolveDecompressDownloadName(stdout, req.file.originalname);
        sendDecompressedFile(res, decompressOutputPath, downloadName);
    });
});

// Download endpoint for compressed files.
app.get("/download", (req, res) => {
    const requestedPath = req.query.path;
    if (!isValidDownloadPath(requestedPath)) {
        return res.status(400).send("Invalid download path");
    }

    if (!fs.existsSync(requestedPath)) {
        return res.status(404).send("File not found");
    }

    res.download(requestedPath);
});

const PORT = Number(process.env.PORT) || 3000;
app.listen(PORT, () => console.log(`Server running on http://localhost:${PORT}`));

module.exports = app;

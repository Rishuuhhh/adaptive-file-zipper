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

// 50 MB upload limit.
const MAX_FILE_SIZE = 50 * 1024 * 1024;

const upload = multer({
    storage,
    limits: { fileSize: MAX_FILE_SIZE }
});

// Compress endpoint.
app.post("/compress", upload.single("file"), (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });
    if (!zipperPath) {
        return res.status(500).json({
            error: "Server misconfiguration: zipper binary not found"
        });
    }

    const inp = req.file.path;
    const originalFilename = req.file.originalname;
    const out = path.join(resultDir, "compressed_" + req.file.filename + ".z");
    execFile(zipperPath, ["compress", inp, out], (err, stdout, stderr) => {
        // Always clean up the uploaded input file.
        fs.unlink(inp, () => {});

        if (err) {
            const detail = (stderr || err.message || "Unknown execution error").trim();
            console.error("Compression failed", {
                message: err.message,
                code: err.code,
                stderr
            });
            return res.status(500).json({ error: "Compression failed", detail });
        }

        let json;
        try {
            json = JSON.parse(stdout);
        } catch (e) {
            console.error("Invalid compressor JSON", { stdout, stderr });
            return res.status(500).json({ error: "Invalid JSON from C++" });
        }

        let buf;
        try {
            buf = fs.readFileSync(out);
        } catch (e) {
            console.error("Could not read compressed file", { path: out, error: e.message });
            return res.status(500).json({
                error: "Could not read compressed file",
                detail: e.message
            });
        }

        const stats = {
            entropy:        json.entropy,
            adaptiveMethod: json.adaptiveMethod,
            adaptiveRatio:  json.adaptiveRatio,
            huffmanRatio:   json.huffmanRatio,
            time:           json.time,
            originalSize:   req.file.size,  // actual uploaded file size from multer
            compressedSize: buf.length      // actual compressed file size on disk
        };

        res.set("Content-Type", "application/octet-stream");
        res.set("Content-Disposition", `attachment; filename="${originalFilename}.z"`);
        res.set("X-Compression-Stats", JSON.stringify(stats));
        res.send(buf);

        // Clean up the compressed output file after sending.
        fs.unlink(out, () => {});
    });
});

// Decompress endpoint.
app.post("/decompress", upload.single("file"), (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });
    if (!zipperPath) {
        return res.status(500).json({
            error: "Server misconfiguration: zipper binary not found"
        });
    }

    const inp = req.file.path;
    const out = path.join(resultDir, "decompressed_" + req.file.filename);
    execFile(zipperPath, ["decompress", inp, out], (err, stdout, stderr) => {
        // Always clean up the uploaded input file.
        fs.unlink(inp, () => {});

        if (err) {
            // The zipper prints its error message to stdout as JSON; fall back to stderr.
            let detail = "";
            try {
                const parsed = JSON.parse(stdout);
                detail = parsed.error || "";
            } catch (_) {}
            if (!detail) detail = (stderr || err.message || "Unknown execution error").trim();
            console.error("Decompression failed", { message: err.message, code: err.code, detail });
            return res.status(500).json({ error: "Decompression failed", detail });
        }

        // Verify the output file exists before attempting to send it.
        if (!fs.existsSync(out)) {
            return res.status(500).json({ error: "Decompressed file not found" });
        }

        // Determine the original filename for the download.
        // The C++ binary emits { "status": "done", "originalFilename": "..." } on stdout.
        let downloadName = "decompressed_output";
        try {
            const parsed = JSON.parse(stdout);
            if (parsed.originalFilename) downloadName = parsed.originalFilename;
        } catch (_) {}
        // Fallback: strip .z extension from the uploaded filename.
        if (downloadName === "decompressed_output") {
            const base = req.file.originalname;
            downloadName = base.endsWith(".z") ? base.slice(0, -2) : base;
        }

        res.download(out, downloadName, (downloadErr) => {
            // Clean up the output file after sending (or on error).
            fs.unlink(out, () => {});
            if (downloadErr && !res.headersSent) {
                res.status(500).json({ error: "Failed to send decompressed file" });
            }
        });
    });
});

// Download endpoint for compressed files.
app.get("/download", (req, res) => {
    const fp = req.query.path;
    if (!fs.existsSync(fp)) return res.status(404).send("File not found");
    res.download(fp);
});

const PORT = Number(process.env.PORT) || 3000;
app.listen(PORT, () => console.log(`Server running on http://localhost:${PORT}`));

module.exports = app;

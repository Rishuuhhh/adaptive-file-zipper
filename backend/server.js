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

const upload = multer({ storage });

// Compress endpoint.
app.post("/compress", upload.single("file"), (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });
    if (!zipperPath) {
        return res.status(500).json({
            error: "Server misconfiguration: zipper binary not found"
        });
    }

    const inp = req.file.path;
    const out = path.join(resultDir, "compressed_" + req.file.filename + ".z");
    execFile(zipperPath, ["compress", inp, out], (err, stdout, stderr) => {
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
        res.json({ ...json, download: `/download?path=${encodeURIComponent(out)}` });
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
    const out = path.join(resultDir, "decompressed_" + req.file.filename + ".txt");
    execFile(zipperPath, ["decompress", inp, out], (err, _stdout, stderr) => {
        if (err) {
            const detail = (stderr || err.message || "Unknown execution error").trim();
            console.error("Decompression failed", {
                message: err.message,
                code: err.code,
                stderr
            });
            return res.status(500).json({ error: "Decompression failed", detail });
        }
        res.download(out);
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

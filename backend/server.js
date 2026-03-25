const express = require("express");
const multer  = require("multer");
const cors    = require("cors");
const { exec } = require("child_process");
const path    = require("path");
const fs      = require("fs");

const app = express();
app.use(cors());
app.use(express.json());

// frontend files serve karo
app.use(express.static(path.join(__dirname, "../frontend")));

app.get("/", (req, res) => {
    res.sendFile(path.join(__dirname, "../frontend/index.html"));
});

const zipperPath = path.join(__dirname, "../zipper");
const uploadDir  = path.join(__dirname, "../data");
const resultDir  = path.join(__dirname, "../results");

// folders nahi hain toh banao
if (!fs.existsSync(uploadDir)) fs.mkdirSync(uploadDir);
if (!fs.existsSync(resultDir)) fs.mkdirSync(resultDir);

// uploaded file ka naam unique rakho
const storage = multer.diskStorage({
    destination: (req, file, cb) => cb(null, uploadDir),
    filename:    (req, file, cb) => cb(null, Date.now() + "_" + file.originalname)
});

const upload = multer({ storage });

// compress endpoint
app.post("/compress", upload.single("file"), (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const inp = req.file.path;
    const out = path.join(resultDir, "compressed_" + req.file.filename + ".z");
    const cmd = `${zipperPath} compress "${inp}" "${out}"`;

    exec(cmd, (err, stdout) => {
        if (err) {
            console.error(err);
            return res.status(500).json({ error: "Compression failed" });
        }
        let json;
        try {
            json = JSON.parse(stdout);
        } catch (e) {
            return res.status(500).json({ error: "Invalid JSON from C++" });
        }
        res.json({ ...json, download: `/download?path=${encodeURIComponent(out)}` });
    });
});

// decompress endpoint
app.post("/decompress", upload.single("file"), (req, res) => {
    if (!req.file) return res.status(400).json({ error: "No file uploaded" });

    const inp = req.file.path;
    const out = path.join(resultDir, "decompressed_" + req.file.filename + ".txt");
    const cmd = `${zipperPath} decompress "${inp}" "${out}"`;

    exec(cmd, (err) => {
        if (err) {
            console.error(err);
            return res.status(500).json({ error: "Decompression failed" });
        }
        res.download(out);
    });
});

// compressed file download karo
app.get("/download", (req, res) => {
    const fp = req.query.path;
    if (!fs.existsSync(fp)) return res.status(404).send("File not found");
    res.download(fp);
});

const PORT = 3000;
app.listen(PORT, () => console.log(`Server running on http://localhost:${PORT}`));

module.exports = app;

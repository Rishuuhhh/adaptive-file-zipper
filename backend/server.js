
const express = require("express");
const multer = require("multer");
const cors = require("cors");
const { exec } = require("child_process");
const path = require("path");
const fs = require("fs");

const app = express();
app.use(cors());
app.use(express.json());

// Serve frontend static files
app.use(express.static(path.join(__dirname, "../frontend")));

app.get("/", (req, res) => {
    res.sendFile(path.join(__dirname, "../frontend/index.html"));
});
const zipperPath = path.join(__dirname, "../zipper");
// folders
const uploadDir = path.join(__dirname, "../data");
const resultDir = path.join(__dirname, "../results");

// ensure folders exist
if (!fs.existsSync(uploadDir)) fs.mkdirSync(uploadDir);
if (!fs.existsSync(resultDir)) fs.mkdirSync(resultDir);

// multer config
const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        cb(null, uploadDir);
    },
    filename: (req, file, cb) => {
        const uniqueName = Date.now() + "_" + file.originalname;
        cb(null, uniqueName);
    }
});

const upload = multer({ storage });


app.post("/compress", upload.single("file"), (req, res) => {

    if (!req.file) {
        return res.status(400).json({ error: "No file uploaded" });
    }

    const inputPath = req.file.path;
    const outputPath = path.join(resultDir, "compressed_" + req.file.filename + ".z");

   

const command = `${zipperPath} compress "${inputPath}" "${outputPath}"`;

    exec(command, (error, stdout, stderr) => {

        if (error) {
            console.error(error);
            return res.status(500).json({ error: "Compression failed" });
        }

        let jsonOutput;

        try {
            console.log("RAW OUTPUT:", stdout);
    jsonOutput = JSON.parse(stdout);
        } catch (e) {
            return res.status(500).json({ error: "Invalid JSON from C++" });
        }

        res.json({
            ...jsonOutput,
            download: `/download?path=${encodeURIComponent(outputPath)}`
        });
    });
});


// ------------------ DECOMPRESS ------------------

app.post("/decompress", upload.single("file"), (req, res) => {

    if (!req.file) {
        return res.status(400).json({ error: "No file uploaded" });
    }

    const inputPath = req.file.path;
    const outputPath = path.join(resultDir, "decompressed_" + req.file.filename + ".txt");

   const command = `${zipperPath} decompress "${inputPath}" "${outputPath}"`;

    exec(command, (error, stdout, stderr) => {

        if (error) {
            console.error(error);
            return res.status(500).json({ error: "Decompression failed" });
        }

        res.download(outputPath);
    });
});


app.get("/download", (req, res) => {
    const filePath = req.query.path;

    if (!fs.existsSync(filePath)) {
        return res.status(404).send("File not found");
    }

    res.download(filePath);
});



const PORT = 3000;

app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});

module.exports = app;

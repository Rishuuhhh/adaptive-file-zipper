"use strict";

const request = require("supertest");
const path = require("path");
const fs = require("fs");
const os = require("os");

// ─── helpers ────────────────────────────────────────────────────────────────

function makeTmpFile(name, content) {
    const p = path.join(os.tmpdir(), name);
    fs.writeFileSync(p, content);
    return p;
}

function tryUnlink(p) {
    try { fs.unlinkSync(p); } catch (_) {}
}

// ─── Tests that need NO mocking (guards) ────────────────────────────────────

describe("missing-file guards (no mock needed)", () => {
    let app;

    beforeAll(() => {
        jest.resetModules();
        app = require("../server");
    });

    // Req 9.3
    test("POST /compress with no file → 400 { error: 'No file uploaded' }", async () => {
        const res = await request(app).post("/compress");
        expect(res.status).toBe(400);
        expect(res.body).toEqual({ error: "No file uploaded" });
    });

    // Req 10.3
    test("POST /decompress with no file → 400 { error: 'No file uploaded' }", async () => {
        const res = await request(app).post("/decompress");
        expect(res.status).toBe(400);
        expect(res.body).toEqual({ error: "No file uploaded" });
    });
});

// ─── Tests that mock execFile (compress success / failure) ──────────────────

describe("POST /compress with mocked execFile", () => {
    let app;

    // Req 9.1 / 2.1 – valid file → 200 binary with X-Compression-Stats header
    describe("execFile succeeds with valid JSON", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                execFile: jest.fn((_bin, _args, cb) => {
                    // Create a fake output file so readFileSync succeeds.
                    const outPath = _args[2];
                    require("fs").writeFileSync(outPath, Buffer.from([0x1f, 0x8b, 0x00]));
                    cb(null, JSON.stringify({
                        entropy: 3.85,
                        adaptiveMethod: "BLOCK_HUFF",
                        adaptiveRatio: 0.72,
                        huffmanRatio: 0.68,
                        time: 0.14,
                        originalSize: 24,
                        compressedSize: 17
                    }), "");
                })
            }));
            app = require("../server");
        });

        afterAll(() => {
            jest.unmock("child_process");
        });

        test("returns 200 with octet-stream content type (Req 9.1, 2.1)", async () => {
            const tmp = makeTmpFile("compress_ok.txt", "Hello world test content");
            const res = await request(app).post("/compress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(200);
            expect(res.headers["content-type"]).toMatch(/application\/octet-stream/);
        });

        test("X-Compression-Stats header is present and parseable with size fields (Req 1.1, 2.4)", async () => {
            const tmp = makeTmpFile("compress_ok2.txt", "Hello world test content");
            const res = await request(app).post("/compress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(200);
            const statsHeader = res.headers["x-compression-stats"];
            expect(statsHeader).toBeDefined();
            const parsed = JSON.parse(statsHeader);
            expect(parsed).toHaveProperty("originalSize");
            expect(parsed).toHaveProperty("compressedSize");
            expect(typeof parsed.originalSize).toBe("number");
            expect(typeof parsed.compressedSize).toBe("number");
        });

        test("Content-Disposition filename ends with .z (Req 2.2)", async () => {
            const tmp = makeTmpFile("myfile.txt", "content");
            const res = await request(app).post("/compress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(200);
            const disposition = res.headers["content-disposition"];
            expect(disposition).toMatch(/filename=".*\.z"/);
        });

        test("response body is non-empty binary (Req 2.1)", async () => {
            const tmp = makeTmpFile("compress_ok3.txt", "Hello world test content");
            const res = await request(app)
                .post("/compress")
                .attach("file", tmp)
                .buffer(true)
                .parse((res, fn) => {
                    const chunks = [];
                    res.on("data", (c) => chunks.push(c));
                    res.on("end", () => fn(null, Buffer.concat(chunks)));
                });
            tryUnlink(tmp);

            expect(res.status).toBe(200);
            expect(res.body.length).toBeGreaterThan(0);
        });
    });

    // Req 9.2 – execFile fails → 500
    describe("execFile fails", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                execFile: jest.fn((_bin, _args, cb) => {
                    cb(new Error("zipper crashed"), "", "stderr output");
                })
            }));
            app = require("../server");
        });

        afterAll(() => {
            jest.unmock("child_process");
        });

        test("returns 500 when zipper fails (Req 9.2)", async () => {
            const tmp = makeTmpFile("compress_fail.txt", "data");
            const res = await request(app).post("/compress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(500);
            expect(res.body).toHaveProperty("error", "Compression failed");
        });
    });

    // Req 3.8 – uploaded temp file is deleted after compress (success and failure)
    describe("temp-file cleanup (Req 3.8)", () => {
        let unlinkSpy;

        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                execFile: jest.fn((_bin, _args, cb) => {
                    const outPath = _args[2];
                    require("fs").writeFileSync(outPath, Buffer.from([0x00]));
                    cb(null, JSON.stringify({
                        entropy: 3.85,
                        adaptiveMethod: "BLOCK_HUFF",
                        adaptiveRatio: 0.72,
                        huffmanRatio: 0.68,
                        time: 0.14,
                        originalSize: 9,
                        compressedSize: 1
                    }), "");
                })
            }));
            app = require("../server");
        });

        beforeEach(() => {
            unlinkSpy = jest.spyOn(fs, "unlink");
        });

        afterEach(() => {
            unlinkSpy.mockRestore();
        });

        afterAll(() => {
            jest.unmock("child_process");
        });

        test("fs.unlink is called for the uploaded input file after compress", async () => {
            const tmp = makeTmpFile("cleanup_test.txt", "some data");
            const res = await request(app).post("/compress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(200);
            // At least one unlink call should have been made
            expect(unlinkSpy).toHaveBeenCalled();
        });
    });
});

// ─── GET /download backward-compat smoke test ───────────────────────────────

describe("GET /download backward-compat smoke test", () => {
    let app;
    let tmpFile;

    beforeAll(() => {
        jest.resetModules();
        app = require("../server");
        // Create a real temp file that the /download endpoint can serve.
        tmpFile = path.join(os.tmpdir(), "download_smoke_test.z");
        fs.writeFileSync(tmpFile, Buffer.from([0x1f, 0x8b, 0x00, 0x01]));
    });

    afterAll(() => {
        tryUnlink(tmpFile);
    });

    test("GET /download with a valid path → 200 (Req 2.5, 2.6)", async () => {
        const res = await request(app)
            .get("/download")
            .query({ path: tmpFile });

        expect(res.status).toBe(200);
    });
});

// ─── Tests that mock execFile (decompress success / failure) ────────────────

describe("POST /decompress with mocked execFile", () => {
    let app;

    // Req 10.1 – valid .z file → file download
    describe("execFile succeeds and creates output file", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                execFile: jest.fn((_bin, _args, cb) => {
                    // Create the output file so res.download succeeds.
                    const outPath = _args[2];
                    require("fs").writeFileSync(outPath, "decompressed content here");
                    cb(null, "", "");
                })
            }));
            app = require("../server");
        });

        afterAll(() => {
            jest.unmock("child_process");
        });

        test("returns 200 file download for a valid .z file (Req 10.1)", async () => {
            const tmp = makeTmpFile("decomp_ok.z", "fake compressed bytes");
            const res = await request(app).post("/decompress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(200);
        });
    });

    // Req 10.2 – execFile fails → 500
    describe("execFile fails", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                execFile: jest.fn((_bin, _args, cb) => {
                    cb(new Error("zipper crashed"), "", "stderr");
                })
            }));
            app = require("../server");
        });

        afterAll(() => {
            jest.unmock("child_process");
        });

        test("returns 500 when zipper fails (Req 10.2)", async () => {
            const tmp = makeTmpFile("decomp_fail.z", "bad data");
            const res = await request(app).post("/decompress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(500);
        });
    });
});

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

// ─── Tests that mock exec (compress success / failure) ──────────────────────

describe("POST /compress with mocked exec", () => {
    let app;

    // Req 9.1 – valid file → 200 with JSON stats + download URL
    describe("exec succeeds with valid JSON", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                exec: jest.fn((cmd, cb) => {
                    cb(
                        null,
                        JSON.stringify({
                            entropy: 3.85,
                            adaptiveMethod: "HUFFMAN",
                            adaptiveRatio: 3.91,
                            huffmanRatio: 3.91,
                            time: 0.14
                        }),
                        ""
                    );
                })
            }));
            app = require("../server");
        });

        afterAll(() => {
            jest.unmock("child_process");
        });

        test("returns 200 with stats and download URL (Req 9.1)", async () => {
            const tmp = makeTmpFile("compress_ok.txt", "Hello world test content");
            const res = await request(app).post("/compress").attach("file", tmp);
            tryUnlink(tmp);

            expect(res.status).toBe(200);
            expect(res.body).toHaveProperty("download");
            expect(res.body).toHaveProperty("entropy");
            expect(res.body).toHaveProperty("adaptiveMethod");
        });
    });

    // Req 9.2 – exec fails → 500
    describe("exec fails", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                exec: jest.fn((cmd, cb) => {
                    cb(new Error("zipper crashed"), "", "stderr");
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
        });
    });
});

// ─── Tests that mock exec (decompress success / failure) ────────────────────

describe("POST /decompress with mocked exec", () => {
    let app;

    // Req 10.1 – valid .z file → file download
    describe("exec succeeds and creates output file", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                exec: jest.fn((cmd, cb) => {
                    // Parse the output path from the command and create it
                    const match = cmd.match(/"([^"]+\.txt)"[^"]*$/);
                    if (match) {
                        require("fs").writeFileSync(match[1], "decompressed content here");
                    }
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

    // Req 10.2 – exec fails → 500
    describe("exec fails", () => {
        beforeAll(() => {
            jest.resetModules();
            jest.mock("child_process", () => ({
                exec: jest.fn((cmd, cb) => {
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

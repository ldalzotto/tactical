'use strict';

const zlib = require('node:zlib');
const crypto = require('node:crypto');

// Layout of one __llvm_prf_data record on wasm32 for this toolchain
// (clang 17, swiftlang fork). See clang's include/profile/InstrProfData.inc:
//   NameRef          u64 @ 0
//   FuncHash         u64 @ 8
//   CounterPtr       i32 @ 16
//   BitmapPtr        i32 @ 20
//   FunctionPointer  i32 @ 24
//   Values           i32 @ 28
//   NumCounters      u32 @ 32
//   NumValueSites    u16[3] @ 36
//   NumBitmapBytes   u32 @ 42
//   (padded to 48 bytes)
const PRF_DATA_RECORD_SIZE = 48;
const PRF_DATA_FUNC_HASH_OFFSET = 8;
const PRF_DATA_NUM_COUNTERS_OFFSET = 32;

function readU32(buf, pos) {
    let result = 0;
    let shift = 0;
    let b;
    do {
        b = buf[pos++];
        result |= (b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    return [result >>> 0, pos];
}

function readS32(buf, pos) {
    let result = 0;
    let shift = 0;
    let b;
    do {
        b = buf[pos++];
        result |= (b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    if (shift < 32 && (b & 0x40)) {
        result |= -(1 << shift);
    }
    return [result | 0, pos];
}

function parseSections(buf) {
    const sections = [];
    let pos = 8; // skip magic + version
    while (pos < buf.length) {
        const id = buf[pos++];
        const [size, next] = readU32(buf, pos);
        pos = next;
        sections.push({ id, data: buf.slice(pos, pos + size) });
        pos += size;
    }
    return sections;
}

function parseDataSegments(buf) {
    const section = parseSections(buf).find((s) => s.id === 11); // Data
    if (!section) {
        return [];
    }

    const d = section.data;
    const segments = [];
    let pos = 0;
    const [count, afterCount] = readU32(d, 0);
    pos = afterCount;

    for (let i = 0; i < count; i++) {
        const [flags, afterFlags] = readU32(d, pos);
        pos = afterFlags;

        let offset = 0;
        if (flags === 0 || flags === 2) {
            if (flags === 2) {
                const [, afterMemIdx] = readU32(d, pos);
                pos = afterMemIdx;
            }
            // Offset expression emitted by clang: i32.const <sleb> end.
            if (d[pos] === 0x41) {
                const [value, afterConst] = readS32(d, pos + 1);
                offset = value;
                pos = afterConst;
            }
            while (d[pos] !== 0x0b) {
                pos++;
            }
            pos++; // skip 0x0b (end)
        }

        const [size, afterSize] = readU32(d, pos);
        pos = afterSize;
        segments.push({ offset, size, bytes: d.slice(pos, pos + size) });
        pos += size;
    }

    return segments;
}

function parseDataSegmentNames(buf) {
    const section = parseSections(buf).find(
        (s) => s.id === 0 && s.data[0] === 4 && s.data.toString('latin1', 1, 5) === 'name',
    );
    if (!section) {
        return {};
    }

    const d = section.data;
    const names = {};
    let pos = 5; // skip the 1-byte length + "name"
    while (pos < d.length) {
        const id = d[pos++];
        const [size, afterSize] = readU32(d, pos);
        pos = afterSize;
        const sub = d.slice(pos, pos + size);

        if (id === 9) {
            // Data segment names subsection.
            const [count, afterCount] = readU32(sub, 0);
            let q = afterCount;
            for (let i = 0; i < count; i++) {
                const [index, afterIndex] = readU32(sub, q);
                const [nameLen, afterNameLen] = readU32(sub, afterIndex);
                names[index] = sub.slice(afterNameLen, afterNameLen + nameLen).toString('latin1');
                q = afterNameLen + nameLen;
            }
        }

        pos += size;
    }

    return names;
}

// LLVM's profile name hash (InstrProf::ComputeHash) is the low 64 bits of the
// MD5 of the PGO function name, read little-endian.
function computeHash(name) {
    return crypto.createHash('md5').update(name).digest().readBigUInt64LE(0);
}

// __llvm_prf_names is a sequence of chunks. Each chunk is
//   ULEB128 uncompressed-length, ULEB128 compressed-length, zlib data.
// Decompressed chunks hold complete names separated by 0x01 (names never span
// a chunk boundary), so each chunk is split independently.
function decompressNames(namesSeg) {
    const names = [];
    let pos = 0;

    while (pos < namesSeg.size) {
        const [, afterUncompressedLen] = readU32(namesSeg.bytes, pos);
        const [compressedLen, afterCompressedLen] = readU32(namesSeg.bytes, afterUncompressedLen);
        const inflated = zlib.inflateSync(
            namesSeg.bytes.slice(afterCompressedLen, afterCompressedLen + compressedLen),
        );
        for (const name of inflated.toString('latin1').split('\x01')) {
            if (name !== '') {
                names.push(name);
            }
        }
        pos = afterCompressedLen + compressedLen;
    }

    return names;
}

// Reads the live coverage counters out of wasm memory after the tests have
// run and serializes them in llvm-profdata's text format, which llvm-profdata
// can merge without us having to write the raw binary profile format.
function buildTextProfile({ wasmBytes, memory }) {
    const segments = parseDataSegments(wasmBytes);
    const segmentNames = parseDataSegmentNames(wasmBytes);

    const byName = new Map();
    segments.forEach((seg, i) => {
        if (segmentNames[i]) {
            byName.set(segmentNames[i], seg);
        }
    });

    const namesSeg = byName.get('__llvm_prf_names');
    const dataSeg = byName.get('__llvm_prf_data');
    const countersSeg = byName.get('__llvm_prf_cnts');

    if (!namesSeg || !dataSeg || !countersSeg) {
        throw new Error(
            'app.wasm has no coverage instrumentation; build with --coverage first',
        );
    }

    const nameByHash = new Map();
    for (const name of decompressNames(namesSeg)) {
        nameByHash.set(computeHash(name), name);
    }

    const numFuncs = dataSeg.size / PRF_DATA_RECORD_SIZE;
    if (dataSeg.size % PRF_DATA_RECORD_SIZE !== 0) {
        throw new Error(`unexpected __llvm_prf_data size: ${dataSeg.size}`);
    }

    const view = new DataView(memory.buffer);
    let counterIndex = 0;
    const parts = [];

    for (let i = 0; i < numFuncs; i++) {
        const base = dataSeg.offset + i * PRF_DATA_RECORD_SIZE;
        const nameRef = view.getBigUint64(base, true);
        const funcHash = view.getBigUint64(base + PRF_DATA_FUNC_HASH_OFFSET, true);
        const numCounters = view.getUint32(base + PRF_DATA_NUM_COUNTERS_OFFSET, true);

        // Counters are laid out sequentially per record; always consume the
        // record's counters so later records stay aligned.
        const counters = [];
        for (let c = 0; c < numCounters; c++) {
            counters.push(view.getBigUint64(countersSeg.offset + counterIndex * 8, true));
            counterIndex++;
        }

        const name = nameByHash.get(nameRef);
        if (name === undefined) {
            console.warn(`no name for __llvm_prf_data record ${i} (hash 0x${nameRef.toString(16)})`);
            continue;
        }

        parts.push(
            `${name}\n` +
            `# Func Hash:\n${funcHash}\n` +
            `# Num Counters:\n${numCounters}\n` +
            `# Counter Values:\n${counters.join('\n')}\n`,
        );
    }

    return parts.join('\n');
}

module.exports = { buildTextProfile };

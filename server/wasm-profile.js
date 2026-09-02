'use strict';

const zlib = require('node:zlib');
const crypto = require('node:crypto');

// Layout of one __llvm_prf_data record on wasm32 (see clang's
// include/profile/InstrProfData.inc). NameRef/FuncHash are stable, but later
// fields shift between clang releases (e.g. IPVK_VTableTarget grew the
// record from 48 to 56 bytes by adding a field before NumCounters):
//   NameRef u64 @ 0, FuncHash u64 @ 8, ...toolchain-dependent...,
//   NumCounters u32 @ (32 old clang / 36 new), padded to record size.
// No reliable version cutoff exists, so detectRecordLayout() below tries
// each known layout and picks the one whose summed NumCounters matches the
// counters segment size.
const PRF_DATA_FUNC_HASH_OFFSET = 8;
const KNOWN_RECORD_LAYOUTS = [
    { recordSize: 48, numCountersOffset: 32 }, // clang <= 17
    { recordSize: 56, numCountersOffset: 36 }, // clang with IPVK_VTableTarget (observed on clang 23 trunk)
];

// Detects which KNOWN_RECORD_LAYOUTS entry produced this binary: the correct
// one is whichever candidate's summed per-record NumCounters matches the
// counters segment size.
function detectRecordLayout(dataSeg, countersSeg) {
    const expectedCounterCount = countersSeg.size / 8;
    const bytes = dataSeg.bytes;
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.length);

    for (const layout of KNOWN_RECORD_LAYOUTS) {
        if (dataSeg.size === 0 || dataSeg.size % layout.recordSize !== 0) {
            continue;
        }
        const numFuncs = dataSeg.size / layout.recordSize;
        let total = 0;
        for (let i = 0; i < numFuncs; i++) {
            total += view.getUint32(i * layout.recordSize + layout.numCountersOffset, true);
        }
        if (total === expectedCounterCount) {
            return layout;
        }
    }

    throw new Error(
        `unable to detect __llvm_prf_data record layout for size ${dataSeg.size} ` +
        `(unrecognized clang toolchain; add its layout to KNOWN_RECORD_LAYOUTS in wasm-profile.js)`,
    );
}

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

// InstrProf::ComputeHash: low 64 bits of MD5(name), little-endian.
function computeHash(name) {
    return crypto.createHash('md5').update(name).digest().readBigUInt64LE(0);
}

// __llvm_prf_names: chunks of (ULEB128 uncompressed-len, ULEB128
// compressed-len, zlib data). Decompressed names are 0x01-separated and
// never span a chunk, so each chunk splits independently.
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

// Reads live coverage counters from wasm memory and serializes them in
// llvm-profdata's text format (avoids writing the raw binary profile format).
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

    const { recordSize, numCountersOffset } = detectRecordLayout(dataSeg, countersSeg);
    const numFuncs = dataSeg.size / recordSize;

    const view = new DataView(memory.buffer);
    let counterIndex = 0;
    const parts = [];

    for (let i = 0; i < numFuncs; i++) {
        const base = dataSeg.offset + i * recordSize;
        const nameRef = view.getBigUint64(base, true);
        const funcHash = view.getBigUint64(base + PRF_DATA_FUNC_HASH_OFFSET, true);
        const numCounters = view.getUint32(base + numCountersOffset, true);

        // Always consume this record's counters to keep later records aligned.
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

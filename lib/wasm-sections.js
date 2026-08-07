const WASM_MAGIC = 0x6d736100; // '\0asm'
const CODE_SECTION_ID = 10;

function readVarUint32(buffer, pos) {
    let result = 0;
    let shift = 0;
    let byte;
    do {
        byte = buffer[pos++];
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return { value: result >>> 0, pos };
}

// DWARF addresses in a wasm module's debug info are relative to the first
// byte of the first function body in the Code section (i.e. right after the
// section's function-count field), not to the start of the file. To turn a
// raw `wasm-function[N]:0xOFFSET` file offset into a DWARF-relative address,
// callers must subtract this base.
function codeSectionBodyOffset(buffer) {
    if (buffer.readUInt32LE(0) !== WASM_MAGIC) {
        throw new Error('not a wasm module');
    }

    let pos = 8; // past magic + version
    while (pos < buffer.length) {
        const id = buffer[pos];
        const sizeField = readVarUint32(buffer, pos + 1);
        const contentStart = sizeField.pos;
        const contentEnd = contentStart + sizeField.value;

        if (id === CODE_SECTION_ID) {
            const count = readVarUint32(buffer, contentStart);
            return count.pos;
        }

        pos = contentEnd;
    }

    throw new Error('wasm module has no Code section');
}

module.exports = { codeSectionBodyOffset };

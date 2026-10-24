#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <array>
#include <cstdio>
#include <cctype>
#include <utility>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <functional>
#include "../../../Memory/Communication.h"
#include "../../../Game/Offsets/Offsets.h"
#include "miniz.h"

namespace Decompiler {

    // Helper: decode LEB128 VarInt
    inline uint32_t ReadVarInt(const uint8_t*& data, const uint8_t* end) {
        uint32_t result = 0;
        uint32_t shift = 0;
        while (data < end) {
            uint8_t b = *data++;
            result |= (b & 127) << shift;
            if ((b & 128) == 0) break;
            shift += 7;
        }
        return result;
    }

    // Luau production opcode order (v3-v12). Roblox encodes the low opcode
    // byte with *227; DecodeRobloxOpcode() applies the modular inverse 203.
    enum LuauOpcode : uint8_t {
        LOP_NOP = 0, LOP_BREAK = 1, LOP_LOADNIL = 2, LOP_LOADB = 3, LOP_LOADN = 4, LOP_LOADK = 5,
        LOP_MOVE = 6, LOP_GETGLOBAL = 7, LOP_SETGLOBAL = 8, LOP_GETUPVAL = 9, LOP_SETUPVAL = 10,
        LOP_CLOSEUPVALS = 11, LOP_GETIMPORT = 12, LOP_GETTABLE = 13, LOP_SETTABLE = 14,
        LOP_GETTABLEKS = 15, LOP_SETTABLEKS = 16, LOP_GETTABLEN = 17, LOP_SETTABLEN = 18,
        LOP_NEWCLOSURE = 19, LOP_NAMECALL = 20, LOP_CALL = 21, LOP_RETURN = 22, LOP_JUMP = 23,
        LOP_JUMPBACK = 24, LOP_JUMPIF = 25, LOP_JUMPIFNOT = 26, LOP_JUMPIFEQ = 27, LOP_JUMPIFLE = 28,
        LOP_JUMPIFLT = 29, LOP_JUMPIFNOTEQ = 30, LOP_JUMPIFNOTLE = 31, LOP_JUMPIFNOTLT = 32,
        LOP_ADD = 33, LOP_SUB = 34, LOP_MUL = 35, LOP_DIV = 36, LOP_MOD = 37, LOP_POW = 38,
        LOP_ADDK = 39, LOP_SUBK = 40, LOP_MULK = 41, LOP_DIVK = 42, LOP_MODK = 43, LOP_POWK = 44,
        LOP_AND = 45, LOP_OR = 46, LOP_ANDK = 47, LOP_ORK = 48, LOP_CONCAT = 49, LOP_NOT = 50,
        LOP_MINUS = 51, LOP_LENGTH = 52, LOP_NEWTABLE = 53, LOP_DUPTABLE = 54, LOP_SETLIST = 55,
        LOP_FORNPREP = 56, LOP_FORNLOOP = 57, LOP_FORGLOOP = 58, LOP_FORGPREP_INEXT = 59,
        LOP_FASTCALL3 = 60, LOP_FORGPREP_NEXT = 61, LOP_NATIVECALL = 62, LOP_GETVARARGS = 63,
        LOP_DUPCLOSURE = 64, LOP_PREPVARARGS = 65, LOP_LOADKX = 66, LOP_JUMPX = 67, LOP_FASTCALL = 68,
        LOP_COVERAGE = 69, LOP_CAPTURE = 70, LOP_SUBRK = 71, LOP_DIVRK = 72, LOP_FASTCALL1 = 73,
        LOP_FASTCALL2 = 74, LOP_FASTCALL2K = 75, LOP_FORGPREP = 76, LOP_JUMPXEQKNIL = 77,
        LOP_JUMPXEQKB = 78, LOP_JUMPXEQKN = 79, LOP_JUMPXEQKS = 80, LOP_IDIV = 81, LOP_IDIVK = 82,
        LOP_GETUDATAKS = 83, LOP_SETUDATAKS = 84, LOP_NAMECALLUDATA = 85,
        LOP_NEWCLASSMEMBER = 86, LOP_CALLFB = 87, LOP_CMPPROTO = 88
    };

    inline uint8_t DecodeRobloxOpcode(uint8_t encoded)
    {
        return static_cast<uint8_t>(static_cast<unsigned>(encoded) * 203u);
    }
    struct Constant {
        uint8_t type = 0;
        bool boolVal = false;
        double numVal = 0.0;
        int64_t intVal = 0;
        std::array<float, 4> vectorVal{ 0.f, 0.f, 0.f, 0.f };
        std::string strVal;
        uint32_t importId = 0;
        uint32_t protoIndex = 0;
        // Luau table-shape constants (LBC_CONSTANT_TABLE / _WITH_CONSTANTS).
        // Keys and values are indices into this prototype's constant table.
        std::vector<uint32_t> tableKeys;
        std::vector<int32_t> tableValues; // -1 => no prefilled value
    };

    struct DebugLocal {
        std::string name;
        uint32_t startPc = 0;
        uint32_t endPc = 0;
        uint8_t reg = 0;
    };

    struct Prototype {
        uint8_t maxStackSize;
        uint8_t numParameters;
        uint8_t numUpvalues;
        uint8_t isVararg;
        std::vector<uint32_t> code;
        std::vector<Constant> constants;
        std::vector<uint32_t> childProtos;
        int debugLine = 0;
        std::string debugName;
        std::vector<DebugLocal> debugLocals;
        std::vector<std::string> debugUpvalues;
    };

    struct BytecodeResult {
        bool ok = false;
        std::string bytes;
        std::string error;
        std::string representation;
    };

    inline BytecodeResult BytecodeFailure(const std::string& message) {
        BytecodeResult r;
        r.error = message;
        return r;
    }

    inline BytecodeResult BytecodeSuccess(std::string bytes, const std::string& representation) {
        BytecodeResult r;
        r.ok = true;
        r.bytes = std::move(bytes);
        r.representation = representation;
        return r;
    }

    // -------------------------------------------------------------------------
    //  Bytecode reader
    //
    //  The offset dumper (theo's offsets / RbxDumperV2) provides:
    //
    //    Offsets::LocalScript::ByteCode  -- offset from script instance base to
    //                                       the start of a bytecode HOLDER STRUCT
    //    Offsets::ModuleScript::ByteCode -- same, for ModuleScript instances
    //    Offsets::Script::ByteCode       -- same, for Script instances
    //
    //    Offsets::ByteCode::Pointer      -- offset WITHIN the holder struct to a
    //                                       uintptr_t pointing at the raw bytecode
    //    Offsets::ByteCode::Size         -- offset WITHIN the holder struct to a
    //                                       size_t holding the bytecode length
    //
    //  Candidate read chain currently being diagnosed:
    //    holderField = scriptAddr + LocalScript::ByteCode
    //    holderAddr  = Read<uintptr_t>(holderField)
    //    dataPtr     = Read<uintptr_t>(holderAddr + ByteCode::Pointer)
    //    dataSize    = Read<size_t>   (holderAddr + ByteCode::Size)
    //
    //  IMPORTANT: readable addresses do not prove these fields have these meanings.
    //  The raw holder dump below is intentionally included so bad layout assumptions
    //  can be identified without guessing new offsets or scanning memory.
    // -------------------------------------------------------------------------

    // ---- Diagnostic helpers ------------------------------------------------

    // ReadMemoryChecked: wraps ReadProcessMemory with full error reporting.
    // Returns true on success. On failure, 'outValue' is zero-initialized.
    template <typename T>
    inline bool ReadMemoryChecked(uintptr_t address, T& outValue,
        const char* label = nullptr)
    {
        outValue = T{};
        if (!Coms->IsConnected()) {
            std::cerr << "[Bytecode] ReadMemoryChecked("
                << (label ? label : "?") << "): not connected\n";
            return false;
        }
        if (address == 0) {
            std::cerr << "[Bytecode] ReadMemoryChecked("
                << (label ? label : "?") << "): null address\n";
            return false;
        }

        SIZE_T bytesRead = 0;
        SetLastError(0);
        BOOL ok = ReadProcessMemory(
            Coms->GetHandle(),
            reinterpret_cast<LPCVOID>(address),
            &outValue,
            sizeof(T),
            &bytesRead
        );
        DWORD err = GetLastError();

        if (!ok || bytesRead != sizeof(T)) {
            std::cerr << "[Bytecode] ReadMemoryChecked("
                << (label ? label : "?")
                << ") FAILED at 0x" << std::hex << address << std::dec
                << " | ok=" << ok
                << " bytesRead=" << bytesRead
                << "/" << sizeof(T)
                << " GLE=" << err << "\n";
            return false;
        }
        return true;
    }

    // Silent variant used by the offset scanner — no stderr output on failure.
    template <typename T>
    inline bool ReadMemorySilent(uintptr_t address, T& outValue)
    {
        outValue = T{};
        if (address == 0 || !Coms->IsConnected()) return false;
        SIZE_T bytesRead = 0;
        BOOL ok = ReadProcessMemory(
            Coms->GetHandle(),
            reinterpret_cast<LPCVOID>(address),
            &outValue,
            sizeof(T),
            &bytesRead
        );
        return ok && bytesRead == sizeof(T);
    }

    // IsReadableMemory: checks whether 'address' points to committed, readable
    // memory in the target process using VirtualQueryEx.
    inline bool IsReadableMemory(uintptr_t address, size_t size = 1) {
        if (address == 0 || !Coms->IsConnected()) return false;

        MEMORY_BASIC_INFORMATION mbi{};
        SIZE_T result = VirtualQueryEx(
            Coms->GetHandle(),
            reinterpret_cast<LPCVOID>(address),
            &mbi,
            sizeof(mbi)
        );
        if (result == 0) return false;

        // Must be committed
        if (mbi.State != MEM_COMMIT) return false;

        // Must have read permission
        DWORD protect = mbi.Protect & 0xFF;
        bool readable = (protect == PAGE_READONLY ||
            protect == PAGE_READWRITE ||
            protect == PAGE_EXECUTE_READ ||
            protect == PAGE_EXECUTE_READWRITE ||
            protect == PAGE_WRITECOPY ||
            protect == PAGE_EXECUTE_WRITECOPY);
        if (!readable) return false;

        // Check the requested range fits within this region
        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (address + size > regionEnd) return false;

        return true;
    }

    // Returns true when ReadScriptBytecode returned diagnostic text instead
    // of a binary bytecode buffer. The old code fed this text into the
    // decompiler, so the leading '-' (ASCII 45) was misread as a bytecode
    // version.
    inline bool IsReaderError(const std::string& value) {
        return value.rfind("-- [Error]", 0) == 0;
    }

    // Dump the exact bytes crossing the reader -> decompiler boundary.
    inline void DumpBytePrefix(const char* tag, const std::string& bytes,
        size_t maxBytes = 16)
    {
        std::ostringstream hex;
        const size_t count = (std::min)(bytes.size(), maxBytes);

        hex << std::hex << std::uppercase << std::setfill('0');
        for (size_t i = 0; i < count; ++i) {
            if (i != 0) hex << ' ';
            hex << std::setw(2)
                << static_cast<unsigned int>(
                    static_cast<unsigned char>(bytes[i]));
        }

        std::cerr << tag
            << " size=" << bytes.size();

        if (!bytes.empty()) {
            std::cerr << " firstByte="
                << static_cast<unsigned int>(
                    static_cast<unsigned char>(bytes[0]));
        }

        std::cerr << " hex=[" << hex.str() << "]\n";
    }

    inline std::string HexPrefix(const std::string& bytes, size_t maxBytes = 16)
    {
        std::ostringstream hex;
        const size_t count = (std::min)(bytes.size(), maxBytes);
        hex << std::hex << std::uppercase << std::setfill('0');
        for (size_t i = 0; i < count; ++i) {
            if (i != 0) hex << ' ';
            hex << std::setw(2)
                << static_cast<unsigned int>(
                    static_cast<unsigned char>(bytes[i]));
        }
        return hex.str();
    }

    // Dumps qword-sized fields from a holder that has already been resolved.
    // This is diagnostic only: values are printed, not followed or scanned.
    inline void DumpHolderFields(uintptr_t holderAddr, uintptr_t lastOffset = 0x40)
    {
        std::cerr << "[Bytecode] holder raw fields:" << "\n";

        for (uintptr_t off = 0; off <= lastOffset; off += sizeof(uint64_t)) {
            uint64_t value = 0;
            const uintptr_t fieldAddr = holderAddr + off;

            if (ReadMemoryChecked(fieldAddr, value, "holderField")) {
                std::cerr
                    << "  +0x" << std::hex << off
                    << " @ 0x" << fieldAddr
                    << " = 0x" << value
                    << std::dec << "\n";
            }
            else {
                std::cerr
                    << "  +0x" << std::hex << off
                    << " @ 0x" << fieldAddr
                    << " = <unreadable>"
                    << std::dec << "\n";
            }
        }
    }

    // ---- Bytecode reading --------------------------------------------------

    // Primary path: 2-level dereference through the holder struct.
    // Returns the bytecode bytes on success, or a diagnostic error string
    // prefixed with "-- [Error]" on failure (never empty — the caller can
    // distinguish data from error by checking the prefix).
    inline bool LooksLikeSupportedRawLuau(const std::string& bytes)
    {
        if (bytes.empty()) return false;
        const uint8_t version = static_cast<uint8_t>(static_cast<unsigned char>(bytes[0]));
        if (version == 3) return bytes.size() >= 2;
        if (version < 3 || version > 12 || bytes.size() < 2) return false;
        const uint8_t typeVersion = static_cast<uint8_t>(static_cast<unsigned char>(bytes[1]));
        return typeVersion >= 1 && typeVersion <= 3;
    }

    // A version/type-byte check alone is too weak for scanning arbitrary process
    // memory: random holder data can begin with e.g. 0x0C 0x03 and be mistaken
    // for Luau v12. Validate the serialized prefix far enough to prove that the
    // string table and the following prototype-count field are in-bounds.
    inline bool LooksStructurallyValidRawLuau(const std::string& bytes)
    {
        if (!LooksLikeSupportedRawLuau(bytes))
            return false;

        const uint8_t* p = reinterpret_cast<const uint8_t*>(bytes.data());
        const uint8_t* end = p + bytes.size();

        const uint8_t version = *p++;
        uint8_t typeVersion = 0;
        if (version >= 4) {
            if (p >= end) return false;
            typeVersion = *p++;
        }

        auto readVar = [&](uint32_t& out) -> bool {
            out = 0;
            unsigned shift = 0;
            for (unsigned i = 0; i < 5; ++i) {
                if (p >= end) return false;
                const uint8_t b = *p++;
                out |= uint32_t(b & 0x7fu) << shift;
                if ((b & 0x80u) == 0)
                    return true;
                shift += 7;
            }
            return false;
            };

        uint32_t stringCount = 0;
        if (!readVar(stringCount))
            return false;

        // Serialized scripts can contain many strings, but an absurd count from
        // a small candidate is a strong indication that this is a false hit.
        if (stringCount > 1'000'000u)
            return false;

        for (uint32_t i = 0; i < stringCount; ++i) {
            uint32_t len = 0;
            if (!readVar(len))
                return false;
            if (static_cast<size_t>(end - p) < static_cast<size_t>(len))
                return false;
            p += len;
        }

        // Type encoding v3 appends a userdata-type remapping table after strings,
        // terminated by a zero tag.
        if (version >= 4 && typeVersion >= 3) {
            bool terminated = false;
            while (p < end) {
                const uint8_t tag = *p++;
                if (tag == 0) {
                    terminated = true;
                    break;
                }
                uint32_t ignoredStringRef = 0;
                if (!readVar(ignoredStringRef))
                    return false;
            }
            if (!terminated)
                return false;
        }

        uint32_t protoCount = 0;
        if (!readVar(protoCount))
            return false;

        // Valid compiled scripts should have at least one prototype; also reject
        // impossible/random counts before the expensive full parser runs.
        if (protoCount == 0 || protoCount > 1'000'000u)
            return false;

        // For v12 each serialized prototype begins with a byte-size varint.
        // Validate the first one so a false candidate cannot pass solely because
        // its string table happened to fit.
        if (version >= 12) {
            uint32_t firstProtoSize = 0;
            if (!readVar(firstProtoSize))
                return false;
            if (firstProtoSize == 0 ||
                static_cast<size_t>(end - p) < static_cast<size_t>(firstProtoSize))
                return false;
        }

        return true;
    }

    inline bool TakeInflatedBuffer(void* ptr, size_t size, std::string& out)
    {
        constexpr size_t kMaxDecodedBytes = 16 * 1024 * 1024;
        if (!ptr) return false;

        if (size == 0 || size > kMaxDecodedBytes) {
            free(ptr);
            return false;
        }

        out.assign(reinterpret_cast<const char*>(ptr), size);
        free(ptr);
        return true;
    }

    // Runtime Zstd loader so the project does not need a zstd header or .lib.
    // Put a 64-bit zstd.dll (or libzstd.dll/libzstd-1.dll) next to Shrimplock.exe.
    struct ZstdApi {
        using DecompressFn = size_t(__cdecl*)(void*, size_t, const void*, size_t);
        using IsErrorFn = unsigned(__cdecl*)(size_t);
        using ErrorNameFn = const char* (__cdecl*)(size_t);

        HMODULE module = nullptr;
        DecompressFn decompress = nullptr;
        IsErrorFn isError = nullptr;
        ErrorNameFn errorName = nullptr;

        bool Load() {
            if (module && decompress && isError) return true;
            const char* names[] = { "zstd.dll", "libzstd.dll", "libzstd-1.dll" };
            for (const char* name : names) {
                module = GetModuleHandleA(name);
                if (!module) module = LoadLibraryA(name);
                if (module) break;
            }
            if (!module) return false;
            decompress = reinterpret_cast<DecompressFn>(GetProcAddress(module, "ZSTD_decompress"));
            isError = reinterpret_cast<IsErrorFn>(GetProcAddress(module, "ZSTD_isError"));
            errorName = reinterpret_cast<ErrorNameFn>(GetProcAddress(module, "ZSTD_getErrorName"));
            return decompress && isError;
        }
    };

    inline ZstdApi& GetZstdApi() { static ZstdApi api; return api; }

    // Roblox RSB1 payloads are an obfuscated RSB1 header + 32-bit raw-size + Zstd frame.
    // The 4-byte obfuscation mask is recoverable from the known plaintext magic "RSB1".
    inline BytecodeResult TryDecodeRSB1(const std::string& candidate)
    {
        if (candidate.size() < 12)
            return BytecodeFailure("not RSB1");

        static constexpr uint8_t magic[4] = { 'R','S','B','1' };
        std::array<uint8_t, 4> key{};
        for (size_t i = 0; i < 4; ++i) {
            const uint8_t encoded = static_cast<uint8_t>(static_cast<unsigned char>(candidate[i]));
            const uint8_t maskAtI = encoded ^ magic[i];
            key[i] = static_cast<uint8_t>(maskAtI - static_cast<uint8_t>(i * 41u));
        }

        std::string decoded(candidate.size(), '\0');
        for (size_t i = 0; i < candidate.size(); ++i) {
            const uint8_t encoded = static_cast<uint8_t>(static_cast<unsigned char>(candidate[i]));
            const uint8_t mask = static_cast<uint8_t>(key[i & 3] + static_cast<uint8_t>(i * 41u));
            decoded[i] = static_cast<char>(encoded ^ mask);
        }

        if (decoded.compare(0, 4, "RSB1") != 0)
            return BytecodeFailure("not RSB1");

        uint32_t rawSize = 0;
        std::memcpy(&rawSize, decoded.data() + 4, sizeof(rawSize));
        if (rawSize == 0 || rawSize > 16u * 1024u * 1024u)
            return BytecodeFailure("RSB1 has invalid raw size " + std::to_string(rawSize));

        const uint8_t* d = reinterpret_cast<const uint8_t*>(decoded.data());
        if (!(d[8] == 0x28 && d[9] == 0xB5 && d[10] == 0x2F && d[11] == 0xFD))
            return BytecodeFailure("RSB1 decoded but Zstd magic was not found at +8; header=[" + HexPrefix(decoded, 16) + "]");

        std::cerr << "[Payload] RSB1 decoded header=[" << HexPrefix(decoded, 16)
            << "] rawSize=" << rawSize << "\n";

        ZstdApi& z = GetZstdApi();
        if (!z.Load())
            return BytecodeFailure("RSB1 detected. Missing 64-bit zstd.dll beside Shrimplock.exe.");

        std::string raw(rawSize, '\0');
        size_t got = z.decompress(raw.data(), raw.size(), decoded.data() + 8, decoded.size() - 8);
        if (z.isError(got)) {
            const char* n = z.errorName ? z.errorName(got) : nullptr;
            return BytecodeFailure(std::string("RSB1 Zstd decompression failed") + (n ? std::string(": ") + n : std::string()));
        }
        raw.resize(got);
        DumpBytePrefix("[Payload] RSB1 -> Zstd", raw, 32);

        if (!LooksStructurallyValidRawLuau(raw))
            return BytecodeFailure("RSB1 decompressed, but output failed Luau structural validation. Header=[" + HexPrefix(raw, 16) + "]");

        return BytecodeSuccess(std::move(raw), "RSB1 -> Zstd -> raw Luau");
    }

    inline BytecodeResult ResolveStandardPayload(const std::string& candidate)
    {
        if (candidate.empty())
            return BytecodeFailure("Candidate script payload is empty.");

        DumpBytePrefix("[Payload] INPUT", candidate, 32);

        if (LooksStructurallyValidRawLuau(candidate)) {
            std::cerr << "[Payload] structurally validated raw Luau bytecode.\n";
            return BytecodeSuccess(candidate, "raw Luau");
        }
        else if (LooksLikeSupportedRawLuau(candidate)) {
            std::cerr << "[Payload] raw-looking header rejected by structural validation; continuing format checks.\n";
        }

        {
            BytecodeResult rsb = TryDecodeRSB1(candidate);
            if (rsb.ok) return rsb;
            if (rsb.error.rfind("RSB1", 0) == 0 && rsb.error != "not RSB1")
                return rsb;
        }

        const void* src = candidate.data();
        const size_t srcSize = candidate.size();

        // Standard RFC 1950 zlib wrapper.
        {
            size_t outSize = 0;
            void* inflated = mz_inflate_zlib(src, srcSize, &outSize);
            std::string decoded;
            if (TakeInflatedBuffer(inflated, outSize, decoded)) {
                DumpBytePrefix("[Payload] zlib decoded", decoded, 32);
                if (LooksStructurallyValidRawLuau(decoded)) {
                    std::cerr << "[Payload] accepted RFC1950/zlib -> structurally validated raw Luau.\n";
                    return BytecodeSuccess(std::move(decoded), "zlib -> raw Luau");
                }
                std::cerr << "[Payload] zlib decompressed, but result is not a supported raw Luau header.\n";
            }
        }

        // Standard RFC 1951 raw DEFLATE. No guessed prefix stripping is done.
        {
            size_t outSize = 0;
            void* inflated = mz_inflate_raw(src, srcSize, &outSize);
            std::string decoded;
            if (TakeInflatedBuffer(inflated, outSize, decoded)) {
                DumpBytePrefix("[Payload] raw-deflate decoded", decoded, 32);
                if (LooksStructurallyValidRawLuau(decoded)) {
                    std::cerr << "[Payload] accepted RFC1951/raw-DEFLATE -> structurally validated raw Luau.\n";
                    return BytecodeSuccess(std::move(decoded), "raw DEFLATE -> raw Luau");
                }
                std::cerr << "[Payload] raw DEFLATE decompressed, but result is not a supported raw Luau header.\n";
            }
        }

        return BytecodeFailure(
            "Unsupported payload representation. Candidate size=" +
            std::to_string(candidate.size()) +
            ", header=[" + HexPrefix(candidate, 16) + "]. "
            "Tried raw Luau, RSB1/Zstd, RFC1950 zlib, and RFC1951 raw DEFLATE."
        );
    }

    // =========================================================================
    //  Offset scanner / guesser
    //
    //  When the configured class offset is zero (or yields garbage), this
    //  scanner probes every 8-byte-aligned slot in the range
    //  [kScanStart, kScanEnd] within the script instance.  For each candidate
    //  classOffset it:
    //
    //    1. Reads holderAddr = scriptAddr + classOffset           (direct field)
    //         OR treats holderAddr = scriptAddr + classOffset     (inline holder)
    //    2. For both interpretations, tries every combination of
    //       (ptrOff, sizeOff) in a compact set of likely inner offsets.
    //    3. Validates the recovered buffer with LooksLikeSupportedRawLuau()
    //       and a quick ResolveStandardPayload() check.
    //    4. On success, caches the winning (classOffset, ptrOff, sizeOff)
    //       triple per className so subsequent calls skip the scan.
    // =========================================================================

    struct OffsetTriple {
        uintptr_t classOffset = 0;   // scriptAddr + classOffset -> holder field
        uintptr_t ptrOff = 0;        // holder + ptrOff -> data pointer / begin pointer
        uintptr_t sizeOff = 0;       // holder + sizeOff -> byte size OR end pointer
        uint8_t   derefCount = 0;    // 0=inline, 1=*field, 2=**field
        bool      sizeIsEnd = false; // true => sizeOff stores end pointer, size=end-begin
    };

    // A scanner hit now carries the exact bytes that passed payload validation.
    // This avoids the old bug where the scanner validated one buffer, discarded it,
    // then immediately performed a second read that could fail for a volatile holder.
    struct ScanHit {
        OffsetTriple offsets{};
        BytecodeResult resolved{};
        std::string rawCandidate;

        bool ok() const {
            return offsets.classOffset != 0 && resolved.ok && !rawCandidate.empty();
        }
    };

    // Per-className cache of discovered offsets.  Protected by a mutex because
    // multiple threads might trigger the scan simultaneously.
    inline std::unordered_map<std::string, OffsetTriple>& OffsetCache()
    {
        static std::unordered_map<std::string, OffsetTriple> cache;
        return cache;
    }
    inline std::mutex& OffsetCacheMutex()
    {
        static std::mutex mtx;
        return mtx;
    }

    // Attempt to read a raw bytecode buffer given a fully resolved holder
    // address, a candidate ptr-offset, and a candidate size-offset.
    // Returns the raw bytes on success, or an empty string on failure.
    // This is intentionally silent (no stderr) to keep scan logs readable.
    inline std::string TryReadFromHolder(uintptr_t holderAddr,
        uintptr_t ptrOff,
        uintptr_t sizeOff)
    {
        constexpr size_t kMinSize = 4;
        constexpr size_t kMaxSize = 5 * 1024 * 1024;
        constexpr size_t kProbeBytes = 4096;

        uintptr_t dataPtr = 0;
        size_t    dataSize = 0;

        if (!ReadMemorySilent(holderAddr + ptrOff, dataPtr))  return {};
        if (!ReadMemorySilent(holderAddr + sizeOff, dataSize)) return {};

        if (dataPtr == 0)                          return {};
        if (dataSize < kMinSize || dataSize > kMaxSize) return {};
        if (!IsReadableMemory(dataPtr, (std::min)(dataSize, kProbeBytes))) return {};

        // Do not reject by the first byte here. Roblox RSB1 payloads can have
        // an obfuscated header, so the in-memory first byte does not need to be
        // a raw Luau version or literal 'R'. ResolveStandardPayload() performs
        // the real format validation after this candidate is returned.

        std::string buf(dataSize, '\0');
        SIZE_T bytesRead = 0;
        BOOL ok = ReadProcessMemory(
            Coms->GetHandle(),
            reinterpret_cast<LPCVOID>(dataPtr),
            buf.data(), dataSize, &bytesRead
        );
        if (!ok || bytesRead == 0) return {};
        if (bytesRead < dataSize)  buf.resize(bytesRead);

        return buf;
    }

    // Some holders use a vector/span representation:
    // [begin pointer][end pointer][capacity pointer].
    inline std::string TryReadFromBeginEnd(uintptr_t holderAddr,
        uintptr_t beginOff,
        uintptr_t endOff)
    {
        constexpr size_t kMinSize = 4;
        constexpr size_t kMaxSize = 5 * 1024 * 1024;
        constexpr size_t kProbeBytes = 4096;

        uintptr_t beginPtr = 0;
        uintptr_t endPtr = 0;

        if (!ReadMemorySilent(holderAddr + beginOff, beginPtr)) return {};
        if (!ReadMemorySilent(holderAddr + endOff, endPtr)) return {};
        if (beginPtr == 0 || endPtr <= beginPtr) return {};

        const uintptr_t delta = endPtr - beginPtr;
        if (delta < kMinSize || delta > kMaxSize) return {};

        const size_t dataSize = static_cast<size_t>(delta);
        if (!IsReadableMemory(beginPtr, (std::min)(dataSize, kProbeBytes)))
            return {};

        std::string buf(dataSize, '\0');
        SIZE_T bytesRead = 0;
        const BOOL ok = ReadProcessMemory(
            Coms->GetHandle(),
            reinterpret_cast<LPCVOID>(beginPtr),
            buf.data(), dataSize, &bytesRead
        );

        if (!ok || bytesRead == 0) return {};
        if (bytesRead < buf.size()) buf.resize(bytesRead);
        return buf;
    }

    // Verbose variant used only when replaying a cached offset triple.
    // The scanner itself stays quiet so brute-force output remains manageable.
    inline std::string TryReadFromHolderVerbose(uintptr_t holderAddr,
        uintptr_t ptrOff,
        uintptr_t sizeOff)
    {
        constexpr size_t kMinSize = 4;
        constexpr size_t kMaxSize = 5 * 1024 * 1024;
        constexpr size_t kProbeBytes = 4096;

        uintptr_t dataPtr = 0;
        size_t dataSize = 0;

        std::cerr << "[ReadTriple] holder=0x" << std::hex << holderAddr
            << " ptrOff=0x" << ptrOff
            << " sizeOff=0x" << sizeOff << std::dec << "\n";

        if (!ReadMemorySilent(holderAddr + ptrOff, dataPtr)) {
            std::cerr << "[ReadTriple] failed to read data pointer field\n";
            return {};
        }
        if (!ReadMemorySilent(holderAddr + sizeOff, dataSize)) {
            std::cerr << "[ReadTriple] failed to read data size field\n";
            return {};
        }

        std::cerr << "[ReadTriple] dataPtr=0x" << std::hex << dataPtr
            << std::dec << " size=" << dataSize << "\n";

        if (dataPtr == 0) {
            std::cerr << "[ReadTriple] rejected: dataPtr is null\n";
            return {};
        }
        if (dataSize < kMinSize || dataSize > kMaxSize) {
            std::cerr << "[ReadTriple] rejected: size outside allowed range\n";
            return {};
        }
        if (!IsReadableMemory(dataPtr, (std::min)(dataSize, kProbeBytes))) {
            std::cerr << "[ReadTriple] rejected: data buffer is not readable\n";
            return {};
        }

        std::string buf(dataSize, '\0');
        SIZE_T bytesRead = 0;
        SetLastError(0);
        const BOOL ok = ReadProcessMemory(
            Coms->GetHandle(),
            reinterpret_cast<LPCVOID>(dataPtr),
            buf.data(),
            dataSize,
            &bytesRead
        );
        const DWORD gle = GetLastError();

        std::cerr << "[ReadTriple] RPM ok=" << ok
            << " bytesRead=" << bytesRead << "/" << dataSize
            << " GLE=" << gle << "\n";

        if (!ok || bytesRead == 0) return {};
        if (bytesRead < buf.size()) buf.resize(bytesRead);
        return buf;
    }

    // Also try the +0x18/+0x28/+0x30 vector-like fallback layout.
    inline std::string TryFallbackLayout(uintptr_t holderAddr)
    {
        constexpr size_t kMinSize = 4;
        constexpr size_t kMaxSize = 5 * 1024 * 1024;
        constexpr size_t kProbeBytes = 4096;

        uintptr_t fallbackPtr = 0;
        size_t    dataSize = 0;
        size_t    fallbackCap = 0;

        if (!ReadMemorySilent(holderAddr + 0x18, fallbackPtr))  return {};
        if (!ReadMemorySilent(holderAddr + 0x28, dataSize))     return {};
        if (!ReadMemorySilent(holderAddr + 0x30, fallbackCap))  return {};

        if (fallbackPtr == 0)                              return {};
        if (dataSize < kMinSize || dataSize > kMaxSize)    return {};
        if (fallbackCap < dataSize)                        return {};
        if (!IsReadableMemory(fallbackPtr, (std::min)(dataSize, kProbeBytes))) return {};

        // Do not reject by the first byte here. The payload may be an
        // obfuscated RSB1 buffer; ResolveStandardPayload() validates it later.

        std::string buf(dataSize, '\0');
        SIZE_T bytesRead = 0;
        BOOL ok = ReadProcessMemory(
            Coms->GetHandle(),
            reinterpret_cast<LPCVOID>(fallbackPtr),
            buf.data(), dataSize, &bytesRead
        );
        if (!ok || bytesRead == 0) return {};
        if (bytesRead < dataSize)  buf.resize(bytesRead);
        return buf;
    }

    // Compact set of (ptrOff, sizeOff) pairs to try within a holder struct.
    // Covers both the configured pair and the most common alternatives seen
    // in Roblox bytecode holders (std::string / small-vector layouts).
    inline std::vector<std::pair<uintptr_t, uintptr_t>> InnerOffsetCandidates()
    {
        // Ordered by likelihood: configured first, then common layouts.
        std::vector<std::pair<uintptr_t, uintptr_t>> v;

        // Inject the configured pair first (if non-zero) so it gets priority.
        if (Offsets::ByteCode::Pointer != 0 && Offsets::ByteCode::Size != 0) {
            v.push_back({ Offsets::ByteCode::Pointer, Offsets::ByteCode::Size });
        }

        // Common Roblox holder layouts (all 8-byte aligned, x64).
        // Format: (data-pointer-offset, size-offset)
        static const std::pair<uintptr_t, uintptr_t> kKnown[] = {
            { 0x00, 0x08 }, { 0x00, 0x10 }, { 0x00, 0x18 },
            { 0x08, 0x10 }, { 0x08, 0x18 }, { 0x08, 0x20 },
            { 0x10, 0x18 }, { 0x10, 0x20 }, { 0x10, 0x28 },
            { 0x18, 0x20 }, { 0x18, 0x28 }, { 0x18, 0x30 },
            { 0x20, 0x28 }, { 0x20, 0x30 }, { 0x20, 0x38 },
            { 0x28, 0x30 }, { 0x28, 0x38 }, { 0x28, 0x40 },
            { 0x30, 0x38 }, { 0x30, 0x40 }, { 0x38, 0x40 },
        };
        for (auto& p : kKnown) {
            if (p != std::make_pair(Offsets::ByteCode::Pointer, Offsets::ByteCode::Size))
                v.push_back(p);
        }
        return v;
    }

    inline std::vector<std::pair<uintptr_t, uintptr_t>> BeginEndCandidates()
    {
        return {
            { 0x00, 0x08 }, { 0x08, 0x10 }, { 0x10, 0x18 },
            { 0x18, 0x20 }, { 0x20, 0x28 }, { 0x28, 0x30 },
            { 0x30, 0x38 }, { 0x38, 0x40 }, { 0x40, 0x48 }
        };
    }

    // ScanForBytecodeOffset: brute-forces classOffset for a given script type.
    //
    // IMPORTANT: on success this returns BOTH the winning offsets and the exact
    // candidate/resolved payload that passed validation. Do not discard that buffer
    // and immediately reread it; some script holders can be transient/volatile.
    inline ScanHit ScanForBytecodeOffset(uintptr_t scriptAddr,
        const std::string& className)
    {
        constexpr uintptr_t kScanStart = 0x20;
        constexpr uintptr_t kScanEnd = 0x800;
        constexpr uintptr_t kStep = 0x08;

        std::cerr << "[OffsetScan] Starting scan for " << className
            << " at scriptAddr=0x" << std::hex << scriptAddr << std::dec
            << " over classOffset range [0x" << std::hex << kScanStart
            << ", 0x" << kScanEnd << std::dec << "]\n";

        const auto innerPairs = InnerOffsetCandidates();

        auto makeHit = [&](uintptr_t classOff, uintptr_t pOff, uintptr_t sOff,
            uint8_t derefCount, bool sizeIsEnd,
            std::string raw, BytecodeResult resolved) -> ScanHit
            {
                ScanHit hit;
                hit.offsets.classOffset = classOff;
                hit.offsets.ptrOff = pOff;
                hit.offsets.sizeOff = sOff;
                hit.offsets.derefCount = derefCount;
                hit.offsets.sizeIsEnd = sizeIsEnd;
                hit.rawCandidate = std::move(raw);
                hit.resolved = std::move(resolved);
                return hit;
            };

        for (uintptr_t classOff = kScanStart; classOff <= kScanEnd; classOff += kStep)
        {
            const uintptr_t fieldAddr = scriptAddr + classOff;
            if (!IsReadableMemory(fieldAddr, sizeof(uintptr_t))) continue;

            // Path A: inline / embedded holder.
            {
                const uintptr_t holderA = fieldAddr;
                if (IsReadableMemory(holderA, sizeof(uintptr_t))) {
                    for (auto& [pOff, sOff] : innerPairs) {
                        std::string raw = TryReadFromHolder(holderA, pOff, sOff);
                        if (raw.empty()) continue;

                        BytecodeResult r = ResolveStandardPayload(raw);
                        if (r.ok) {
                            std::cerr << "[OffsetScan] HIT (inline) classOff=0x"
                                << std::hex << classOff
                                << " ptrOff=0x" << pOff
                                << " sizeOff=0x" << sOff << std::dec << "\n";
                            return makeHit(classOff, pOff, sOff, 0, false,
                                std::move(raw), std::move(r));
                        }
                    }

                    std::string raw = TryFallbackLayout(holderA);
                    if (!raw.empty()) {
                        BytecodeResult r = ResolveStandardPayload(raw);
                        if (r.ok) {
                            std::cerr << "[OffsetScan] HIT (inline/fallback) classOff=0x"
                                << std::hex << classOff << std::dec << "\n";
                            return makeHit(classOff, 0x18, 0x28, 0, false,
                                std::move(raw), std::move(r));
                        }
                    }

                    for (auto& [bOff, eOff] : BeginEndCandidates()) {
                        std::string rangeRaw = TryReadFromBeginEnd(holderA, bOff, eOff);
                        if (rangeRaw.empty()) continue;

                        BytecodeResult rr = ResolveStandardPayload(rangeRaw);
                        if (rr.ok) {
                            std::cerr << "[OffsetScan] HIT (inline/begin-end) classOff=0x"
                                << std::hex << classOff
                                << " beginOff=0x" << bOff
                                << " endOff=0x" << eOff << std::dec << "\n";
                            return makeHit(classOff, bOff, eOff, 0, true,
                                std::move(rangeRaw), std::move(rr));
                        }
                    }
                }
            }

            // Path B: pointer-to-holder.
            {
                uintptr_t holderB = 0;
                if (!ReadMemorySilent(fieldAddr, holderB)) continue;
                if (holderB == 0 || !IsReadableMemory(holderB, sizeof(uintptr_t))) continue;

                for (auto& [pOff, sOff] : innerPairs) {
                    std::string raw = TryReadFromHolder(holderB, pOff, sOff);
                    if (raw.empty()) continue;

                    BytecodeResult r = ResolveStandardPayload(raw);
                    if (r.ok) {
                        std::cerr << "[OffsetScan] HIT (indirect) classOff=0x"
                            << std::hex << classOff
                            << " ptrOff=0x" << pOff
                            << " sizeOff=0x" << sOff << std::dec << "\n";
                        return makeHit(classOff, pOff, sOff, 1, false,
                            std::move(raw), std::move(r));
                    }
                }

                std::string raw = TryFallbackLayout(holderB);
                if (!raw.empty()) {
                    BytecodeResult r = ResolveStandardPayload(raw);
                    if (r.ok) {
                        std::cerr << "[OffsetScan] HIT (indirect/fallback) classOff=0x"
                            << std::hex << classOff << std::dec << "\n";
                        return makeHit(classOff, 0x18, 0x28, 1, false,
                            std::move(raw), std::move(r));
                    }
                }

                for (auto& [bOff, eOff] : BeginEndCandidates()) {
                    std::string rangeRaw = TryReadFromBeginEnd(holderB, bOff, eOff);
                    if (rangeRaw.empty()) continue;

                    BytecodeResult rr = ResolveStandardPayload(rangeRaw);
                    if (rr.ok) {
                        std::cerr << "[OffsetScan] HIT (indirect/begin-end) classOff=0x"
                            << std::hex << classOff
                            << " beginOff=0x" << bOff
                            << " endOff=0x" << eOff << std::dec << "\n";
                        return makeHit(classOff, bOff, eOff, 1, true,
                            std::move(rangeRaw), std::move(rr));
                    }
                }

                // Extra wrapper layer: holderC = **(script + classOff)
                uintptr_t holderC = 0;
                if (ReadMemorySilent(holderB, holderC) &&
                    holderC != 0 &&
                    holderC != holderB &&
                    IsReadableMemory(holderC, sizeof(uintptr_t)))
                {
                    for (auto& [pOff, sOff] : innerPairs) {
                        std::string raw2 = TryReadFromHolder(holderC, pOff, sOff);
                        if (raw2.empty()) continue;

                        BytecodeResult r2 = ResolveStandardPayload(raw2);
                        if (r2.ok) {
                            std::cerr << "[OffsetScan] HIT (double-indirect) classOff=0x"
                                << std::hex << classOff
                                << " ptrOff=0x" << pOff
                                << " sizeOff=0x" << sOff << std::dec << "\n";
                            return makeHit(classOff, pOff, sOff, 2, false,
                                std::move(raw2), std::move(r2));
                        }
                    }

                    for (auto& [bOff, eOff] : BeginEndCandidates()) {
                        std::string raw2 = TryReadFromBeginEnd(holderC, bOff, eOff);
                        if (raw2.empty()) continue;

                        BytecodeResult r2 = ResolveStandardPayload(raw2);
                        if (r2.ok) {
                            std::cerr << "[OffsetScan] HIT (double-indirect/begin-end) classOff=0x"
                                << std::hex << classOff
                                << " beginOff=0x" << bOff
                                << " endOff=0x" << eOff << std::dec << "\n";
                            return makeHit(classOff, bOff, eOff, 2, true,
                                std::move(raw2), std::move(r2));
                        }
                    }
                }
            }
        }

        std::cerr << "[OffsetScan] No structurally valid Luau payload found for "
            << className << "\n";
        return {};
    }

    inline bool TryGetCachedOffsets(const std::string& className, OffsetTriple& out)
    {
        std::lock_guard<std::mutex> lock(OffsetCacheMutex());
        const auto& cache = OffsetCache();
        auto it = cache.find(className);
        if (it == cache.end() || it->second.classOffset == 0)
            return false;
        out = it->second;
        return true;
    }

    inline void CacheOffsets(const std::string& className, const OffsetTriple& t)
    {
        if (t.classOffset == 0) return;
        std::lock_guard<std::mutex> lock(OffsetCacheMutex());
        OffsetCache()[className] = t;
    }

    inline void ClearCachedOffsets(const std::string& className)
    {
        std::lock_guard<std::mutex> lock(OffsetCacheMutex());
        OffsetCache().erase(className);
    }

    inline std::string OffsetTripleDescription(const OffsetTriple& t)
    {
        std::ostringstream s;
        s << "classOff=0x" << std::hex << t.classOffset
            << " ptrOff=0x" << t.ptrOff
            << " sizeOff=0x" << t.sizeOff << std::dec
            << " deref=" << unsigned(t.derefCount)
            << (t.sizeIsEnd ? " begin-end" : " ptr-size");
        return s.str();
    }

    // Use a discovered OffsetTriple to read the raw candidate bytes.
    // Returns an empty string on failure.
    inline bool ResolveHolderAddress(uintptr_t scriptAddr,
        const OffsetTriple& t,
        uintptr_t& holderAddr)
    {
        if (t.classOffset == 0) return false;

        holderAddr = scriptAddr + t.classOffset;

        for (uint8_t i = 0; i < t.derefCount; ++i) {
            uintptr_t next = 0;
            if (!ReadMemorySilent(holderAddr, next) || next == 0)
                return false;

            holderAddr = next;
            if (!IsReadableMemory(holderAddr, sizeof(uintptr_t)))
                return false;
        }

        return true;
    }

    inline std::string ReadWithTriple(uintptr_t scriptAddr, const OffsetTriple& t)
    {
        uintptr_t holderAddr = 0;
        if (!ResolveHolderAddress(scriptAddr, t, holderAddr))
            return {};

        if (t.sizeIsEnd)
            return TryReadFromBeginEnd(holderAddr, t.ptrOff, t.sizeOff);

        std::string raw = TryReadFromHolder(holderAddr, t.ptrOff, t.sizeOff);
        if (raw.empty() && t.derefCount <= 1)
            raw = TryFallbackLayout(holderAddr);
        return raw;
    }

    inline std::string ReadWithTripleVerbose(uintptr_t scriptAddr, const OffsetTriple& t)
    {
        uintptr_t holderAddr = 0;
        if (!ResolveHolderAddress(scriptAddr, t, holderAddr)) {
            std::cerr << "[ReadTriple] failed resolving holder; "
                << OffsetTripleDescription(t) << "\n";
            return {};
        }

        std::cerr << "[ReadTriple] resolved holder=0x"
            << std::hex << holderAddr << std::dec
            << " | " << OffsetTripleDescription(t) << "\n";

        if (t.sizeIsEnd) {
            std::string raw = TryReadFromBeginEnd(holderAddr, t.ptrOff, t.sizeOff);
            if (raw.empty())
                std::cerr << "[ReadTriple] begin/end candidate failed\n";
            return raw;
        }

        std::string raw = TryReadFromHolderVerbose(holderAddr, t.ptrOff, t.sizeOff);
        if (!raw.empty()) return raw;

        if (t.derefCount <= 1) {
            std::cerr << "[ReadTriple] ptr/size failed; trying legacy +0x18/+0x28/+0x30 fallback\n";
            raw = TryFallbackLayout(holderAddr);
        }

        return raw;
    }

    // Reads the bytecode-related payload from the selected script and then
    // resolves only standard/documented representations into raw Luau.
    inline BytecodeResult ReadScriptBytecodeResult(uintptr_t scriptAddr,
        const std::string& className)
    {
        if (scriptAddr == 0)
            return BytecodeFailure("Script address is null.");

        std::cerr << "[Bytecode] ===== ReadScriptBytecode =====\n"
            << "[Bytecode] scriptAddr  = 0x" << std::hex << scriptAddr << std::dec << "\n"
            << "[Bytecode] className   = " << className << "\n";

        uintptr_t classOffset = 0;
        if (className == "LocalScript")
            classOffset = Offsets::LocalScript::ByteCode;
        else if (className == "Script")
            classOffset = Offsets::Script::ByteCode;
        else if (className == "ModuleScript")
            classOffset = Offsets::ModuleScript::ByteCode;
        else
            return BytecodeFailure("Unsupported script class '" + className + "'.");

        std::cerr << "[Bytecode] classOffset = " << classOffset
            << " (0x" << std::hex << classOffset << std::dec << ")\n";

        // ── Primary path (configured offsets) ─────────────────────────────────
        bool primaryFailed = (classOffset == 0 ||
            Offsets::ByteCode::Pointer == 0 ||
            Offsets::ByteCode::Size == 0);

        if (!primaryFailed)
        {
            const uintptr_t holderField = scriptAddr + classOffset;
            std::cerr << "[Bytecode] holderField = 0x" << std::hex << holderField << std::dec << "\n";

            if (IsReadableMemory(holderField, sizeof(uintptr_t))) {
                uintptr_t holderAddr = 0;
                if (ReadMemoryChecked(holderField, holderAddr, "holderPtr") && holderAddr != 0
                    && IsReadableMemory(holderAddr, sizeof(uintptr_t)))
                {
                    DumpHolderFields(holderAddr, 0x40);

                    const uintptr_t pointerField = holderAddr + Offsets::ByteCode::Pointer;
                    const uintptr_t sizeField = holderAddr + Offsets::ByteCode::Size;

                    uintptr_t dataPtr = 0;
                    size_t    dataSize = 0;

                    if (ReadMemoryChecked(pointerField, dataPtr, "dataPtr") &&
                        ReadMemoryChecked(sizeField, dataSize, "dataSize"))
                    {
                        std::cerr << "[Bytecode] configured dataPtr=0x" << std::hex << dataPtr
                            << std::dec << " size=" << dataSize << "\n";

                        if (dataSize > 0 && dataSize <= 5 * 1024 * 1024)
                        {
                            // Handle unreadable configured pointer via fallback layout.
                            if (dataPtr == 0 || !IsReadableMemory(
                                dataPtr, (std::min)(dataSize, static_cast<size_t>(4096))))
                            {
                                uintptr_t fallbackPtr = 0;
                                size_t    fallbackCap = 0;
                                if (ReadMemoryChecked(holderAddr + 0x18, fallbackPtr, "fallbackPtr(+0x18)") &&
                                    ReadMemoryChecked(holderAddr + 0x30, fallbackCap, "fallbackCap(+0x30)") &&
                                    fallbackPtr != 0 && fallbackCap >= dataSize &&
                                    fallbackCap <= 5 * 1024 * 1024 &&
                                    IsReadableMemory(fallbackPtr, (std::min)(dataSize, static_cast<size_t>(4096))))
                                {
                                    dataPtr = fallbackPtr;
                                    std::cerr << "[Bytecode] using validated +0x18/+0x28/+0x30 payload layout.\n";
                                }
                            }

                            if (dataPtr != 0 && IsReadableMemory(
                                dataPtr, (std::min)(dataSize, static_cast<size_t>(4096))))
                            {
                                std::string candidate(dataSize, '\0');
                                SIZE_T bytesRead = 0;
                                SetLastError(0);
                                const BOOL ok = ReadProcessMemory(
                                    Coms->GetHandle(),
                                    reinterpret_cast<LPCVOID>(dataPtr),
                                    candidate.data(), dataSize, &bytesRead
                                );
                                const DWORD gle = GetLastError();

                                std::cerr << "[Bytecode] ReadBuffer ok=" << ok
                                    << " bytesRead=" << bytesRead << "/" << dataSize
                                    << " GLE=" << gle << "\n";

                                if (ok && bytesRead > 0) {
                                    if (bytesRead < candidate.size())
                                        candidate.resize(bytesRead);
                                    DumpBytePrefix("[Bytecode] RAW CANDIDATE", candidate, 32);
                                    BytecodeResult r = ResolveStandardPayload(candidate);
                                    if (r.ok) return r;
                                    std::cerr << "[Bytecode] Primary path resolved payload but format was invalid: "
                                        << r.error << "\n";
                                }
                            }
                        }
                    }
                }
            }
            std::cerr << "[Bytecode] Primary path failed; falling back to offset scanner.\n";
        }
        else
        {
            std::cerr << "[Bytecode] Configured offsets are zero; using offset scanner directly.\n";
        }

        // ── Offset scanner fallback ────────────────────────────────────────────
        std::cerr << "[Bytecode] ===== Offset Scanner =====\n";

        // First replay a class-level cached layout. If it fails for this particular
        // instance, invalidate it and perform a fresh scan. The fresh scan returns
        // the exact bytes that passed validation, so there is no second-read race.
        OffsetTriple cached{};
        if (TryGetCachedOffsets(className, cached))
        {
            std::cerr << "[OffsetScan] Using cached offsets for " << className
                << ": " << OffsetTripleDescription(cached) << "\n";

            std::string candidate = ReadWithTripleVerbose(scriptAddr, cached);
            if (!candidate.empty()) {
                DumpBytePrefix("[Bytecode] CACHED CANDIDATE", candidate, 32);
                BytecodeResult cachedResult = ResolveStandardPayload(candidate);
                if (cachedResult.ok) {
                    cachedResult.representation = "cached[" +
                        OffsetTripleDescription(cached) + "] -> " +
                        cachedResult.representation;
                    return cachedResult;
                }

                std::cerr << "[OffsetScan] Cached bytes were readable but payload validation failed: "
                    << cachedResult.error << "\n";
            }
            else {
                std::cerr << "[OffsetScan] Cached layout could not read this instance.\n";
            }

            std::cerr << "[OffsetScan] Clearing cached " << className
                << " layout and scanning this instance.\n";
            ClearCachedOffsets(className);
        }

        ScanHit hit = ScanForBytecodeOffset(scriptAddr, className);
        if (!hit.ok())
        {
            return BytecodeFailure(
                "Offset scan exhausted all candidates for '" + className + "'. "
                "Tried inline, 1x indirect, 2x indirect, ptr/size and begin/end holder layouts. "
                "The script may genuinely have no attached compiled bytecode, or its payload representation is unsupported. "
                "Verify scriptAddr=0x" +
                [&] {
                    std::ostringstream s; s << std::hex << scriptAddr; return s.str();
                }() + " is correct."
                    );
        }

        CacheOffsets(className, hit.offsets);

        std::cerr << "[OffsetScan] Cached winning layout for " << className
            << ": " << OffsetTripleDescription(hit.offsets) << "\n";
        DumpBytePrefix("[Bytecode] SCAN-HIT CANDIDATE", hit.rawCandidate, 32);

        // Critical fix: use the resolved payload produced by the successful scan.
        // Do NOT call ReadWithTriple() again here.
        hit.resolved.representation = "scanned[" +
            OffsetTripleDescription(hit.offsets) + "] -> " +
            hit.resolved.representation;
        return hit.resolved;
    }

    // Compatibility wrapper for any older caller that still expects std::string.
    inline std::string ReadScriptBytecode(uintptr_t scriptAddr,
        const std::string& className)
    {
        BytecodeResult r = ReadScriptBytecodeResult(scriptAddr, className);
        if (r.ok) return r.bytes;
        return "-- [Error] " + r.error;
    }

    inline std::string EscapeLuaString(const std::string& value)
    {
        std::string out = "\"";
        for (unsigned char ch : value) {
            switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch >= 32 && ch < 127) out += static_cast<char>(ch);
                else {
                    char tmp[8]{};
                    std::snprintf(tmp, sizeof(tmp), "\\x%02X", static_cast<unsigned>(ch));
                    out += tmp;
                }
            }
        }
        out += '"';
        return out;
    }

    inline bool IsLuaIdentifier(const std::string& s)
    {
        if (s.empty()) return false;
        auto first = [](unsigned char c) { return std::isalpha(c) != 0 || c == '_'; };
        auto rest = [](unsigned char c) { return std::isalnum(c) != 0 || c == '_'; };
        if (!first(static_cast<unsigned char>(s[0]))) return false;
        for (size_t i = 1; i < s.size(); ++i)
            if (!rest(static_cast<unsigned char>(s[i]))) return false;
        return true;
    }

    inline std::string ConstantToLua(const Constant& k)
    {
        std::ostringstream out;
        switch (k.type) {
        case 0: return "nil";
        case 1: return k.boolVal ? "true" : "false";
        case 2: out << k.numVal; return out.str();
        case 3: return EscapeLuaString(k.strVal);
        case 4: out << "import_0x" << std::hex << k.importId; return out.str();
        case 6: return "function_" + std::to_string(k.protoIndex);
        case 7: {
            // Roblox materializes Luau vector constants as Vector3 values.
            // Keeping the constant is critical for comparisons such as
            // Humanoid.MoveDirection == Vector3.new(0, 0, 0).
            out << "Vector3.new(" << k.vectorVal[0] << ", " << k.vectorVal[1]
                << ", " << k.vectorVal[2] << ")";
            return out.str();
        }
        case 9:
            out << k.intVal;
            return out.str();
        default: return "nil";
        }
    }

    inline std::string ConstantString(const Prototype& p, uint32_t index)
    {
        if (index < p.constants.size() && p.constants[index].type == 3)
            return p.constants[index].strVal;
        return {};
    }

    inline bool OpcodeHasAux(uint8_t op)
    {
        switch (op) {
        case LOP_GETGLOBAL: case LOP_SETGLOBAL: case LOP_GETIMPORT:
        case LOP_GETTABLEKS: case LOP_SETTABLEKS: case LOP_NAMECALL:
        case LOP_JUMPIFEQ: case LOP_JUMPIFLE: case LOP_JUMPIFLT:
        case LOP_JUMPIFNOTEQ: case LOP_JUMPIFNOTLE: case LOP_JUMPIFNOTLT:
        case LOP_NEWTABLE: case LOP_SETLIST: case LOP_FORGLOOP:
        case LOP_FASTCALL3: case LOP_LOADKX: case LOP_FASTCALL2:
        case LOP_FASTCALL2K: case LOP_JUMPXEQKNIL: case LOP_JUMPXEQKB:
        case LOP_JUMPXEQKN: case LOP_JUMPXEQKS: case LOP_GETUDATAKS:
        case LOP_SETUDATAKS: case LOP_NAMECALLUDATA:
        case LOP_NEWCLASSMEMBER: case LOP_CALLFB: case LOP_CMPPROTO:
            return true;
        default:
            return false;
        }
    }

    inline std::string ImportPathFromAux(uint32_t aux, const Prototype& p)
    {
        const uint32_t count = aux >> 30;
        const uint32_t ids[3] = { (aux >> 20) & 0x3ffu, (aux >> 10) & 0x3ffu, aux & 0x3ffu };
        if (count < 1 || count > 3) return {};

        std::string result;
        for (uint32_t i = 0; i < count; ++i) {
            std::string part = ConstantString(p, ids[i]);
            if (part.empty()) return {};
            if (i == 0) result = part;
            else if (IsLuaIdentifier(part)) result += "." + part;
            else result += "[" + EscapeLuaString(part) + "]";
        }
        return result;
    }

    // Decompiler reconstruction v9.2-readerfix: stable-register IR + conservative temp folding
    inline std::string DecompileLuau(const std::string& bytecode, const std::string& scriptPath) {
        if (bytecode.empty())
            return "-- [Error] Bytecode is empty or script has no compiled code.";

        // Log the exact string/buffer received from ReadScriptBytecode.
        DumpBytePrefix("[Decompiler] RECEIVED", bytecode);

        // Critical fix: ReadScriptBytecode historically used std::string for
        // both binary data and error messages. If it returned "-- [Error]...",
        // the old code treated '-' as the version byte, producing version 45.
        if (IsReaderError(bytecode)) {
            std::cerr << "[Decompiler] Input is reader diagnostic text; skipping bytecode parse.\n";
            return bytecode;
        }

        const uint8_t* data = reinterpret_cast<const uint8_t*>(bytecode.data());
        const uint8_t* end = data + bytecode.size();

        uint8_t version = *data++;
        if (version < 3 || version > 12) {
            std::stringstream ss;
            ss << "-- [Error] Unsupported Luau bytecode version: " << (int)version << "\n";
            ss << "-- Script Path: " << scriptPath << "\n";
            return ss.str();
        }

        uint8_t typeVersion = 0;
        if (version >= 4) {
            if (data >= end) return "-- [Error] Bytecode truncated before type version";
            typeVersion = *data++;
            if (typeVersion < 1 || typeVersion > 3)
                return "-- [Error] Unsupported Luau type encoding version: " + std::to_string(typeVersion);
        }

        // 1. Read String Table
        uint32_t stringCount = ReadVarInt(data, end);
        std::vector<std::string> stringTable;
        stringTable.reserve(stringCount);

        for (uint32_t i = 0; i < stringCount; ++i) {
            uint32_t strLen = ReadVarInt(data, end);
            if (data + strLen > end) return "-- [Error] Bytecode format corrupt (String Table overflow)";
            std::string s(reinterpret_cast<const char*>(data), strLen);
            data += strLen;
            stringTable.push_back(s);
        }

        // Type encoding v3 writes a userdata-type -> string mapping here, terminated by 0.
        if (version >= 4 && typeVersion >= 3) {
            while (data < end) {
                uint8_t tag = *data++;
                if (tag == 0) break;
                (void)ReadVarInt(data, end);
            }
            if (data > end) return "-- [Error] Bytecode corrupt (userdata type map)";
        }

        // 2. Read Prototypes
        uint32_t protoCount = ReadVarInt(data, end);
        std::vector<Prototype> prototypes;
        prototypes.reserve(protoCount);

        for (uint32_t i = 0; i < protoCount; ++i) {
            // Bytecode v12 prepends every serialized proto with its byte size.
            // This lets us parse the stable portion and safely skip v12-only
            // trailing metadata such as feedback/cost data.
            const uint8_t* protoEnd = end;
            if (version >= 12) {
                uint32_t protoSize = ReadVarInt(data, end);
                if (protoSize == 0 || data > end || static_cast<size_t>(end - data) < protoSize)
                    return "-- [Error] Bytecode corrupt (v12 proto size overflow)";
                protoEnd = data + protoSize;
            }

            if (data + 5 > protoEnd)
                return "-- [Error] Bytecode corrupt (proto header overflow)";

            Prototype p{};
            p.maxStackSize = *data++;
            p.numParameters = *data++;
            p.numUpvalues = *data++;
            p.isVararg = *data++;

            if (version >= 4) {
                data++; // flags
            }

            if (version >= 4) {
                uint32_t typeInfoSize = ReadVarInt(data, protoEnd);
                if (data + typeInfoSize > protoEnd)
                    return "-- [Error] Bytecode corrupt (TypeInfo overflow)";
                data += typeInfoSize;
            }

            // Read Code
            uint32_t insCount = ReadVarInt(data, protoEnd);
            p.code.reserve(insCount);
            for (uint32_t c = 0; c < insCount; ++c) {
                if (data + 4 > protoEnd) return "-- [Error] Bytecode corrupt (Code overflow)";
                uint32_t ins = *reinterpret_cast<const uint32_t*>(data);
                data += 4;
                p.code.push_back(ins);
            }

            // Read Constants
            uint32_t constCount = ReadVarInt(data, protoEnd);
            p.constants.reserve(constCount);
            for (uint32_t k = 0; k < constCount; ++k) {
                Constant c{};
                c.type = *data++;
                switch (c.type) {
                case 0: break; // Nil
                case 1: c.boolVal = (*data++ != 0); break; // Boolean
                case 2: // Number
                    if (data + 8 > protoEnd) return "-- [Error] Bytecode corrupt (Number Constant overflow)";
                    c.numVal = *reinterpret_cast<const double*>(data);
                    data += 8;
                    break;
                case 3: // String
                {
                    uint32_t strIdx = ReadVarInt(data, protoEnd);
                    if (strIdx > 0 && strIdx <= stringTable.size())
                        c.strVal = stringTable[strIdx - 1];
                }
                break;
                case 4: // Import
                    if (data + 4 > protoEnd) return "-- [Error] Import overflow";
                    c.importId = *reinterpret_cast<const uint32_t*>(data);
                    data += 4;
                    break;
                case 5: { // Table shape
                    uint32_t n = ReadVarInt(data, protoEnd);
                    c.tableKeys.reserve(n);
                    c.tableValues.assign(n, -1);
                    for (uint32_t ti = 0; ti < n; ++ti)
                        c.tableKeys.push_back(ReadVarInt(data, protoEnd));
                    break;
                }
                case 6: c.protoIndex = ReadVarInt(data, protoEnd); break; // Closure
                case 7: { // Vector4f
                    if (data + 16 > protoEnd) return "-- [Error] Vector constant overflow";
                    std::memcpy(c.vectorVal.data(), data, 16);
                    data += 16;
                    break;
                }
                case 8: { // Table shape with constants
                    uint32_t n = ReadVarInt(data, protoEnd);
                    c.tableKeys.reserve(n);
                    c.tableValues.reserve(n);
                    for (uint32_t ti = 0; ti < n; ++ti) {
                        c.tableKeys.push_back(ReadVarInt(data, protoEnd));
                        if (data + 4 > protoEnd) return "-- [Error] Table constant overflow";
                        int32_t valueIndex = 0;
                        std::memcpy(&valueIndex, data, sizeof(valueIndex));
                        data += 4;
                        c.tableValues.push_back(valueIndex);
                    }
                    break;
                }
                case 9: { // 64-bit integer: sign byte + varint magnitude
                    if (data >= protoEnd) return "-- [Error] Integer constant overflow";
                    const bool negative = (*data++ != 0);
                    uint64_t magnitude = 0;
                    unsigned shift = 0;
                    bool terminated = false;
                    for (int vi = 0; vi < 10 && data < protoEnd; ++vi) {
                        uint8_t byte = *data++;
                        magnitude |= uint64_t(byte & 0x7f) << shift;
                        if (!(byte & 0x80)) {
                            terminated = true;
                            break;
                        }
                        shift += 7;
                    }
                    if (!terminated)
                        return "-- [Error] Integer constant varint overflow";
                    if (negative) {
                        if (magnitude > uint64_t(INT64_MAX) + 1ull)
                            return "-- [Error] Integer constant magnitude overflow";
                        c.intVal = magnitude == uint64_t(INT64_MAX) + 1ull
                            ? INT64_MIN
                            : -static_cast<int64_t>(magnitude);
                    }
                    else {
                        if (magnitude > uint64_t(INT64_MAX))
                            return "-- [Error] Integer constant magnitude overflow";
                        c.intVal = static_cast<int64_t>(magnitude);
                    }
                    break;
                }
                default:
                    return "-- [Error] Unsupported constant tag: " + std::to_string(c.type);
                }
                p.constants.push_back(c);
            }

            // Child Prototypes
            uint32_t childProtoCount = ReadVarInt(data, protoEnd);
            p.childProtos.reserve(childProtoCount);
            for (uint32_t cp = 0; cp < childProtoCount; ++cp)
                p.childProtos.push_back(ReadVarInt(data, protoEnd));

            // Per-prototype source/debug header.
            p.debugLine = static_cast<int>(ReadVarInt(data, protoEnd));
            {
                const uint32_t debugNameRef = ReadVarInt(data, protoEnd);
                if (debugNameRef > 0 && debugNameRef <= stringTable.size())
                    p.debugName = stringTable[debugNameRef - 1];
            }
            if (data > protoEnd) return "-- [Error] Bytecode corrupt (proto debug header)";

            // Skip line info
            if (data < protoEnd) {
                uint8_t hasLineInfo = *data++;
                if (hasLineInfo) {
                    if (data >= protoEnd) return "-- [Error] Bytecode corrupt (lineInfo header)";
                    uint8_t lineGapLog2 = *data++;
                    if (data + insCount > protoEnd) return "-- [Error] Bytecode corrupt (lineInfo array)";
                    data += insCount;
                    uint32_t intervals = (insCount > 0) ? ((insCount - 1) >> lineGapLog2) + 1 : 0;
                    if (data + intervals * 4 > protoEnd) return "-- [Error] Bytecode corrupt (absLineInfo)";
                    data += intervals * 4;
                }
            }

            // Parse debug locals/upvalues when present. These names are part of
            // Luau's serialized debug metadata and are far better than v0/v1.
            if (data < protoEnd) {
                uint8_t hasDebugInfo = *data++;
                if (hasDebugInfo) {
                    uint32_t locVarCount = ReadVarInt(data, protoEnd);
                    p.debugLocals.reserve(locVarCount);
                    for (uint32_t lv = 0; lv < locVarCount; ++lv) {
                        if (data >= protoEnd) return "-- [Error] Bytecode corrupt (debug local)";
                        const uint32_t nameRef = ReadVarInt(data, protoEnd);
                        const uint32_t startPc = ReadVarInt(data, protoEnd);
                        const uint32_t endPc = ReadVarInt(data, protoEnd);
                        if (data >= protoEnd) return "-- [Error] Bytecode corrupt (debug local reg)";
                        const uint8_t reg = *data++;

                        DebugLocal dl;
                        if (nameRef > 0 && nameRef <= stringTable.size())
                            dl.name = stringTable[nameRef - 1];
                        dl.startPc = startPc;
                        dl.endPc = endPc;
                        dl.reg = reg;
                        p.debugLocals.push_back(std::move(dl));
                    }

                    uint32_t upvalDebugCount = ReadVarInt(data, protoEnd);
                    p.debugUpvalues.reserve(upvalDebugCount);
                    for (uint32_t uv = 0; uv < upvalDebugCount; ++uv) {
                        if (data >= protoEnd) return "-- [Error] Bytecode corrupt (debug upvalue)";
                        const uint32_t nameRef = ReadVarInt(data, protoEnd);
                        if (nameRef > 0 && nameRef <= stringTable.size())
                            p.debugUpvalues.push_back(stringTable[nameRef - 1]);
                        else
                            p.debugUpvalues.push_back({});
                    }
                }
            }

            if (version >= 12) {
                if (data > protoEnd)
                    return "-- [Error] Bytecode corrupt (v12 proto over-read)";

                if (data < protoEnd) {
                    std::cerr << "[Decompiler] v12 proto " << i
                        << ": skipped " << static_cast<size_t>(protoEnd - data)
                        << " byte(s) of trailing feedback/cost metadata\n";
                    data = protoEnd;
                }
            }

            prototypes.push_back(p);
        }

        uint32_t mainProto = ReadVarInt(data, end);
        if (mainProto >= prototypes.size() && !prototypes.empty())
            return "-- [Error] Bytecode corrupt (main proto out of range)";

        // 3. Reconstruct readable Luau from the main prototype.
        if (prototypes.empty())
            return "-- [Error] Bytecode contains no prototypes";

        // Pre-pass the main prototype to recover names for closures that are
        // immediately assigned to globals. This turns function_24-style names
        // into useful names such as configureAnimationSet where possible.
        std::unordered_map<uint32_t, std::string> protoFriendlyName;
        {
            const Prototype& mp = prototypes[mainProto];
            constexpr uint32_t kNoProto = 0xffffffffu;
            std::array<uint32_t, 256> closureReg{};
            closureReg.fill(kNoProto);

            for (size_t pc = 0; pc < mp.code.size(); ++pc) {
                const uint32_t ins = mp.code[pc];
                const uint8_t op = DecodeRobloxOpcode(static_cast<uint8_t>(ins & 0xff));
                const uint8_t a = static_cast<uint8_t>((ins >> 8) & 0xff);
                const uint8_t b = static_cast<uint8_t>((ins >> 16) & 0xff);
                const int16_t d = static_cast<int16_t>((ins >> 16) & 0xffff);

                uint32_t aux = 0;
                const bool hasAux = OpcodeHasAux(op);
                if (hasAux && pc + 1 < mp.code.size())
                    aux = mp.code[++pc];

                switch (op) {
                case LOP_NEWCLOSURE: {
                    const uint16_t childSlot = static_cast<uint16_t>(d);
                    closureReg[a] = childSlot < mp.childProtos.size()
                        ? mp.childProtos[childSlot] : kNoProto;
                    break;
                }
                case LOP_DUPCLOSURE: {
                    const uint16_t ci = static_cast<uint16_t>(d);
                    if (ci < mp.constants.size() && mp.constants[ci].type == 6)
                        closureReg[a] = mp.constants[ci].protoIndex;
                    else
                        closureReg[a] = kNoProto;
                    break;
                }
                case LOP_MOVE:
                    closureReg[a] = closureReg[b];
                    break;

                case LOP_SETGLOBAL: {
                    const uint32_t ci = aux & 0x00ffffffu;
                    const std::string name = ConstantString(mp, ci);
                    if (!name.empty() && closureReg[a] != kNoProto)
                        protoFriendlyName[closureReg[a]] = name;
                    break;
                }

                default:
                    // Any instruction that clearly overwrites A invalidates the
                    // closure identity in that register.
                    switch (op) {
                    case LOP_LOADNIL: case LOP_LOADB: case LOP_LOADN:
                    case LOP_LOADK: case LOP_LOADKX: case LOP_GETGLOBAL:
                    case LOP_GETUPVAL: case LOP_GETIMPORT: case LOP_GETTABLE:
                    case LOP_GETTABLEKS: case LOP_GETTABLEN: case LOP_GETUDATAKS:
                    case LOP_NAMECALL: case LOP_NAMECALLUDATA: case LOP_CALL:
                    case LOP_CALLFB: case LOP_ADD: case LOP_SUB: case LOP_MUL:
                    case LOP_DIV: case LOP_MOD: case LOP_POW: case LOP_IDIV:
                    case LOP_ADDK: case LOP_SUBK: case LOP_MULK: case LOP_DIVK:
                    case LOP_MODK: case LOP_POWK: case LOP_IDIVK: case LOP_SUBRK:
                    case LOP_DIVRK: case LOP_AND: case LOP_OR: case LOP_ANDK:
                    case LOP_ORK: case LOP_NOT: case LOP_MINUS: case LOP_LENGTH:
                    case LOP_CONCAT: case LOP_NEWTABLE: case LOP_DUPTABLE:
                    case LOP_GETVARARGS:
                        closureReg[a] = kNoProto;
                        break;
                    default:
                        break;
                    }
                    break;
                }
            }
        }

        auto ProtoName = [&](uint32_t protoIndex) -> std::string {
            if (protoIndex < prototypes.size() &&
                !prototypes[protoIndex].debugName.empty() &&
                IsLuaIdentifier(prototypes[protoIndex].debugName))
                return prototypes[protoIndex].debugName;

            auto it = protoFriendlyName.find(protoIndex);
            if (it != protoFriendlyName.end() && IsLuaIdentifier(it->second))
                return it->second;
            return "function_" + std::to_string(protoIndex);
            };

        auto SimplifyCondition = [](std::string cond) -> std::string {
            auto trim = [](std::string x) {
                while (!x.empty() && std::isspace(static_cast<unsigned char>(x.front()))) x.erase(x.begin());
                while (!x.empty() && std::isspace(static_cast<unsigned char>(x.back()))) x.pop_back();
                return x;
                };

            cond = trim(cond);
            const std::string pre = "not (";
            if (cond.size() > pre.size() + 1 && cond.rfind(pre, 0) == 0 && cond.back() == ')') {
                std::string inner = cond.substr(pre.size(), cond.size() - pre.size() - 1);
                struct Swap { const char* from; const char* to; };
                static const Swap swaps[] = {
                    {" ~= ", " == "}, {" == ", " ~= "},
                    {" >= ", " < "},  {" <= ", " > "},
                    {" > ", " <= "},  {" < ", " >= "}
                };
                for (const auto& sw : swaps) {
                    size_t pos = inner.find(sw.from);
                    if (pos != std::string::npos) {
                        inner.replace(pos, std::strlen(sw.from), sw.to);
                        return inner;
                    }
                }
                if (inner.rfind("not (", 0) == 0 && inner.back() == ')')
                    return inner.substr(5, inner.size() - 6);
                return "not " + inner;
            }
            return cond;
            };

        auto PrettyIndent = [](const std::string& src) -> std::string {
            std::istringstream in(src);
            std::ostringstream out;
            std::string line;
            int depth = 0;
            while (std::getline(in, line)) {
                std::string t = line;
                while (!t.empty() && std::isspace(static_cast<unsigned char>(t.front()))) t.erase(t.begin());
                if (t == "end" || t.rfind("else", 0) == 0 || t.rfind("elseif ", 0) == 0)
                    depth = (std::max)(0, depth - 1);
                out << std::string(static_cast<size_t>(depth) * 4, ' ') << t << "\n";
                if ((t.rfind("if ", 0) == 0 && t.size() >= 5 && t.substr(t.size() - 5) == " then") ||
                    t.rfind("for ", 0) == 0 || t.rfind("while ", 0) == 0 || t.rfind("repeat", 0) == 0 ||
                    t == "else" || t.rfind("elseif ", 0) == 0)
                    ++depth;
            }
            return out.str();
            };

        auto RemoveEmptyStructuredBlocks = [](const std::string& src) -> std::string {
            auto trim = [](std::string x) {
                while (!x.empty() && std::isspace(static_cast<unsigned char>(x.front()))) x.erase(x.begin());
                while (!x.empty() && std::isspace(static_cast<unsigned char>(x.back()))) x.pop_back();
                return x;
                };

            std::vector<std::string> lines;
            std::istringstream in(src);
            std::string line;
            while (std::getline(in, line)) lines.push_back(line);

            bool changed = true;
            while (changed) {
                changed = false;
                for (size_t i = 0; i + 1 < lines.size(); ++i) {
                    const std::string a = trim(lines[i]);
                    const std::string b = trim(lines[i + 1]);
                    if (a.rfind("if ", 0) == 0 && a.size() >= 5 && a.substr(a.size() - 5) == " then" && b == "end") {
                        lines.erase(lines.begin() + i, lines.begin() + i + 2);
                        changed = true;
                        break;
                    }
                    if (i + 2 < lines.size()) {
                        const std::string c = trim(lines[i + 2]);
                        if (a.rfind("if ", 0) == 0 && a.size() >= 5 && a.substr(a.size() - 5) == " then" && b == "else" && c == "end") {
                            lines.erase(lines.begin() + i, lines.begin() + i + 3);
                            changed = true;
                            break;
                        }
                    }
                }
            }

            std::ostringstream out;
            for (const std::string& l : lines) out << l << "\n";
            return out.str();
            };

        // Conservative post-IR cleanup.  The reconstruction pass deliberately
        // materializes VM registers for correctness; this pass folds only anonymous
        // vNN/_rNN temporaries with a single nearby use.  Named debug locals are never
        // folded, and calls are only moved when the use is immediately adjacent.
        auto CleanupAnonymousRegisters = [](const std::string& src,
            const std::unordered_set<std::string>& protectedNames) -> std::string {
                auto trim = [](std::string x) {
                    while (!x.empty() && std::isspace(static_cast<unsigned char>(x.front()))) x.erase(x.begin());
                    while (!x.empty() && std::isspace(static_cast<unsigned char>(x.back()))) x.pop_back();
                    return x;
                    };
                auto isBarrier = [&](const std::string& raw) {
                    const std::string t = trim(raw);
                    return t == "end" || t == "else" || t.rfind("elseif ", 0) == 0 ||
                        t.rfind("if ", 0) == 0 || t.rfind("for ", 0) == 0 ||
                        t.rfind("while ", 0) == 0 || t == "repeat" ||
                        t.rfind("until ", 0) == 0;
                    };
                auto tokenCount = [](const std::string& line, const std::string& tok) {
                    size_t count = 0;
                    for (size_t pos = 0; (pos = line.find(tok, pos)) != std::string::npos; ) {
                        const bool left = pos == 0 || !(std::isalnum(static_cast<unsigned char>(line[pos - 1])) || line[pos - 1] == '_');
                        const size_t e = pos + tok.size();
                        const bool right = e >= line.size() || !(std::isalnum(static_cast<unsigned char>(line[e])) || line[e] == '_');
                        if (left && right) ++count;
                        pos = e;
                    }
                    return count;
                    };
                auto replaceToken = [](std::string line, const std::string& tok, const std::string& value) {
                    for (size_t pos = 0; (pos = line.find(tok, pos)) != std::string::npos; ) {
                        const bool left = pos == 0 || !(std::isalnum(static_cast<unsigned char>(line[pos - 1])) || line[pos - 1] == '_');
                        const size_t e = pos + tok.size();
                        const bool right = e >= line.size() || !(std::isalnum(static_cast<unsigned char>(line[e])) || line[e] == '_');
                        if (left && right) {
                            line.replace(pos, tok.size(), value);
                            pos += value.size();
                        }
                        else pos = e;
                    }
                    return line;
                    };
                auto sideEffectful = [](const std::string& rhs) {
                    // Function/method calls and constructors may have observable ordering.
                    return rhs.find('(') != std::string::npos || rhs.find(':') != std::string::npos;
                    };

                std::vector<std::string> lines;
                std::istringstream in(src);
                std::string line;
                while (std::getline(in, line)) lines.push_back(line);

                const std::regex assignRe(R"(^\s*(?:local\s+)?((?:v|_r)[0-9]+)\s*=\s*(.+?)\s*$)");
                const std::regex anonRe(R"(^(?:v|_r)[0-9]+$)");

                // First collapse VM-style self-redefinition chains such as:
                //   v5 = UDim2.new
                //   ...
                //   v5 = v5(...)
                // and:
                //   v4 = upvalue_3
                //   v4 = v4.Position
                //
                // Only side-effect-free producers may cross intervening statements.
                for (unsigned pass = 0; pass < 12; ++pass) {
                    bool folded = false;
                    for (size_t i = 0; i < lines.size(); ++i) {
                        std::smatch m;
                        if (!std::regex_match(lines[i], m, assignRe)) continue;

                        const std::string var = m[1].str();
                        const std::string rhs = trim(m[2].str());
                        if (rhs.empty() || rhs == var) continue;
                        if (protectedNames.count(var)) continue;

                        const bool movable = !sideEffectful(rhs);
                        for (size_t j = i + 1; j < lines.size(); ++j) {
                            if (isBarrier(lines[j])) break;

                            std::smatch dm;
                            if (std::regex_match(lines[j], dm, assignRe) && dm[1].str() == var) {
                                const std::string nextRhs = trim(dm[2].str());
                                if (tokenCount(nextRhs, var) == 1 &&
                                    (movable || j == i + 1))
                                {
                                    const std::string replaced = replaceToken(nextRhs, var, rhs);
                                    const std::string prefix =
                                        lines[j].find("local ") != std::string::npos ? "local " : "";
                                    lines[j] = prefix + var + " = " + replaced;
                                    lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(i));
                                    folded = true;
                                }
                                break;
                            }

                            // If this value is observed before its self-redefinition,
                            // it cannot be collapsed into the later definition.
                            if (tokenCount(lines[j], var) != 0)
                                break;
                        }

                        if (folded) break;
                    }
                    if (!folded) break;
                }

                bool changed = true;
                for (unsigned pass = 0; pass < 8 && changed; ++pass) {
                    changed = false;
                    for (size_t i = 0; i < lines.size(); ++i) {
                        std::smatch m;
                        if (!std::regex_match(lines[i], m, assignRe)) continue;
                        const std::string var = m[1].str();
                        const std::string rhs = trim(m[2].str());
                        if (!std::regex_match(var, anonRe) || rhs.empty() || rhs == var) continue;
                        if (protectedNames.count(var)) continue;

                        // Never fold table/function construction statements or MULTRET temps.
                        if (rhs.rfind("{}", 0) == 0 || rhs.rfind("function", 0) == 0) continue;

                        size_t useLine = static_cast<size_t>(-1);
                        size_t uses = 0;
                        bool crossedBarrier = false;

                        // Count *all* uses until the VM temp is redefined.  The old pass
                        // only looked one line ahead for call expressions; that incorrectly
                        // transformed:
                        //
                        //   local v2 = template:Clone()
                        //   v2.Visible = true
                        //   v2.Text = ...
                        //
                        // into template:Clone().Visible = true and left later v2 uses
                        // dangling.  A temp may only be folded when it has exactly one
                        // remaining use before redefinition.
                        for (size_t j = i + 1; j < lines.size(); ++j) {
                            std::smatch dm;
                            if (std::regex_match(lines[j], dm, assignRe) && dm[1].str() == var)
                                break;

                            if (isBarrier(lines[j])) {
                                crossedBarrier = true;
                                break;
                            }

                            const size_t c = tokenCount(lines[j], var);
                            uses += c;
                            if (c) useLine = j;
                            if (uses > 1) break;
                        }

                        if (crossedBarrier || uses != 1 || useLine == static_cast<size_t>(-1))
                            continue;

                        // Calls/constructors can have side effects.  Only move them into an
                        // immediately adjacent single use; pure expressions may move a few
                        // straight-line statements safely.
                        if (sideEffectful(rhs) && useLine != i + 1)
                            continue;
                        if (!sideEffectful(rhs) && useLine > i + 16)
                            continue;

                        lines[useLine] = replaceToken(lines[useLine], var, rhs);
                        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(i));
                        changed = true;
                        break;
                    }
                }

                // Remove dead pure VM temporaries after propagation. A LOADK/GETIMPORT/
                // property-chain temp that has no later textual use is bytecode plumbing,
                // not a source local.
                for (size_t i = 0; i < lines.size();) {
                    std::smatch m;
                    if (!std::regex_match(lines[i], m, assignRe)) {
                        ++i;
                        continue;
                    }

                    const std::string var = m[1].str();
                    const std::string rhs = trim(m[2].str());

                    if (!std::regex_match(var, anonRe) || sideEffectful(rhs) ||
                        protectedNames.count(var)) {
                        ++i;
                        continue;
                    }

                    size_t uses = 0;
                    for (size_t j = i + 1; j < lines.size(); ++j) {
                        std::smatch dm;
                        if (std::regex_match(lines[j], dm, assignRe) && dm[1].str() == var)
                            break;
                        uses += tokenCount(lines[j], var);
                        if (uses) break;
                    }

                    if (uses == 0) {
                        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(i));
                        continue;
                    }
                    ++i;
                }

                // Remove exact no-op assignments left after folding: foo = foo.
                std::vector<std::string> kept;
                const std::regex noopRe(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\1\s*$)");
                for (const std::string& l : lines) {
                    if (!std::regex_match(l, noopRe)) kept.push_back(l);
                }

                std::ostringstream out;
                for (const std::string& l : kept) out << l << "\n";
                return out.str();
            };

        // V8: aliases recovered from NEWCLOSURE + CAPTURE instructions.
        // Key: child prototype index -> child upvalue index -> source expression.
        // This lets nested prototypes print the lexical value they actually capture
        // instead of anonymous upvalue_0/upvalue_1 placeholders.
        std::unordered_map<uint32_t, std::vector<std::string>> recoveredCaptures;

        auto MergeCaptureAlias = [&](uint32_t childProto, size_t upIndex, const std::string& alias) {
            if (alias.empty()) return;
            auto& v = recoveredCaptures[childProto];
            if (v.size() <= upIndex) v.resize(upIndex + 1);

            if (v[upIndex].empty()) {
                v[upIndex] = alias;
            }
            else if (v[upIndex] != alias) {
                // The same child prototype can theoretically be instantiated with
                // different captures. Do not substitute a misleading alias.
                v[upIndex].clear();
            }
            };

        auto reconstructProto = [&](uint32_t protoIndex) -> std::string
            {
                if (protoIndex >= prototypes.size())
                    return "-- invalid proto index\n";

                const Prototype& p = prototypes[protoIndex];

                struct DecodedInst {
                    size_t pc = 0;
                    size_t nextPc = 0;
                    uint8_t op = 0, a = 0, b = 0, c = 0;
                    int16_t d = 0;
                    int32_t e = 0;
                    uint32_t aux = 0;
                    bool hasAux = false;
                };

                std::vector<DecodedInst> insts;
                std::unordered_map<size_t, size_t> pcToInst;
                for (size_t pc = 0; pc < p.code.size();) {
                    DecodedInst di;
                    di.pc = pc;
                    const uint32_t ins = p.code[pc];
                    di.op = DecodeRobloxOpcode(static_cast<uint8_t>(ins & 0xff));
                    di.a = static_cast<uint8_t>((ins >> 8) & 0xff);
                    di.b = static_cast<uint8_t>((ins >> 16) & 0xff);
                    di.c = static_cast<uint8_t>((ins >> 24) & 0xff);
                    di.d = static_cast<int16_t>((ins >> 16) & 0xffff);
                    di.e = static_cast<int32_t>(ins) >> 8;
                    di.hasAux = OpcodeHasAux(di.op);
                    if (di.hasAux && pc + 1 < p.code.size()) {
                        di.aux = p.code[pc + 1];
                        di.nextPc = pc + 2;
                    }
                    else {
                        di.nextPc = pc + 1;
                    }
                    pcToInst[di.pc] = insts.size();
                    insts.push_back(di);
                    pc = di.nextPc;
                }

                auto jumpTargetD = [&](const DecodedInst& di) -> size_t {
                    const int64_t t = static_cast<int64_t>(di.pc) + 1 + static_cast<int64_t>(di.d);
                    return t < 0 ? 0 : static_cast<size_t>(t);
                    };
                auto jumpTargetE = [&](const DecodedInst& di) -> size_t {
                    const int64_t t = static_cast<int64_t>(di.pc) + 1 + static_cast<int64_t>(di.e);
                    return t < 0 ? 0 : static_cast<size_t>(t);
                    };
                auto isCondOp = [&](uint8_t op) {
                    switch (op) {
                    case LOP_JUMPIF: case LOP_JUMPIFNOT:
                    case LOP_JUMPIFEQ: case LOP_JUMPIFLE: case LOP_JUMPIFLT:
                    case LOP_JUMPIFNOTEQ: case LOP_JUMPIFNOTLE: case LOP_JUMPIFNOTLT:
                    case LOP_JUMPXEQKNIL: case LOP_JUMPXEQKB:
                    case LOP_JUMPXEQKN: case LOP_JUMPXEQKS:
                        return true;
                    default:
                        return false;
                    }
                    };
                auto isUncondJump = [&](uint8_t op) {
                    return op == LOP_JUMP || op == LOP_JUMPBACK || op == LOP_JUMPX;
                    };
                auto uncondTarget = [&](const DecodedInst& di) -> size_t {
                    return di.op == LOP_JUMPX ? jumpTargetE(di) : jumpTargetD(di);
                    };

                // Detect classic if/else bytecode:
                //   conditional -> elseStart
                //   then body
                //   JUMP -> end
                // elseStart:
                //   else body
                std::unordered_map<size_t, size_t> elseStartToEnd;
                std::unordered_set<size_t> skipUnconditionalJump;
                for (size_t ii = 0; ii < insts.size(); ++ii) {
                    const DecodedInst& ci = insts[ii];
                    if (!isCondOp(ci.op))
                        continue;

                    const size_t elsePc = jumpTargetD(ci);
                    auto eit = pcToInst.find(elsePc);
                    if (eit == pcToInst.end() || eit->second == 0)
                        continue;

                    const DecodedInst& beforeElse = insts[eit->second - 1];
                    if (!isUncondJump(beforeElse.op))
                        continue;

                    const size_t endPc = uncondTarget(beforeElse);
                    if (endPc > elsePc) {
                        elseStartToEnd[elsePc] = endPc;
                        skipUnconditionalJump.insert(beforeElse.pc);
                    }
                }

                // Detect while-style loops where a conditional at the loop head
                // jumps forward to the exit and a backedge returns to that head.
                std::unordered_map<size_t, size_t> whileHeaderToBack;
                std::unordered_set<size_t> structuredBackedges;
                for (const DecodedInst& ji : insts) {
                    if (!isUncondJump(ji.op))
                        continue;
                    const size_t target = uncondTarget(ji);
                    if (target >= ji.pc)
                        continue;

                    auto hit = pcToInst.find(target);
                    if (hit == pcToInst.end())
                        continue;

                    const DecodedInst& head = insts[hit->second];
                    if (isCondOp(head.op)) {
                        const size_t exitPc = jumpTargetD(head);
                        if (exitPc > ji.pc) {
                            whileHeaderToBack[target] = ji.pc;
                            structuredBackedges.insert(ji.pc);
                        }
                    }
                }

                // Match numeric loop setup with the FORNLOOP backedge.
                std::unordered_map<size_t, size_t> numericPrepToLoop;
                std::unordered_set<size_t> numericLoopControls;
                for (size_t ii = 0; ii < insts.size(); ++ii) {
                    if (insts[ii].op != LOP_FORNPREP)
                        continue;
                    for (size_t jj = ii + 1; jj < insts.size(); ++jj) {
                        if (insts[jj].op == LOP_FORNLOOP && insts[jj].a == insts[ii].a) {
                            const size_t back = jumpTargetD(insts[jj]);
                            if (back <= insts[ii].nextPc) {
                                numericPrepToLoop[insts[ii].pc] = insts[jj].pc;
                                numericLoopControls.insert(insts[jj].pc);
                                break;
                            }
                        }
                    }
                }

                // Generic FORGPREP instructions jump to their FORGLOOP control.
                std::unordered_map<size_t, size_t> genericPrepToLoop;
                std::unordered_set<size_t> genericLoopControls;
                for (const DecodedInst& di : insts) {
                    if (di.op == LOP_FORGPREP || di.op == LOP_FORGPREP_INEXT || di.op == LOP_FORGPREP_NEXT) {
                        const size_t control = jumpTargetD(di);
                        auto it = pcToInst.find(control);
                        if (it != pcToInst.end() && insts[it->second].op == LOP_FORGLOOP) {
                            genericPrepToLoop[di.pc] = control;
                            genericLoopControls.insert(control);
                        }
                    }
                }

                // Comparisons used as expressions are commonly encoded as
                //   JUMP<cond> -> second LOADB
                //   LOADB result, false/true, skip
                //   LOADB result, true/false
                // Treat these as a value expression instead of an if statement.
                struct BoolMaterialize {
                    size_t firstLoadPc = 0;
                    size_t secondLoadPc = 0;
                    uint8_t dest = 0;
                    bool fallthroughValue = false;
                    bool jumpValue = false;
                };
                std::unordered_map<size_t, BoolMaterialize> boolMaterialize;
                std::unordered_set<size_t> consumedBoolLoadPcs;
                for (size_t ii = 0; ii < insts.size(); ++ii) {
                    const DecodedInst& ci = insts[ii];
                    if (!isCondOp(ci.op) || ii + 1 >= insts.size())
                        continue;

                    const DecodedInst& first = insts[ii + 1];
                    if (first.op != LOP_LOADB)
                        continue;

                    const size_t target = jumpTargetD(ci);
                    auto tit = pcToInst.find(target);
                    if (tit == pcToInst.end())
                        continue;

                    const DecodedInst& second = insts[tit->second];
                    if (second.op != LOP_LOADB || first.a != second.a)
                        continue;

                    const size_t firstTarget = first.pc + 1 + unsigned(first.c);
                    if (first.c == 0 || firstTarget < second.nextPc)
                        continue;

                    boolMaterialize[ci.pc] = {
                        first.pc, second.pc, first.a,
                        first.b != 0, second.b != 0
                    };
                    consumedBoolLoadPcs.insert(first.pc);
                    consumedBoolLoadPcs.insert(second.pc);
                }

                // Remaining direct backward conditionals are naturally expressible
                // as repeat/until loops: the VM repeats while the jump condition is
                // true, so the source-level until condition is the fallthrough test.
                std::unordered_map<size_t, size_t> repeatCondStartToEnd;
                std::unordered_map<size_t, size_t> repeatUncondStartToEnd;
                for (const DecodedInst& di : insts) {
                    if (isCondOp(di.op)) {
                        const size_t target = jumpTargetD(di);
                        if (target < di.pc && whileHeaderToBack.find(target) == whileHeaderToBack.end())
                            repeatCondStartToEnd.emplace(target, di.pc);
                    }
                    else if (isUncondJump(di.op)) {
                        const size_t target = uncondTarget(di);
                        if (target < di.pc && !structuredBackedges.count(di.pc) &&
                            !numericLoopControls.count(di.pc) && !genericLoopControls.count(di.pc))
                            repeatUncondStartToEnd.emplace(target, di.pc);
                    }
                }

                std::array<std::string, 256> reg{};
                std::array<std::string, 256> regExpr{};
                for (size_t i = 0; i < reg.size(); ++i)
                    reg[i] = "v" + std::to_string(i);

                auto ParamName = [&](unsigned r) -> std::string {
                    const DebugLocal* best = nullptr;
                    for (const DebugLocal& dl : p.debugLocals) {
                        if (dl.reg != r || dl.name.empty() || !IsLuaIdentifier(dl.name)) continue;
                        // Parameters are live from the function entry in normal Luau debug info.
                        if (dl.startPc > 1) continue;
                        if (!best || dl.startPc < best->startPc ||
                            (dl.startPc == best->startPc && dl.endPc > best->endPc))
                            best = &dl;
                    }
                    return best ? best->name : ("p" + std::to_string(r));
                    };

                for (uint32_t i = 0; i < p.numParameters; ++i)
                    reg[i] = ParamName(i);

                auto R = [&](unsigned i) -> std::string& { return reg[i & 0xffu]; };
                auto K = [&](uint32_t i) -> std::string {
                    return i < p.constants.size() ? ConstantToLua(p.constants[i]) : ("CONST[" + std::to_string(i) + "]");
                    };

                auto childFunctionName = [&](uint16_t childSlot) -> std::string {
                    if (childSlot < p.childProtos.size())
                        return ProtoName(p.childProtos[childSlot]);
                    return "function_child_" + std::to_string(childSlot);
                    };

                auto comparisonText = [&](uint8_t op, const std::string& lhs, const std::string& rhs) -> std::string {
                    switch (op) {
                    case LOP_JUMPIFEQ:    return lhs + " == " + rhs;
                    case LOP_JUMPIFLE:    return lhs + " <= " + rhs;
                    case LOP_JUMPIFLT:    return lhs + " < " + rhs;
                    case LOP_JUMPIFNOTEQ: return lhs + " ~= " + rhs;
                    case LOP_JUMPIFNOTLE: return lhs + " > " + rhs;
                    case LOP_JUMPIFNOTLT: return lhs + " >= " + rhs;
                    default:              return lhs;
                    }
                    };

                auto jumpConditionText = [&](const DecodedInst& di) -> std::string {
                    switch (di.op) {
                    case LOP_JUMPIF:
                        return R(di.a);
                    case LOP_JUMPIFNOT:
                        return SimplifyCondition("not (" + R(di.a) + ")");
                    case LOP_JUMPIFEQ: case LOP_JUMPIFLE: case LOP_JUMPIFLT:
                    case LOP_JUMPIFNOTEQ: case LOP_JUMPIFNOTLE: case LOP_JUMPIFNOTLT:
                        return SimplifyCondition(comparisonText(di.op, R(di.a), R(di.aux & 0xffu)));
                    case LOP_JUMPXEQKNIL: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        return R(di.a) + (invert ? " ~= nil" : " == nil");
                    }
                    case LOP_JUMPXEQKB: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        const bool value = (di.aux & 1u) != 0;
                        return R(di.a) + (invert ? " ~= " : " == ") + (value ? "true" : "false");
                    }
                    case LOP_JUMPXEQKN: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        const uint32_t ci = di.aux & 0x00ffffffu;
                        return R(di.a) + (invert ? " ~= " : " == ") + K(ci);
                    }
                    case LOP_JUMPXEQKS: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        const uint32_t ci = di.aux & 0x00ffffffu;
                        std::string rhs = ConstantString(p, ci);
                        rhs = rhs.empty() ? ("CONST[" + std::to_string(ci) + "]") : EscapeLuaString(rhs);
                        return R(di.a) + (invert ? " ~= " : " == ") + rhs;
                    }
                    default:
                        return "true";
                    }
                    };

                auto executeCondition = [&](const DecodedInst& di) -> std::string {
                    switch (di.op) {
                    case LOP_JUMPIF:
                        return SimplifyCondition("not (" + R(di.a) + ")");
                    case LOP_JUMPIFNOT:
                        return SimplifyCondition(R(di.a));
                    case LOP_JUMPIFEQ: case LOP_JUMPIFLE: case LOP_JUMPIFLT:
                    case LOP_JUMPIFNOTEQ: case LOP_JUMPIFNOTLE: case LOP_JUMPIFNOTLT: {
                        const std::string jumpCond = comparisonText(di.op, R(di.a), R(di.aux & 0xffu));
                        return SimplifyCondition("not (" + jumpCond + ")");
                    }
                    case LOP_JUMPXEQKNIL: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        const std::string jumpCond = R(di.a) + (invert ? " ~= nil" : " == nil");
                        return SimplifyCondition("not (" + jumpCond + ")");
                    }
                    case LOP_JUMPXEQKB: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        const bool value = (di.aux & 1u) != 0;
                        const std::string jumpCond = R(di.a) + (invert ? " ~= " : " == ") + (value ? "true" : "false");
                        return SimplifyCondition("not (" + jumpCond + ")");
                    }
                    case LOP_JUMPXEQKN: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        const uint32_t ci = di.aux & 0x00ffffffu;
                        const std::string jumpCond = R(di.a) + (invert ? " ~= " : " == ") + K(ci);
                        return SimplifyCondition("not (" + jumpCond + ")");
                    }
                    case LOP_JUMPXEQKS: {
                        const bool invert = (di.aux & 0x80000000u) != 0;
                        const uint32_t ci = di.aux & 0x00ffffffu;
                        std::string rhs = ConstantString(p, ci);
                        rhs = rhs.empty() ? ("CONST[" + std::to_string(ci) + "]") : EscapeLuaString(rhs);
                        const std::string jumpCond = R(di.a) + (invert ? " ~= " : " == ") + rhs;
                        return SimplifyCondition("not (" + jumpCond + ")");
                    }
                    default:
                        return "true";
                    }
                    };

                struct Block {
                    size_t endPc = 0;
                    size_t elsePc = static_cast<size_t>(-1);
                    bool elsePending = false;
                    bool loop = false;
                    bool repeatLoop = false;
                    bool repeatInfinite = false;
                };

                std::vector<Block> blocks;
                std::unordered_map<unsigned, std::string> multiRetCall;
                std::ostringstream body;
                unsigned tempCounter = 0;

                // v5-fixed: keep VM register values stable instead of aggressively
                // substituting expressions through later control-flow. Luau reuses
                // registers heavily; expression propagation across a branch/loop was
                // the main cause of lost locals, constant loop indices and 0*1 output.
                std::array<bool, 256> stableReg{};
                std::unordered_set<std::string> declaredNames;
                std::unordered_set<std::string> capturedLocalNames;
                size_t currentPc = 0;
                size_t currentNextPc = 0;

                auto DebugLocalName = [&](unsigned r) -> std::string {
                    r &= 0xffu;
                    // Prefer the narrowest active debug-local range for this PC.
                    const DebugLocal* best = nullptr;
                    for (const DebugLocal& dl : p.debugLocals) {
                        if (dl.reg != r || dl.name.empty() || !IsLuaIdentifier(dl.name))
                            continue;
                        if (currentPc < dl.startPc || currentPc >= dl.endPc)
                            continue;
                        if (!best || (dl.endPc - dl.startPc) < (best->endPc - best->startPc))
                            best = &dl;
                    }

                    // Luau debug locals commonly become live *after* the instruction
                    // that computes their initializer.  If no local is active at the
                    // current PC, allow a local whose lifetime starts at nextPc to name
                    // the value written by this instruction.
                    if (!best && currentNextPc != 0) {
                        for (const DebugLocal& dl : p.debugLocals) {
                            if (dl.reg != r || dl.name.empty() || !IsLuaIdentifier(dl.name))
                                continue;
                            if (dl.startPc != currentNextPc)
                                continue;
                            if (!best || (dl.endPc - dl.startPc) < (best->endPc - best->startPc))
                                best = &dl;
                        }
                    }

                    return best ? best->name : std::string();
                    };

                auto StableName = [&](unsigned r) -> std::string {
                    r &= 0xffu;
                    std::string debug = DebugLocalName(r);
                    if (!debug.empty())
                        return debug;
                    if (r < p.numParameters)
                        return "p" + std::to_string(r);
                    return "v" + std::to_string(r);
                    };

                auto U = [&](unsigned i) -> std::string {
                    auto capIt = recoveredCaptures.find(protoIndex);
                    if (capIt != recoveredCaptures.end() &&
                        i < capIt->second.size() &&
                        !capIt->second[i].empty())
                        return capIt->second[i];

                    if (i < p.debugUpvalues.size() &&
                        !p.debugUpvalues[i].empty() &&
                        IsLuaIdentifier(p.debugUpvalues[i]))
                        return p.debugUpvalues[i];

                    return "upvalue_" + std::to_string(i);
                    };

                auto IsAnonymousRegName = [](const std::string& x) -> bool {
                    if (x.size() < 2 || x[0] != 'v') return false;
                    for (size_t i = 1; i < x.size(); ++i)
                        if (!std::isdigit(static_cast<unsigned char>(x[i])))
                            return false;
                    return true;
                    };

                auto IsSafeInlineExpr = [](const std::string& x) -> bool {
                    if (x.empty()) return false;

                    // Calls/method calls can have side effects and must not be copied.
                    if (x.find('(') != std::string::npos || x.find(':') != std::string::npos)
                        return false;

                    // Tables/functions/multiret placeholders are materialized.
                    if (x.find('{') != std::string::npos ||
                        x.find("function") != std::string::npos ||
                        x.find("...") != std::string::npos)
                        return false;

                    return true;
                    };

                auto Value = [&](unsigned r) -> std::string {
                    r &= 0xffu;
                    const std::string n = R(r);
                    if (IsAnonymousRegName(n) &&
                        !regExpr[r].empty() &&
                        IsSafeInlineExpr(regExpr[r]))
                        return regExpr[r];
                    return n;
                    };

                auto Callable = [&](unsigned r) -> std::string {
                    r &= 0xffu;
                    const std::string n = R(r);

                    // GETGLOBAL/GETIMPORT/GETUPVAL followed by CALL commonly creates
                    // a temporary only because of the VM register model. Recover the
                    // original callable expression when moving it is side-effect free.
                    if (IsAnonymousRegName(n) &&
                        !regExpr[r].empty() &&
                        IsSafeInlineExpr(regExpr[r]))
                        return regExpr[r];
                    return n;
                    };

                auto Store = [&](unsigned r, const std::string& expr) {
                    r &= 0xffu;
                    const std::string name = StableName(r);
                    if (r < p.numParameters) {
                        body << name << " = " << expr << "\n";
                    }
                    else if (!stableReg[r] || !declaredNames.count(name)) {
                        body << "local " << name << " = " << expr << "\n";
                        stableReg[r] = true;
                        declaredNames.insert(name);
                    }
                    else {
                        body << name << " = " << expr << "\n";
                    }
                    R(r) = name;
                    regExpr[r] = expr;
                    multiRetCall.erase(r);
                    };

                auto RecoverChildCaptures = [&](uint32_t childProto, size_t closureInstIndex) {
                    if (childProto >= prototypes.size())
                        return;

                    const size_t expected = prototypes[childProto].numUpvalues;
                    for (size_t uv = 0; uv < expected && closureInstIndex + 1 + uv < insts.size(); ++uv) {
                        const DecodedInst& cap = insts[closureInstIndex + 1 + uv];
                        if (cap.op != LOP_CAPTURE)
                            break;

                        std::string alias;

                        // Luau CAPTURE:
                        //   A=0 VAL  : capture current value from parent register B
                        //   A=1 REF  : capture parent register/cell B by reference
                        //   A=2 UPVAL: capture parent upvalue B
                        //
                        // Important: a closure captures a lexical VALUE/CELL, not the
                        // initializer expression that happened to produce that register.
                        // Using regExpr here caused bugs such as:
                        //   upvalue_1:Clone():Destroy()
                        // and
                        //   false = not false
                        // because the child lost the parent variable identity.
                        if (cap.a == 0 || cap.a == 1) {
                            const unsigned srcReg = unsigned(cap.b) & 0xffu;
                            alias = R(srcReg);
                            if (alias.empty())
                                alias = StableName(srcReg);

                            // Captured anonymous VM locals must survive post-IR cleanup,
                            // even if there is no textual use in the parent function.
                            capturedLocalNames.insert(alias);
                        }
                        else if (cap.a == 2) {
                            alias = U(cap.b);
                        }

                        MergeCaptureAlias(childProto, uv, alias);
                    }
                    };

                auto closeOrElseAt = [&](size_t pc) {
                    bool changed = true;
                    while (changed && !blocks.empty()) {
                        changed = false;
                        Block& top = blocks.back();
                        if (top.elsePending && top.elsePc == pc) {
                            body << "else\n";
                            top.elsePending = false;
                            changed = true;
                        }
                        else if (top.endPc == pc && !top.repeatLoop) {
                            body << "end\n";
                            blocks.pop_back();
                            changed = true;
                        }
                    }
                    };

                auto beginIf = [&](const DecodedInst& di, const std::string& cond) -> bool {
                    const size_t target = jumpTargetD(di);
                    if (target <= di.pc || target > p.code.size())
                        return false;

                    // A jump to the very next decoded instruction has no source-level body.
                    if (target == di.nextPc)
                        return true;

                    auto eit = elseStartToEnd.find(target);
                    if (eit != elseStartToEnd.end()) {
                        body << "if " << SimplifyCondition(cond) << " then\n";
                        blocks.push_back({ eit->second, target, true, false });
                    }
                    else {
                        body << "if " << SimplifyCondition(cond) << " then\n";
                        blocks.push_back({ target, static_cast<size_t>(-1), false, false });
                    }
                    return true;
                    };

                auto nearestLoopEnd = [&]() -> size_t {
                    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it)
                        if (it->loop)
                            return it->endPc;
                    return static_cast<size_t>(-1);
                    };

                for (size_t ii = 0; ii < insts.size(); ++ii) {
                    const DecodedInst& di = insts[ii];
                    currentPc = di.pc;
                    currentNextPc = di.nextPc;
                    closeOrElseAt(di.pc);

                    // Close repeat loops on their control instruction.
                    if (!blocks.empty() && blocks.back().repeatLoop && blocks.back().endPc == di.pc) {
                        const bool infinite = blocks.back().repeatInfinite;
                        blocks.pop_back();
                        if (infinite)
                            body << "until false\n";
                        else
                            body << "until " << executeCondition(di) << "\n";
                        continue;
                    }

                    // Open previously-unstructured backward loops at their target.
                    auto rc = repeatCondStartToEnd.find(di.pc);
                    if (rc != repeatCondStartToEnd.end()) {
                        body << "repeat\n";
                        blocks.push_back({ rc->second, static_cast<size_t>(-1), false, true, true, false });
                    }
                    auto ru = repeatUncondStartToEnd.find(di.pc);
                    if (ru != repeatUncondStartToEnd.end()) {
                        body << "repeat\n";
                        blocks.push_back({ ru->second, static_cast<size_t>(-1), false, true, true, true });
                    }

                    // Convert recognized conditional+backedge patterns to while loops.
                    auto wh = whileHeaderToBack.find(di.pc);
                    if (wh != whileHeaderToBack.end() && isCondOp(di.op)) {
                        body << "while " << executeCondition(di) << " do\n";
                        blocks.push_back({ wh->second, static_cast<size_t>(-1), false, true });
                        continue;
                    }

                    const uint8_t op = di.op;
                    const uint8_t a = di.a, b = di.b, c = di.c;
                    const int16_t d = di.d;
                    const int32_t e = di.e;
                    const uint32_t aux = di.aux;

                    if (consumedBoolLoadPcs.count(di.pc))
                        continue;

                    auto bmi = boolMaterialize.find(di.pc);
                    if (bmi != boolMaterialize.end()) {
                        const BoolMaterialize& bm = bmi->second;
                        const std::string jc = SimplifyCondition(jumpConditionText(di));
                        std::string expr;
                        if (bm.jumpValue && !bm.fallthroughValue)
                            expr = jc;
                        else if (!bm.jumpValue && bm.fallthroughValue)
                            expr = SimplifyCondition("not (" + jc + ")");
                        else
                            expr = bm.jumpValue ? "true" : "false";
                        Store(bm.dest, expr);
                        continue;
                    }

                    switch (op) {
                    case LOP_NOP:
                    case LOP_BREAK:
                    case LOP_CLOSEUPVALS:
                    case LOP_COVERAGE:
                    case LOP_CAPTURE:
                    case LOP_PREPVARARGS:
                    case LOP_NATIVECALL:
                    case LOP_FASTCALL:
                    case LOP_FASTCALL1:
                    case LOP_FASTCALL2:
                    case LOP_FASTCALL2K:
                    case LOP_FASTCALL3:
                        break;

                    case LOP_LOADNIL:
                        Store(a, "nil");
                        break;

                    case LOP_LOADB:
                        Store(a, b ? "true" : "false");
                        // LOADB carries a short skip in C. Skip decoded instructions
                        // whose header is before the VM target.
                        if (c != 0) {
                            const size_t target = di.pc + 1 + unsigned(c);
                            while (ii + 1 < insts.size() && insts[ii + 1].pc < target)
                                ++ii;
                        }
                        break;

                    case LOP_LOADN:
                        Store(a, std::to_string(d));
                        break;
                    case LOP_LOADK:
                        Store(a, K(static_cast<uint16_t>(d)));
                        break;
                    case LOP_LOADKX:
                        Store(a, K(aux & 0x00ffffffu));
                        break;
                    case LOP_MOVE: {
                        auto mr = multiRetCall.find(b);
                        if (mr != multiRetCall.end()) {
                            multiRetCall[a] = mr->second;
                            R(a) = R(b);
                        }
                        else {
                            Store(a, R(b));
                        }
                        break;
                    }

                    case LOP_GETGLOBAL: {
                        const uint32_t ci = aux & 0x00ffffffu;
                        std::string name = ConstantString(p, ci);
                        Store(a, !name.empty() ? name : ("GLOBAL[" + std::to_string(ci) + "]"));
                        break;
                    }
                    case LOP_SETGLOBAL: {
                        const uint32_t ci = aux & 0x00ffffffu;
                        std::string name = ConstantString(p, ci);
                        const std::string lhs = !name.empty() ? name : ("GLOBAL[" + std::to_string(ci) + "]");
                        const std::string source = Value(a);
                        if (lhs != source)
                            body << lhs << " = " << source << "\n";
                        break;
                    }
                    case LOP_GETUPVAL:
                        Store(a, U(b));
                        break;
                    case LOP_SETUPVAL: {
                        const std::string lhs = U(b);
                        const std::string source = Value(a);
                        if (lhs != source)
                            body << lhs << " = " << source << "\n";
                        break;
                    }

                    case LOP_GETIMPORT: {
                        std::string path = ImportPathFromAux(aux, p);
                        if (path.empty() && static_cast<uint16_t>(d) < p.constants.size())
                            path = ConstantToLua(p.constants[static_cast<uint16_t>(d)]);
                        Store(a, path.empty() ? "import_value" : path);
                        break;
                    }

                    case LOP_GETTABLE:
                        Store(a, Value(b) + "[" + Value(c) + "]");
                        break;
                    case LOP_SETTABLE:
                        body << R(b) << "[" << R(c) << "] = " << R(a) << "\n";
                        break;

                    case LOP_GETTABLEKS: {
                        std::string key = ConstantString(p, aux & 0x00ffffffu);
                        Store(a, IsLuaIdentifier(key) ? (Value(b) + "." + key) : (Value(b) + "[" + EscapeLuaString(key) + "]"));
                        break;
                    }
                    case LOP_SETTABLEKS: {
                        std::string key = ConstantString(p, aux & 0x00ffffffu);
                        body << (IsLuaIdentifier(key) ? (R(b) + "." + key) : (R(b) + "[" + EscapeLuaString(key) + "]"))
                            << " = " << R(a) << "\n";
                        break;
                    }
                    case LOP_GETTABLEN:
                        Store(a, Value(b) + "[" + std::to_string(unsigned(c) + 1) + "]");
                        break;
                    case LOP_SETTABLEN:
                        body << R(b) << "[" << (unsigned(c) + 1) << "] = " << R(a) << "\n";
                        break;

                    case LOP_GETUDATAKS: {
                        std::string key = ConstantString(p, aux & 0xffffu);
                        Store(a, IsLuaIdentifier(key) ? (R(b) + "." + key) : (R(b) + "[" + EscapeLuaString(key) + "]"));
                        break;
                    }
                    case LOP_SETUDATAKS: {
                        std::string key = ConstantString(p, aux & 0xffffu);
                        body << (IsLuaIdentifier(key) ? (R(b) + "." + key) : (R(b) + "[" + EscapeLuaString(key) + "]"))
                            << " = " << R(a) << "\n";
                        break;
                    }

                    case LOP_NAMECALL:
                    case LOP_NAMECALLUDATA: {
                        const uint32_t ci = (op == LOP_NAMECALLUDATA) ? (aux & 0xffffu) : (aux & 0x00ffffffu);
                        std::string method = ConstantString(p, ci);
                        const std::string self = Value(b);
                        R(a) = self + ":" + (method.empty() ? "method" : method);
                        regExpr[a] = R(a);
                        R(unsigned(a) + 1) = self;
                        regExpr[unsigned(a) + 1] = self;
                        multiRetCall.erase(a);
                        break;
                    }

                    case LOP_CALL:
                    case LOP_CALLFB: {
                        std::ostringstream call;
                        call << Callable(a) << "(";

                        if (b == 0) {
                            // MULTRET argument tail: the final argument producer is kept
                            // as one expression (f(g())) instead of exploding it into
                            // select(1, g()), select(2, g()), ... and re-running g().
                            unsigned tailStart = 256;
                            for (const auto& mr : multiRetCall) {
                                if (mr.first > a && mr.first < tailStart)
                                    tailStart = mr.first;
                            }

                            bool first = true;
                            const bool methodCall = Callable(a).find(':') != std::string::npos;
                            const unsigned firstArg = methodCall ? 2u : 1u;
                            const unsigned fixedEnd = tailStart < 256 ? tailStart : unsigned(a) + firstArg;

                            for (unsigned rr = unsigned(a) + firstArg; rr < fixedEnd && rr < 256; ++rr) {
                                if (!first) call << ", ";
                                call << Value(rr);
                                first = false;
                            }

                            if (tailStart < 256) {
                                if (!first) call << ", ";
                                call << multiRetCall[tailStart];
                                first = false;
                            }
                            else if (first) {
                                call << "...";
                            }
                        }
                        else {
                            bool first = true;
                            for (unsigned arg = 1; arg < b; ++arg) {
                                if (arg == 1 && Callable(a).find(':') != std::string::npos)
                                    continue;
                                if (!first) call << ", ";
                                call << Value(unsigned(a) + arg);
                                first = false;
                            }
                        }

                        call << ")";
                        const std::string expr = call.str();

                        // A CALL consumes any variadic argument producer above A.
                        for (auto it = multiRetCall.begin(); it != multiRetCall.end();) {
                            if (it->first > a) it = multiRetCall.erase(it);
                            else ++it;
                        }

                        if (c == 1) {
                            body << expr << "\n";
                            R(a) = "v" + std::to_string(a);
                            regExpr[a].clear();
                            multiRetCall.erase(a);
                        }
                        else if (c == 0) {
                            // Keep the expression whole until a MULTRET-aware consumer
                            // (FORGPREP, CALL B=0, RETURN B=0, SETLIST C=0) uses it.
                            multiRetCall[a] = expr;
                            R(a) = expr;
                            regExpr[a] = expr;
                            for (unsigned ri = 1; ri < 8 && unsigned(a) + ri < 256; ++ri)
                                R(unsigned(a) + ri) = "v" + std::to_string(unsigned(a) + ri);
                        }
                        else {
                            const unsigned results = unsigned(c) - 1u;

                            bool consumedByGenericFor = false;
                            if (results >= 3 && ii + 1 < insts.size()) {
                                const DecodedInst& ni = insts[ii + 1];
                                consumedByGenericFor =
                                    (ni.op == LOP_FORGPREP ||
                                        ni.op == LOP_FORGPREP_INEXT ||
                                        ni.op == LOP_FORGPREP_NEXT) &&
                                    ni.a == a;
                            }

                            if (consumedByGenericFor) {
                                // pairs/ipairs-style iterator triples are one source
                                // expression in Luau even though the VM stores three
                                // protocol registers.
                                multiRetCall[a] = expr;
                                R(a) = expr;
                            }
                            else if (results == 1) {
                                multiRetCall.erase(a);
                                Store(a, expr);
                            }
                            else {
                                multiRetCall.erase(a);
                                // Evaluate a multi-result call exactly once. Repeating
                                // select(n, expr) duplicates side effects and was one of
                                // V4's biggest correctness bugs.
                                std::vector<std::string> names;
                                names.reserve(results);
                                for (unsigned ri = 0; ri < results && unsigned(a) + ri < 256; ++ri)
                                    names.push_back("_r" + std::to_string(tempCounter++));

                                body << "local ";
                                for (size_t ni = 0; ni < names.size(); ++ni) {
                                    if (ni) body << ", ";
                                    body << names[ni];
                                }
                                body << " = " << expr << "\n";

                                for (unsigned ri = 0; ri < names.size(); ++ri)
                                    R(unsigned(a) + ri) = names[ri];
                            }
                        }
                        break;
                    }

                    case LOP_RETURN:
                        if (b == 0) {
                            auto mr = multiRetCall.find(a);
                            if (mr != multiRetCall.end())
                                body << "return " << mr->second << "\n";
                            else
                                body << "return " << R(a) << ", ...\n";
                        }
                        else if (b <= 1) {
                            body << "return\n";
                        }
                        else {
                            body << "return ";
                            for (unsigned ri = 0; ri < unsigned(b) - 1; ++ri) {
                                if (ri) body << ", ";
                                body << R(unsigned(a) + ri);
                            }
                            body << "\n";
                        }
                        break;

                    case LOP_ADD: Store(a, "(" + Value(b) + " + " + Value(c) + ")"); break;
                    case LOP_SUB: Store(a, "(" + Value(b) + " - " + Value(c) + ")"); break;
                    case LOP_MUL: Store(a, "(" + Value(b) + " * " + Value(c) + ")"); break;
                    case LOP_DIV: Store(a, "(" + Value(b) + " / " + Value(c) + ")"); break;
                    case LOP_MOD: Store(a, "(" + Value(b) + " % " + Value(c) + ")"); break;
                    case LOP_POW: Store(a, "(" + Value(b) + " ^ " + Value(c) + ")"); break;
                    case LOP_IDIV: Store(a, "(" + Value(b) + " // " + Value(c) + ")"); break;

                    case LOP_ADDK: Store(a, "(" + Value(b) + " + " + K(c) + ")"); break;
                    case LOP_SUBK: Store(a, "(" + Value(b) + " - " + K(c) + ")"); break;
                    case LOP_MULK: Store(a, "(" + Value(b) + " * " + K(c) + ")"); break;
                    case LOP_DIVK: Store(a, "(" + Value(b) + " / " + K(c) + ")"); break;
                    case LOP_MODK: Store(a, "(" + R(b) + " % " + K(c) + ")"); break;
                    case LOP_POWK: Store(a, "(" + R(b) + " ^ " + K(c) + ")"); break;
                    case LOP_IDIVK: Store(a, "(" + R(b) + " // " + K(c) + ")"); break;

                    case LOP_SUBRK: Store(a, "(" + K(b) + " - " + R(c) + ")"); break;
                    case LOP_DIVRK: Store(a, "(" + K(b) + " / " + R(c) + ")"); break;
                    case LOP_AND: Store(a, "(" + Value(b) + " and " + Value(c) + ")"); break;
                    case LOP_OR: Store(a, "(" + Value(b) + " or " + Value(c) + ")"); break;
                    case LOP_ANDK: Store(a, "(" + R(b) + " and " + K(c) + ")"); break;
                    case LOP_ORK: Store(a, "(" + R(b) + " or " + K(c) + ")"); break;

                    case LOP_NOT: Store(a, "not (" + Value(b) + ")"); break;
                    case LOP_MINUS: Store(a, "-(" + Value(b) + ")"); break;
                    case LOP_LENGTH: Store(a, "#(" + Value(b) + ")"); break;

                    case LOP_CONCAT: {
                        std::ostringstream ex;
                        for (unsigned r = b; r <= c; ++r) {
                            if (r != b) ex << " .. ";
                            ex << Value(r);
                        }
                        Store(a, "(" + ex.str() + ")");
                        break;
                    }

                    case LOP_NEWTABLE: {
                        const std::string name = "_t" + std::to_string(tempCounter++);
                        body << "local " << name << " = {}\n";
                        R(a) = name;
                        multiRetCall.erase(a);
                        break;
                    }
                    case LOP_DUPTABLE: {
                        const std::string name = "_t" + std::to_string(tempCounter++);
                        const uint16_t ci = static_cast<uint16_t>(d);
                        body << "local " << name << " = {";
                        if (ci < p.constants.size()) {
                            const Constant& tc = p.constants[ci];
                            bool firstField = true;
                            if (tc.type == 8 && tc.tableKeys.size() == tc.tableValues.size()) {
                                for (size_t ti = 0; ti < tc.tableKeys.size(); ++ti) {
                                    const uint32_t keyIndex = tc.tableKeys[ti];
                                    const int32_t valueIndex = tc.tableValues[ti];
                                    if (valueIndex < 0 || keyIndex >= p.constants.size() ||
                                        static_cast<uint32_t>(valueIndex) >= p.constants.size())
                                        continue;
                                    const Constant& keyConst = p.constants[keyIndex];
                                    std::string keyText;
                                    if (keyConst.type == 3 && IsLuaIdentifier(keyConst.strVal))
                                        keyText = keyConst.strVal;
                                    else
                                        keyText = "[" + ConstantToLua(keyConst) + "]";
                                    if (!firstField) body << ", ";
                                    body << keyText << " = " << ConstantToLua(p.constants[static_cast<uint32_t>(valueIndex)]);
                                    firstField = false;
                                }
                            }
                        }
                        body << "}\n";
                        R(a) = name;
                        multiRetCall.erase(a);
                        break;
                    }
                    case LOP_SETLIST: {
                        if (c == 0) {
                            auto mr = multiRetCall.find(b);
                            if (mr != multiRetCall.end()) {
                                const std::string pack = "_pack" + std::to_string(tempCounter++);
                                body << "local " << pack << " = table.pack(" << mr->second << ")\n";
                                body << "for _i = 1, " << pack << ".n do\n";
                                body << "    " << R(a) << "[" << aux << " + _i] = " << pack << "[_i]\n";
                                body << "end\n";
                                multiRetCall.erase(mr);
                            }
                        }
                        else {
                            const unsigned count = unsigned(c) - 1u;
                            for (unsigned i = 0; i < count; ++i)
                                body << R(a) << "[" << (aux + i + 1u) << "] = " << R(unsigned(b) + i) << "\n";
                        }
                        break;
                    }

                    case LOP_NEWCLOSURE: {
                        const uint16_t childSlot = static_cast<uint16_t>(d);
                        const uint32_t childProto =
                            childSlot < p.childProtos.size()
                            ? p.childProtos[childSlot]
                            : 0xffffffffu;

                        RecoverChildCaptures(childProto, ii);
                        Store(a, childFunctionName(childSlot));
                        break;
                    }
                    case LOP_DUPCLOSURE: {
                        const uint16_t ci = static_cast<uint16_t>(d);
                        if (ci < p.constants.size() && p.constants[ci].type == 6) {
                            const uint32_t childProto = p.constants[ci].protoIndex;
                            RecoverChildCaptures(childProto, ii);
                            Store(a, ProtoName(childProto));
                        }
                        else {
                            Store(a, K(ci));
                        }
                        break;
                    }

                    case LOP_GETVARARGS:
                        if (b == 0) {
                            multiRetCall[a] = "...";
                            R(a) = "...";
                        }
                        else {
                            multiRetCall.erase(a);
                            const unsigned results = unsigned(b) - 1u;
                            if (results == 1) {
                                Store(a, "select(1, ...)");
                            }
                            else if (results > 1) {
                                std::vector<std::string> names;
                                for (unsigned i = 0; i < results && unsigned(a) + i < 256; ++i)
                                    names.push_back("_r" + std::to_string(tempCounter++));
                                body << "local ";
                                for (size_t ni = 0; ni < names.size(); ++ni) {
                                    if (ni) body << ", ";
                                    body << names[ni];
                                }
                                body << " = ...\n";
                                for (unsigned i = 0; i < names.size(); ++i)
                                    R(unsigned(a) + i) = names[i];
                            }
                        }
                        break;

                    case LOP_FORGPREP:
                    case LOP_FORGPREP_INEXT:
                    case LOP_FORGPREP_NEXT: {
                        auto git = genericPrepToLoop.find(di.pc);
                        if (git == genericPrepToLoop.end())
                            break;

                        unsigned varCount = 2;
                        auto ctlIt = pcToInst.find(git->second);
                        if (ctlIt != pcToInst.end()) {
                            const DecodedInst& ctl = insts[ctlIt->second];
                            if (ctl.op == LOP_FORGLOOP)
                                varCount = ctl.aux & 0xffu;
                        }
                        if (varCount == 0) varCount = 1;

                        std::ostringstream vars;
                        for (unsigned vi = 0; vi < varCount; ++vi) {
                            if (vi) vars << ", ";
                            const unsigned vr = unsigned(a) + 3u + vi;
                            const std::string vn = StableName(vr);
                            stableReg[vr & 0xffu] = true;
                            R(vr) = vn;
                            vars << vn;
                        }

                        std::string iteratorExpr;
                        auto mr = multiRetCall.find(a);
                        if (mr != multiRetCall.end()) {
                            iteratorExpr = mr->second;
                            multiRetCall.erase(mr);
                        }
                        else {
                            iteratorExpr = R(a) + ", " + R(unsigned(a) + 1) + ", " + R(unsigned(a) + 2);
                        }

                        body << "for " << vars.str() << " in " << iteratorExpr << " do\n";
                        blocks.push_back({ git->second, static_cast<size_t>(-1), false, true });
                        break;
                    }

                    case LOP_FORGLOOP:
                        // Closed by closeOrElseAt() before this control instruction.
                        break;

                    case LOP_FORNPREP: {
                        auto nit = numericPrepToLoop.find(di.pc);
                        if (nit == numericPrepToLoop.end())
                            break;

                        // FORN protocol keeps limit/step/index at A..A+2.
                        // On current Luau bytecode the body reads the user-visible
                        // loop value from the index slot; alias A+3 as well for
                        // compatibility with bytecode revisions/documentation.
                        const unsigned visibleReg = unsigned(a) + 2u;
                        const std::string start = R(visibleReg);
                        const std::string limit = R(a);
                        const std::string step = R(unsigned(a) + 1);
                        const std::string var = StableName(visibleReg);

                        stableReg[visibleReg & 0xffu] = true;
                        stableReg[(unsigned(a) + 3u) & 0xffu] = true;
                        R(visibleReg) = var;
                        R(unsigned(a) + 3u) = var;

                        body << "for " << var << " = " << start << ", " << limit;
                        if (step != "1")
                            body << ", " << step;
                        body << " do\n";
                        blocks.push_back({ nit->second, static_cast<size_t>(-1), false, true });
                        break;
                    }

                    case LOP_FORNLOOP:
                        // Closed by closeOrElseAt().
                        break;

                    case LOP_JUMP:
                    case LOP_JUMPBACK:
                    case LOP_JUMPX: {
                        if (skipUnconditionalJump.count(di.pc) ||
                            structuredBackedges.count(di.pc) ||
                            numericLoopControls.count(di.pc) ||
                            genericLoopControls.count(di.pc)) {
                            break;
                        }

                        const size_t target = uncondTarget(di);
                        if (target < di.pc) {
                            const size_t loopEnd = nearestLoopEnd();
                            if (loopEnd != static_cast<size_t>(-1))
                                body << "continue\n";
                            else
                                body << "-- unstructured backedge to bytecode pc " << target << "\n";
                        }
                        else {
                            const size_t loopEnd = nearestLoopEnd();
                            if (loopEnd != static_cast<size_t>(-1) && target > loopEnd)
                                body << "break\n";
                            else if (target != di.nextPc)
                                body << "-- forward branch to bytecode pc " << target << "\n";
                        }
                        break;
                    }

                    case LOP_JUMPIF: {
                        if (R(a) == "true") {
                            // Jump always taken; source block has no fallthrough body.
                        }
                        else if (R(a) == "false" || R(a) == "nil") {
                            // Jump never taken.
                        }
                        else {
                            beginIf(di, "not (" + R(a) + ")");
                        }
                        break;
                    }

                    case LOP_JUMPIFNOT: {
                        if (R(a) == "false" || R(a) == "nil") {
                            // Jump always taken.
                        }
                        else if (R(a) == "true") {
                            // Jump never taken.
                        }
                        else {
                            beginIf(di, R(a));
                        }
                        break;
                    }

                    case LOP_JUMPIFEQ:
                    case LOP_JUMPIFLE:
                    case LOP_JUMPIFLT:
                    case LOP_JUMPIFNOTEQ:
                    case LOP_JUMPIFNOTLE:
                    case LOP_JUMPIFNOTLT: {
                        const std::string rhs = R(aux & 0xffu);
                        const std::string jumpCond = comparisonText(op, R(a), rhs);
                        beginIf(di, "not (" + jumpCond + ")");
                        break;
                    }

                    case LOP_JUMPXEQKNIL: {
                        const bool invert = (aux & 0x80000000u) != 0;
                        const std::string jumpCond = R(a) + (invert ? " ~= nil" : " == nil");
                        beginIf(di, "not (" + jumpCond + ")");
                        break;
                    }
                    case LOP_JUMPXEQKB: {
                        const bool invert = (aux & 0x80000000u) != 0;
                        const bool value = (aux & 1u) != 0;
                        const std::string rhs = value ? "true" : "false";
                        const std::string jumpCond = R(a) + (invert ? " ~= " : " == ") + rhs;
                        beginIf(di, "not (" + jumpCond + ")");
                        break;
                    }
                    case LOP_JUMPXEQKN: {
                        const bool invert = (aux & 0x80000000u) != 0;
                        const uint32_t ci = aux & 0x00ffffffu;
                        const std::string jumpCond = R(a) + (invert ? " ~= " : " == ") + K(ci);
                        beginIf(di, "not (" + jumpCond + ")");
                        break;
                    }
                    case LOP_JUMPXEQKS: {
                        const bool invert = (aux & 0x80000000u) != 0;
                        const uint32_t ci = aux & 0x00ffffffu;
                        std::string rhs = ConstantString(p, ci);
                        rhs = rhs.empty() ? ("CONST[" + std::to_string(ci) + "]") : EscapeLuaString(rhs);
                        const std::string jumpCond = R(a) + (invert ? " ~= " : " == ") + rhs;
                        beginIf(di, "not (" + jumpCond + ")");
                        break;
                    }

                    case LOP_NEWCLASSMEMBER: {
                        std::string key = ConstantString(p, aux & 0x00ffffffu);
                        body << R(a) << "." << (key.empty() ? "member" : key) << " = " << R(c) << "\n";
                        break;
                    }
                    case LOP_CMPPROTO:
                        body << "-- closure/proto comparison omitted\n";
                        break;

                    default:
                        body << "-- op_" << unsigned(op) << " A=" << unsigned(a) << " B=" << unsigned(b)
                            << " C=" << unsigned(c)
                            << (di.hasAux ? (" AUX=" + std::to_string(aux)) : std::string()) << "\n";
                        break;
                    }
                }

                closeOrElseAt(p.code.size());
                while (!blocks.empty()) {
                    body << "end\n";
                    blocks.pop_back();
                }

                std::string cleaned = RemoveEmptyStructuredBlocks(PrettyIndent(body.str()));
                cleaned = CleanupAnonymousRegisters(cleaned, capturedLocalNames);
                return PrettyIndent(cleaned);
            };

        // Populate capture aliases before final printing. Start at the main
        // prototype, then walk its lexical children; reconstructing a parent records
        // the CAPTURE bindings needed by its nested prototypes.
        std::unordered_map<uint32_t, std::string> reconstructedCache;
        std::unordered_set<uint32_t> warming;

        std::function<void(uint32_t)> WarmPrototype = [&](uint32_t pi) {
            if (pi >= prototypes.size() || warming.count(pi) || reconstructedCache.count(pi))
                return;

            warming.insert(pi);
            reconstructedCache[pi] = reconstructProto(pi);
            for (uint32_t child : prototypes[pi].childProtos)
                WarmPrototype(child);
            warming.erase(pi);
            };

        WarmPrototype(mainProto);
        for (uint32_t i = 0; i < prototypes.size(); ++i)
            WarmPrototype(i);

        // Re-run once after all lexical aliases are known so parent-derived
        // upvalue substitutions are reflected in every cached body.
        reconstructedCache.clear();
        warming.clear();
        WarmPrototype(mainProto);
        for (uint32_t i = 0; i < prototypes.size(); ++i)
            WarmPrototype(i);

        std::ostringstream out;
        out << "-- Decompiled: " << scriptPath << "\n";
        out << "-- Luau bytecode v" << unsigned(version) << " | " << prototypes.size()
            << " proto(s) | " << bytecode.size() << " bytes | op=203 | reconstruct=v9.2-readerfix\n";

        for (uint32_t i = 0; i < prototypes.size(); ++i) {
            if (i == mainProto) continue;

            const Prototype& fp = prototypes[i];
            out << "\nlocal function " << ProtoName(i) << "(";
            for (uint32_t pidx = 0; pidx < fp.numParameters; ++pidx) {
                if (pidx) out << ", ";
                std::string pname;
                const DebugLocal* best = nullptr;
                for (const DebugLocal& dl : fp.debugLocals) {
                    if (dl.reg != pidx || dl.name.empty() || !IsLuaIdentifier(dl.name) || dl.startPc > 1)
                        continue;
                    if (!best || dl.startPc < best->startPc ||
                        (dl.startPc == best->startPc && dl.endPc > best->endPc))
                        best = &dl;
                }
                pname = best ? best->name : ("p" + std::to_string(pidx));
                out << pname;
            }
            if (fp.isVararg) {
                if (fp.numParameters) out << ", ";
                out << "...";
            }
            out << ")\n";

            std::istringstream functionLines(reconstructedCache[i]);
            std::string line;
            while (std::getline(functionLines, line)) {
                if (!line.empty())
                    out << "    " << line << "\n";
            }
            out << "end\n";
        }

        out << "\n-- Main Script Thread\n";
        out << reconstructedCache[mainProto];
        return out.str();
    }

    inline std::string DecompileScript(uintptr_t scriptAddr,
        const std::string& className,
        const std::string& scriptPath)
    {
        BytecodeResult r = ReadScriptBytecodeResult(scriptAddr, className);
        if (!r.ok) {
            return "-- [Error] " + r.error + "\n-- Script Path: " + scriptPath;
        }

        std::string output = DecompileLuau(r.bytes, scriptPath);
        if (!r.representation.empty() && output.rfind("-- Decompiled", 0) == 0) {
            output.insert(output.find('\n') + 1,
                "-- Payload: " + r.representation + "\n");
        }
        return output;
    }

}

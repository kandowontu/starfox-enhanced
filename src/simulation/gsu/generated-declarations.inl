// SPDX-License-Identifier: ISC
// Generated adaptation of Ares v148, 0aafd85789215e84e1e43415c07d4c88461b7899.
// Copyright (c) 2004-2025 ares team, Near et al.
// See LICENSE-ARES.txt for the complete permission notice.
// Regenerate with tools/codegen/generate-gsu.py; do not edit manually.

    auto instruction(n8 opcode) -> Task<void>;
    auto instructionSTOP() -> Task<void>;
    auto instructionNOP() -> Task<void>;
    auto instructionCACHE() -> Task<void>;
    auto instructionLSR() -> Task<void>;
    auto instructionROL() -> Task<void>;
    auto instructionBranch(bool take) -> Task<void>;
    auto instructionTO_MOVE(u32 n) -> Task<void>;
    auto instructionWITH(u32 n) -> Task<void>;
    auto instructionStore(u32 n) -> Task<void>;
    auto instructionLOOP() -> Task<void>;
    auto instructionALT1() -> Task<void>;
    auto instructionALT2() -> Task<void>;
    auto instructionALT3() -> Task<void>;
    auto instructionLoad(u32 n) -> Task<void>;
    auto instructionPLOT_RPIX() -> Task<void>;
    auto instructionSWAP() -> Task<void>;
    auto instructionCOLOR_CMODE() -> Task<void>;
    auto instructionNOT() -> Task<void>;
    auto instructionADD_ADC(u32 n) -> Task<void>;
    auto instructionSUB_SBC_CMP(u32 n) -> Task<void>;
    auto instructionMERGE() -> Task<void>;
    auto instructionAND_BIC(u32 n) -> Task<void>;
    auto instructionMULT_UMULT(u32 n) -> Task<void>;
    auto instructionSBK() -> Task<void>;
    auto instructionLINK(u32 n) -> Task<void>;
    auto instructionSEX() -> Task<void>;
    auto instructionASR_DIV2() -> Task<void>;
    auto instructionROR() -> Task<void>;
    auto instructionJMP_LJMP(u32 n) -> Task<void>;
    auto instructionLOB() -> Task<void>;
    auto instructionFMULT_LMULT() -> Task<void>;
    auto instructionIBT_LMS_SMS(u32 n) -> Task<void>;
    auto instructionFROM_MOVES(u32 n) -> Task<void>;
    auto instructionHIB() -> Task<void>;
    auto instructionOR_XOR(u32 n) -> Task<void>;
    auto instructionINC(u32 n) -> Task<void>;
    auto instructionGETC_RAMB_ROMB() -> Task<void>;
    auto instructionDEC(u32 n) -> Task<void>;
    auto instructionGETB() -> Task<void>;
    auto instructionIWT_LM_SM(u32 n) -> Task<void>;
    auto stop() -> void;
    auto color(n8 source) -> n8;
    auto plot(n8 x, n8 y) -> Task<void>;
    auto rpix(n8 x, n8 y) -> Task<n8>;
    auto flushPixelCache(PixelCache& cache) -> Task<void>;
    auto read(n24 address, n8 data = 0) -> Task<n8>;
    auto write(n24 address, n8 data) -> Task<void>;
    auto readOpcode(n16 address) -> Task<n8>;
    auto peekpipe() -> Task<n8>;
    auto pipe() -> Task<n8>;
    auto flushCache() -> void;
    auto readCache(n16 address) -> n8;
    auto writeCache(n16 address, n8 data) -> void;
    auto step(u32 clocks) -> Task<void>;
    auto syncROMBuffer() -> Task<void>;
    auto readROMBuffer() -> Task<n8>;
    auto updateROMBuffer() -> void;
    auto syncRAMBuffer() -> Task<void>;
    auto readRAMBuffer(n16 address) -> Task<n8>;
    auto writeRAMBuffer(n16 address, n8 data) -> Task<void>;
    auto readIO(n24 address, n8) -> n8;
    auto writeIO(n24 address, n8 data) -> void;

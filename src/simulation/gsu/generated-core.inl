// SPDX-License-Identifier: ISC
// Generated adaptation of Ares v148, 0aafd85789215e84e1e43415c07d4c88461b7899.
// Copyright (c) 2004-2025 ares team, Near et al.
// See LICENSE-ARES.txt for the complete permission notice.
// Regenerate with tools/codegen/generate-gsu.py; do not edit manually.

// ares/component/processor/gsu/instruction.cpp
auto Core::instruction(n8 opcode) -> Task<void> {
  #define op(id, name, ...) \
    case id: co_return co_await instruction##name(__VA_ARGS__); \

  #define op4(id, name) \
    case id+ 0: co_return co_await instruction##name((n4)opcode); \
    case id+ 1: co_return co_await instruction##name((n4)opcode); \
    case id+ 2: co_return co_await instruction##name((n4)opcode); \
    case id+ 3: co_return co_await instruction##name((n4)opcode); \

  #define op6(id, name) \
    op4(id, name) \
    case id+ 4: co_return co_await instruction##name((n4)opcode); \
    case id+ 5: co_return co_await instruction##name((n4)opcode); \

  #define op12(id, name) \
    op6(id, name) \
    case id+ 6: co_return co_await instruction##name((n4)opcode); \
    case id+ 7: co_return co_await instruction##name((n4)opcode); \
    case id+ 8: co_return co_await instruction##name((n4)opcode); \
    case id+ 9: co_return co_await instruction##name((n4)opcode); \
    case id+10: co_return co_await instruction##name((n4)opcode); \
    case id+11: co_return co_await instruction##name((n4)opcode); \

  #define op15(id, name) \
    op12(id, name) \
    case id+12: co_return co_await instruction##name((n4)opcode); \
    case id+13: co_return co_await instruction##name((n4)opcode); \
    case id+14: co_return co_await instruction##name((n4)opcode); \

  #define op16(id, name) \
    op15(id, name) \
    case id+15: co_return co_await instruction##name((n4)opcode); \

  switch(opcode) {
  op  (0x00, STOP)
  op  (0x01, NOP)
  op  (0x02, CACHE)
  op  (0x03, LSR)
  op  (0x04, ROL)
  op  (0x05, Branch, 1)  //bra
  op  (0x06, Branch, (regs.sfr.s ^ regs.sfr.ov) == 0)  //blt
  op  (0x07, Branch, (regs.sfr.s ^ regs.sfr.ov) == 1)  //bge
  op  (0x08, Branch, regs.sfr.z == 0)  //bne
  op  (0x09, Branch, regs.sfr.z == 1)  //beq
  op  (0x0a, Branch, regs.sfr.s == 0)  //bpl
  op  (0x0b, Branch, regs.sfr.s == 1)  //bmi
  op  (0x0c, Branch, regs.sfr.cy == 0)  //bcc
  op  (0x0d, Branch, regs.sfr.cy == 1)  //bcs
  op  (0x0e, Branch, regs.sfr.ov == 0)  //bvc
  op  (0x0f, Branch, regs.sfr.ov == 1)  //bvs
  op16(0x10, TO_MOVE)
  op16(0x20, WITH)
  op12(0x30, Store)
  op  (0x3c, LOOP)
  op  (0x3d, ALT1)
  op  (0x3e, ALT2)
  op  (0x3f, ALT3)
  op12(0x40, Load)
  op  (0x4c, PLOT_RPIX)
  op  (0x4d, SWAP)
  op  (0x4e, COLOR_CMODE)
  op  (0x4f, NOT)
  op16(0x50, ADD_ADC)
  op16(0x60, SUB_SBC_CMP)
  op  (0x70, MERGE)
  op15(0x71, AND_BIC)
  op16(0x80, MULT_UMULT)
  op  (0x90, SBK)
  op4 (0x91, LINK)
  op  (0x95, SEX)
  op  (0x96, ASR_DIV2)
  op  (0x97, ROR)
  op6 (0x98, JMP_LJMP)
  op  (0x9e, LOB)
  op  (0x9f, FMULT_LMULT)
  op16(0xa0, IBT_LMS_SMS)
  op16(0xb0, FROM_MOVES)
  op  (0xc0, HIB)
  op15(0xc1, OR_XOR)
  op15(0xd0, INC)
  op  (0xdf, GETC_RAMB_ROMB)
  op15(0xe0, DEC)
  op  (0xef, GETB)
  op16(0xf0, IWT_LM_SM)
  }

  #undef op
  #undef op4
  #undef op6
  #undef op12
  #undef op15
  #undef op16
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionSTOP() -> Task<void> {
  if(regs.cfgr.irq == 0) {
    regs.sfr.irq = 1;
    stop();
  }
  regs.sfr.g = 0;
  regs.pipeline = 0x01;  //nop
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionNOP() -> Task<void> {
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionCACHE() -> Task<void> {
  if(regs.cbr != (regs.r[15] & 0xfff0)) {
    regs.cbr = regs.r[15] & 0xfff0;
    flushCache();
  }
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionLSR() -> Task<void> {
  regs.sfr.cy = (regs.sr() & 1);
  regs.dr() = regs.sr() >> 1;
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionROL() -> Task<void> {
  bool carry = (regs.sr() & 0x8000);
  regs.dr() = (regs.sr() << 1) | regs.sfr.cy;
  regs.sfr.s  = (regs.dr() & 0x8000);
  regs.sfr.cy = carry;
  regs.sfr.z  = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionBranch(bool take) -> Task<void> {
  auto displacement = (i8)(co_await pipe());
  if(take) regs.r[15] += displacement;
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionTO_MOVE(u32 n) -> Task<void> {
  if(!regs.sfr.b) {
    regs.dreg = n;
  } else {
    regs.r[n] = regs.sr();
    regs.reset();
  }
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionWITH(u32 n) -> Task<void> {
  regs.sreg = n;
  regs.dreg = n;
  regs.sfr.b = 1;
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionStore(u32 n) -> Task<void> {
  regs.ramaddr = regs.r[n];
  (co_await writeRAMBuffer(regs.ramaddr, regs.sr()));
  if(!regs.sfr.alt1) (co_await writeRAMBuffer(regs.ramaddr ^ 1, regs.sr() >> 8));
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionLOOP() -> Task<void> {
  regs.r[12]--;
  regs.sfr.s = (regs.r[12] & 0x8000);
  regs.sfr.z = (regs.r[12] == 0);
  if(!regs.sfr.z) regs.r[15] = regs.r[13];
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionALT1() -> Task<void> {
  regs.sfr.b = 0;
  regs.sfr.alt1 = 1;
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionALT2() -> Task<void> {
  regs.sfr.b = 0;
  regs.sfr.alt2 = 1;
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionALT3() -> Task<void> {
  regs.sfr.b = 0;
  regs.sfr.alt1 = 1;
  regs.sfr.alt2 = 1;
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionLoad(u32 n) -> Task<void> {
  regs.ramaddr = regs.r[n];
  regs.dr() = (co_await readRAMBuffer(regs.ramaddr));
  if(!regs.sfr.alt1) regs.dr() |= (co_await readRAMBuffer(regs.ramaddr ^ 1)) << 8;
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionPLOT_RPIX() -> Task<void> {
  if(!regs.sfr.alt1) {
    (co_await plot(regs.r[1], regs.r[2]));
    regs.r[1]++;
  } else {
    regs.dr() = (co_await rpix(regs.r[1], regs.r[2]));
    regs.sfr.s = (regs.dr() & 0x8000);
    regs.sfr.z = (regs.dr() == 0);
  }
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionSWAP() -> Task<void> {
  regs.dr() = regs.sr() >> 8 | regs.sr() << 8;
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionCOLOR_CMODE() -> Task<void> {
  if(!regs.sfr.alt1) {
    regs.colr = color(regs.sr());
  } else {
    regs.por = regs.sr();
  }
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionNOT() -> Task<void> {
  regs.dr() = ~regs.sr();
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionADD_ADC(u32 n) -> Task<void> {
  if(!regs.sfr.alt2) n = regs.r[n];
  s32 r = regs.sr() + n + (regs.sfr.alt1 ? regs.sfr.cy : 0);
  regs.sfr.ov = ~(regs.sr() ^ n) & (n ^ r) & 0x8000;
  regs.sfr.s  = (r & 0x8000);
  regs.sfr.cy = (r >= 0x10000);
  regs.sfr.z  = ((n16)r == 0);
  regs.dr() = r;
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionSUB_SBC_CMP(u32 n) -> Task<void> {
  if(!regs.sfr.alt2 || regs.sfr.alt1) n = regs.r[n];
  s32 r = regs.sr() - n - (!regs.sfr.alt2 && regs.sfr.alt1 ? !regs.sfr.cy : 0);
  regs.sfr.ov = (regs.sr() ^ n) & (regs.sr() ^ r) & 0x8000;
  regs.sfr.s  = (r & 0x8000);
  regs.sfr.cy = (r >= 0);
  regs.sfr.z  = ((n16)r == 0);
  if(!regs.sfr.alt2 || !regs.sfr.alt1) regs.dr() = r;
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionMERGE() -> Task<void> {
  regs.dr() = (regs.r[7] & 0xff00) | (regs.r[8] >> 8);
  regs.sfr.ov = (regs.dr() & 0xc0c0);
  regs.sfr.s  = (regs.dr() & 0x8080);
  regs.sfr.cy = (regs.dr() & 0xe0e0);
  regs.sfr.z  = (regs.dr() & 0xf0f0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionAND_BIC(u32 n) -> Task<void> {
  if(!regs.sfr.alt2) n = regs.r[n];
  regs.dr() = regs.sr() & (regs.sfr.alt1 ? ~n : n);
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionMULT_UMULT(u32 n) -> Task<void> {
  if(!regs.sfr.alt2) n = regs.r[n];
  regs.dr() = (!regs.sfr.alt1 ? n16((i8)regs.sr() * (i8)n) : n16((n8)regs.sr() * (n8)n));
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  if(!regs.cfgr.ms0) (co_await step(regs.clsr ? 1 : 2));
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionSBK() -> Task<void> {
  (co_await writeRAMBuffer(regs.ramaddr ^ 0, regs.sr() >> 0));
  (co_await writeRAMBuffer(regs.ramaddr ^ 1, regs.sr() >> 8));
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionLINK(u32 n) -> Task<void> {
  regs.r[11] = regs.r[15] + n;
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionSEX() -> Task<void> {
  regs.dr() = (i8)regs.sr();
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionASR_DIV2() -> Task<void> {
  regs.sfr.cy = (regs.sr() & 1);
  regs.dr() = ((i16)regs.sr() >> 1) + (regs.sfr.alt1 ? ((regs.sr() + 1) >> 16) : 0);
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionROR() -> Task<void> {
  bool carry = (regs.sr() & 1);
  regs.dr() = (regs.sfr.cy << 15) | (regs.sr() >> 1);
  regs.sfr.s  = (regs.dr() & 0x8000);
  regs.sfr.cy = carry;
  regs.sfr.z  = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionJMP_LJMP(u32 n) -> Task<void> {
  if(!regs.sfr.alt1) {
    regs.r[15] = regs.r[n];
  } else {
    regs.pbr = regs.r[n] & 0x7f;
    regs.r[15] = regs.sr();
    regs.cbr = regs.r[15] & 0xfff0;
    flushCache();
  }
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionLOB() -> Task<void> {
  regs.dr() = regs.sr() & 0xff;
  regs.sfr.s = (regs.dr() & 0x80);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionFMULT_LMULT() -> Task<void> {
  n32 result = (i16)regs.sr() * (i16)regs.r[6];
  if(regs.sfr.alt1) regs.r[4] = result;
  regs.dr() = result >> 16;
  regs.sfr.s  = (regs.dr() & 0x8000);
  regs.sfr.cy = (result & 0x8000);
  regs.sfr.z  = (regs.dr() == 0);
  regs.reset();
  (co_await step((regs.cfgr.ms0 ? 3 : 7) * (regs.clsr ? 1 : 2)));
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionIBT_LMS_SMS(u32 n) -> Task<void> {
  if(regs.sfr.alt1) {
    regs.ramaddr = (co_await pipe()) << 1;
    n8 lo     = (co_await readRAMBuffer(regs.ramaddr ^ 0)) << 0;
    regs.r[n] = (co_await readRAMBuffer(regs.ramaddr ^ 1)) << 8 | lo;
  } else if(regs.sfr.alt2) {
    regs.ramaddr = (co_await pipe()) << 1;
    (co_await writeRAMBuffer(regs.ramaddr ^ 0, regs.r[n] >> 0));
    (co_await writeRAMBuffer(regs.ramaddr ^ 1, regs.r[n] >> 8));
  } else {
    regs.r[n] = (i8)(co_await pipe());
  }
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionFROM_MOVES(u32 n) -> Task<void> {
  if(!regs.sfr.b) {
    regs.sreg = n;
  } else {
    regs.dr() = regs.r[n];
    regs.sfr.ov = (regs.dr() & 0x80);
    regs.sfr.s  = (regs.dr() & 0x8000);
    regs.sfr.z  = (regs.dr() == 0);
    regs.reset();
  }
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionHIB() -> Task<void> {
  regs.dr() = regs.sr() >> 8;
  regs.sfr.s = (regs.dr() & 0x80);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionOR_XOR(u32 n) -> Task<void> {
  if(!regs.sfr.alt2) n = regs.r[n];
  regs.dr() = (!regs.sfr.alt1 ? (regs.sr() | n) : (regs.sr() ^ n));
  regs.sfr.s = (regs.dr() & 0x8000);
  regs.sfr.z = (regs.dr() == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionINC(u32 n) -> Task<void> {
  regs.r[n]++;
  regs.sfr.s = (regs.r[n] & 0x8000);
  regs.sfr.z = (regs.r[n] == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionGETC_RAMB_ROMB() -> Task<void> {
  if(!regs.sfr.alt2) {
    regs.colr = color((co_await readROMBuffer()));
  } else if(!regs.sfr.alt1) {
    (co_await syncRAMBuffer());
    regs.rambr = regs.sr() & 0x01;
  } else {
    (co_await syncROMBuffer());
    regs.rombr = regs.sr() & 0x7f;
  }
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionDEC(u32 n) -> Task<void> {
  regs.r[n]--;
  regs.sfr.s = (regs.r[n] & 0x8000);
  regs.sfr.z = (regs.r[n] == 0);
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionGETB() -> Task<void> {
  switch(regs.sfr.alt2 << 1 | regs.sfr.alt1 << 0) {
  case 0: regs.dr() = (co_await readROMBuffer()); break;
  case 1: regs.dr() = (co_await readROMBuffer()) << 8 | (n8)regs.sr(); break;
  case 2: regs.dr() = (regs.sr() & 0xff00) | (co_await readROMBuffer()); break;
  case 3: regs.dr() = (i8)(co_await readROMBuffer()); break;
  }
  regs.reset();
  co_return;
}

// ares/component/processor/gsu/instructions.cpp
auto Core::instructionIWT_LM_SM(u32 n) -> Task<void> {
  if(regs.sfr.alt1) {
    regs.ramaddr  = (co_await pipe()) << 0;
    regs.ramaddr |= (co_await pipe()) << 8;
    n8 lo     = (co_await readRAMBuffer(regs.ramaddr ^ 0)) << 0;
    regs.r[n] = (co_await readRAMBuffer(regs.ramaddr ^ 1)) << 8 | lo;
  } else if(regs.sfr.alt2) {
    regs.ramaddr  = (co_await pipe()) << 0;
    regs.ramaddr |= (co_await pipe()) << 8;
    (co_await writeRAMBuffer(regs.ramaddr ^ 0, regs.r[n] >> 0));
    (co_await writeRAMBuffer(regs.ramaddr ^ 1, regs.r[n] >> 8));
  } else {
    n8 lo     = (co_await pipe());
    regs.r[n] = (co_await pipe()) << 8 | lo;
  }
  regs.reset();
  co_return;
}

// ares/sfc/coprocessor/superfx/core.cpp
auto Core::stop() -> void {
  irq_line = true;
}

// ares/sfc/coprocessor/superfx/core.cpp
auto Core::color(n8 source) -> n8 {
  if(regs.por.highnibble) return (regs.colr & 0xf0) | (source >> 4);
  if(regs.por.freezehigh) return (regs.colr & 0xf0) | (source & 0x0f);
  return source;
}

// ares/sfc/coprocessor/superfx/core.cpp
auto Core::plot(n8 x, n8 y) -> Task<void> {
  if(!regs.por.transparent) {
    if(regs.scmr.md == 3) {
      if(regs.por.freezehigh) {
        if((regs.colr & 0x0f) == 0) co_return;
      } else {
        if(regs.colr == 0) co_return;
      }
    } else {
      if((regs.colr & 0x0f) == 0) co_return;
    }
  }

  n8 color = regs.colr;
  if(regs.por.dither && regs.scmr.md != 3) {
    if((x ^ y) & 1) color >>= 4;
    color &= 0x0f;
  }

  n16 offset = (y << 5) + (x >> 3);
  if(offset != pixelcache[0].offset) {
    (co_await flushPixelCache(pixelcache[1]));
    pixelcache[1] = pixelcache[0];
    pixelcache[0].bitpend = 0x00;
    pixelcache[0].offset = offset;
  }

  x = (x & 7) ^ 7;
  pixelcache[0].data[x] = color;
  pixelcache[0].bitpend |= 1 << x;
  if(pixelcache[0].bitpend == 0xff) {
    (co_await flushPixelCache(pixelcache[1]));
    pixelcache[1] = pixelcache[0];
    pixelcache[0].bitpend = 0x00;
  }
  co_return;
}

// ares/sfc/coprocessor/superfx/core.cpp
auto Core::rpix(n8 x, n8 y) -> Task<n8> {
  (co_await flushPixelCache(pixelcache[1]));
  (co_await flushPixelCache(pixelcache[0]));

  u32 cn = 0;  //character number
  switch(regs.por.obj ? 3 : regs.scmr.ht) {
  case 0: cn = ((x & 0xf8) << 1) + ((y & 0xf8) >> 3); break;
  case 1: cn = ((x & 0xf8) << 1) + ((x & 0xf8) >> 1) + ((y & 0xf8) >> 3); break;
  case 2: cn = ((x & 0xf8) << 1) + ((x & 0xf8) << 0) + ((y & 0xf8) >> 3); break;
  case 3: cn = ((y & 0x80) << 2) + ((x & 0x80) << 1) + ((y & 0x78) << 1) + ((x & 0x78) >> 3); break;
  }
  u32 bpp = 2 << (regs.scmr.md - (regs.scmr.md >> 1));  // = [regs.scmr.md]{ 2, 4, 4, 8 };
  u32 addr = 0x700000 + (cn * (bpp << 3)) + (regs.scbr << 10) + ((y & 0x07) * 2);
  n8  data = 0x00;
  x = (x & 7) ^ 7;

  for(u32 n : range(bpp)) {
    u32 byte = ((n >> 1) << 4) + (n & 1);  // = [n]{ 0, 1, 16, 17, 32, 33, 48, 49 };
    (co_await step(regs.clsr ? 5 : 6));
    data |= (((co_await read(addr + byte)) >> x) & 1) << n;
  }

  co_return data;
}

// ares/sfc/coprocessor/superfx/core.cpp
auto Core::flushPixelCache(PixelCache& cache) -> Task<void> {
  if(cache.bitpend == 0x00) co_return;

  n8 x = cache.offset << 3;
  n8 y = cache.offset >> 5;

  u32 cn = 0;  //character number
  switch(regs.por.obj ? 3 : regs.scmr.ht) {
  case 0: cn = ((x & 0xf8) << 1) + ((y & 0xf8) >> 3); break;
  case 1: cn = ((x & 0xf8) << 1) + ((x & 0xf8) >> 1) + ((y & 0xf8) >> 3); break;
  case 2: cn = ((x & 0xf8) << 1) + ((x & 0xf8) << 0) + ((y & 0xf8) >> 3); break;
  case 3: cn = ((y & 0x80) << 2) + ((x & 0x80) << 1) + ((y & 0x78) << 1) + ((x & 0x78) >> 3); break;
  }
  u32 bpp = 2 << (regs.scmr.md - (regs.scmr.md >> 1));  // = [regs.scmr.md]{ 2, 4, 4, 8 };
  u32 addr = 0x700000 + (cn * (bpp << 3)) + (regs.scbr << 10) + ((y & 0x07) * 2);

  for(u32 n : range(bpp)) {
    u32 byte = ((n >> 1) << 4) + (n & 1);  // = [n]{ 0, 1, 16, 17, 32, 33, 48, 49 };
    n8  data = 0x00;
    for(u32 x : range(8)) data |= ((cache.data[x] >> n) & 1) << x;
    if(cache.bitpend != 0xff) {
      (co_await step(regs.clsr ? 5 : 6));
      data &= cache.bitpend;
      data |= (co_await read(addr + byte)) & ~cache.bitpend;
    }
    (co_await step(regs.clsr ? 5 : 6));
    (co_await write(addr + byte, data));
  }

  cache.bitpend = 0x00;
  co_return;
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::read(n24 address, n8 data) -> Task<n8> {
  if((address & 0xc00000) == 0x000000) {  //$00-3f:0000-7fff,:8000-ffff
    while(!regs.scmr.ron) {
      (co_await step(6));


    }
    co_return rom.read((((address & 0x3f0000) >> 1) | (address & 0x7fff)) & romMask);
  }

  if((address & 0xe00000) == 0x400000) {  //$40-5f:0000-ffff
    while(!regs.scmr.ron) {
      (co_await step(6));


    }
    co_return rom.read(address & romMask);
  }

  if((address & 0xfe0000) == 0x700000) {  //$70-71:0000-ffff
    while(!regs.scmr.ran) {
      (co_await step(6));


    }
    co_return ram.read(address & ramMask);
  }

  co_return data;
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::write(n24 address, n8 data) -> Task<void> {
  if((address & 0xfe0000) == 0x700000) {  //$70-71:0000-ffff
    while(!regs.scmr.ran) {
      (co_await step(6));


    }
    co_return ram.write(address & ramMask, data);
  }
  co_return;
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::readOpcode(n16 address) -> Task<n8> {
  n16 offset = address - regs.cbr;
  if(offset < 512) {
    if(cache.valid[offset >> 4] == false) {
      u32 dp = offset & 0xfff0;
      u32 sp = (regs.pbr << 16) + ((regs.cbr + dp) & 0xfff0);
      for([[maybe_unused]] u32 n : range(16)) {
        (co_await step(regs.clsr ? 5 : 6));
        cache.buffer[dp++] = (co_await read(sp++));
      }
      cache.valid[offset >> 4] = true;
    } else {
      (co_await step(regs.clsr ? 1 : 2));
    }
    co_return cache.buffer[offset];
  }

  if(regs.pbr <= 0x5f) {
    //$00-5f:0000-ffff ROM
    (co_await syncROMBuffer());
    (co_await step(regs.clsr ? 5 : 6));
    co_return (co_await read(regs.pbr << 16 | address));
  } else {
    //$60-7f:0000-ffff RAM
    (co_await syncRAMBuffer());
    (co_await step(regs.clsr ? 5 : 6));
    co_return (co_await read(regs.pbr << 16 | address));
  }
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::peekpipe() -> Task<n8> {
  n8 result = regs.pipeline;
  regs.pipeline = (co_await readOpcode(regs.r[15]));
  regs.r[15].modified = false;
  co_return result;
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::pipe() -> Task<n8> {
  n8 result = regs.pipeline;
  regs.pipeline = (co_await readOpcode(++regs.r[15]));
  regs.r[15].modified = false;
  co_return result;
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::flushCache() -> void {
  for(u32 n : range(32)) cache.valid[n] = false;
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::readCache(n16 address) -> n8 {
  address = (address + regs.cbr) & 511;
  return cache.buffer[address];
}

// ares/sfc/coprocessor/superfx/memory.cpp
auto Core::writeCache(n16 address, n8 data) -> void {
  address = (address + regs.cbr) & 511;
  cache.buffer[address] = data;
  if((address & 15) == 15) cache.valid[address >> 4] = true;
}

// ares/sfc/coprocessor/superfx/timing.cpp
auto Core::step(u32 clocks) -> Task<void> {
  if(regs.romcl) {
    regs.romcl -= min(clocks, regs.romcl);
    if(regs.romcl == 0) {
      regs.sfr.r = 0;
      regs.romdr = (co_await read((regs.rombr << 16) + regs.r[14]));
    }
  }

  if(regs.ramcl) {
    regs.ramcl -= min(clocks, regs.ramcl);
    if(regs.ramcl == 0) {
      (co_await write(0x700000 + (regs.rambr << 16) + regs.ramar, regs.ramdr));
    }
  }

  (co_await clock_wait(clocks));

  co_return;
}

// ares/sfc/coprocessor/superfx/timing.cpp
auto Core::syncROMBuffer() -> Task<void> {
  if(regs.romcl) (co_await step(regs.romcl));
  co_return;
}

// ares/sfc/coprocessor/superfx/timing.cpp
auto Core::readROMBuffer() -> Task<n8> {
  (co_await syncROMBuffer());
  co_return regs.romdr;
}

// ares/sfc/coprocessor/superfx/timing.cpp
auto Core::updateROMBuffer() -> void {
  regs.sfr.r = 1;
  regs.romcl = regs.clsr ? 5 : 6;
}

// ares/sfc/coprocessor/superfx/timing.cpp
auto Core::syncRAMBuffer() -> Task<void> {
  if(regs.ramcl) (co_await step(regs.ramcl));
  co_return;
}

// ares/sfc/coprocessor/superfx/timing.cpp
auto Core::readRAMBuffer(n16 address) -> Task<n8> {
  (co_await syncRAMBuffer());
  co_return (co_await read(0x700000 + (regs.rambr << 16) + address));
}

// ares/sfc/coprocessor/superfx/timing.cpp
auto Core::writeRAMBuffer(n16 address, n8 data) -> Task<void> {
  (co_await syncRAMBuffer());
  regs.ramcl = regs.clsr ? 5 : 6;
  regs.ramar = address;
  regs.ramdr = data;
  co_return;
}

// ares/sfc/coprocessor/superfx/io.cpp
auto Core::readIO(n24 address, n8) -> n8 {

  address = 0x3000 | address.bit(0,9);

  if(address >= 0x3100 && address <= 0x32ff) {
    return readCache(address - 0x3100);
  }

  if(address >= 0x3000 && address <= 0x301f) {
    return regs.r[address >> 1 & 15] >> ((address & 1) << 3);
  }

  switch(address) {
  case 0x3030: {
    return regs.sfr >> 0;
  }

  case 0x3031: {
    n8 r = regs.sfr >> 8;
    regs.sfr.irq = 0;
    irq_line = false;
    return r;
  }

  case 0x3034: {
    return regs.pbr;
  }

  case 0x3036: {
    return regs.rombr;
  }

  case 0x303b: {
    return regs.vcr;
  }

  case 0x303c: {
    return regs.rambr;
  }

  case 0x303e: {
    return regs.cbr >> 0;
  }

  case 0x303f: {
    return regs.cbr >> 8;
  }
  }

  return 0x00;
}

// ares/sfc/coprocessor/superfx/io.cpp
auto Core::writeIO(n24 address, n8 data) -> void {

  address = 0x3000 | address.bit(0,9);

  if(address >= 0x3100 && address <= 0x32ff) {
    return writeCache(address - 0x3100, data);
  }

  if(address >= 0x3000 && address <= 0x301f) {
    n4 n = address >> 1 & 15;
    if(!address.bit(0)) {
      regs.r[n] = (regs.r[n] & 0xff00) | data;
    } else {
      regs.r[n] = (data << 8) | (regs.r[n] & 0xff);
    }
    if(n == 14) updateROMBuffer();

    if(address == 0x301f) regs.sfr.g = 1;
    return;
  }

  switch(address) {
  case 0x3030: {
    bool g = regs.sfr.g;
    regs.sfr = (regs.sfr & 0xff00) | (data << 0);
    if(g == 1 && regs.sfr.g == 0) {
      regs.cbr = 0x0000;
      flushCache();
    }
  } break;

  case 0x3031: {
    regs.sfr = (data << 8) | (regs.sfr & 0x00ff);
  } break;

  case 0x3033: {
    regs.bramr = data & 0x01;
  } break;

  case 0x3034: {
    regs.pbr = data & 0x7f;
    flushCache();
  } break;

  case 0x3037: {
    regs.cfgr = data;
  } break;

  case 0x3038: {
    regs.scbr = data;
  } break;

  case 0x3039: {
    regs.clsr = data & 0x01;
  } break;

  case 0x303a: {
    regs.scmr = data;
  } break;
  }
}

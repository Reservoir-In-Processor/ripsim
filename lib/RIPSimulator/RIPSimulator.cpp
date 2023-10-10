#include "RIPSimulator/RIPSimulator.h"
#include <cmath>
#include <set>

namespace {
// BitWidth < 32
int signExtend(const unsigned Imm, unsigned BitWidth) {
  if ((Imm >> (BitWidth - 1)) & 1) {
    return (~0 ^ ((int)(pow(2, BitWidth) - 1))) | (signed)Imm;
  } else {
    return Imm;
  }
}

} // namespace

void PipelineStates::dump() {
  // TODO: dump stall,
  for (int Stage = STAGES::IF; Stage <= STAGES::WB; ++Stage) {
    std::cerr << StageNames[(STAGES)Stage] << ": ";
    if (Insts[Stage] != nullptr) {
      std::cerr << std::hex << "PC=" << PCs[Stage] << " ";
      Insts[Stage]->mprint(std::cerr);
      std::cerr << ", ";
    } else
      std::cerr << "Bubble, ";

    // TODO: dump stage specific info.
    switch (Stage) {
    case STAGES::DE:
      std::cerr << "DERs1Val=" << DERs1Val << ", ";
      std::cerr << "DERs2Val=" << DERs2Val << ", ";
      std::cerr << "DEImmVal=" << DEImmVal << "\n";
      break;
    case STAGES::EX:
      std::cerr << "EXRdVal=" << EXRdVal << "\n";
      break;
    case STAGES::MA:
      std::cerr << "MARdVal=" << MARdVal << "\n";
      break;
    default:
      std::cerr << "\n";
      break;
    }
  }
}

RIPSimulator::RIPSimulator(std::istream &is) : PC(DRAM_BASE), CycleNum(0) {
  // TODO: parse per 2 bytes for compressed instructions
  char Buff[4];
  // starts from DRAM_BASE
  Address P = DRAM_BASE;
  // Load binary into memory
  while (is.read(Buff, 4)) {
    unsigned InstVal = *(reinterpret_cast<unsigned *>(Buff));
    Mem.writeWord(P, InstVal);
    P += 4;
    CodeSize += 4;
  }
}

void RIPSimulator::writeback(GPRegisters &, PipelineStates &) {
  const auto &Inst = PS[STAGES::WB];
  std::string Mnemo = Inst->getMnemo();

  if (Mnemo == "sw") {
    // Instructions without writeback
  } else {
    // Instructions with writeback
    GPRegs.write(Inst->getRd(), PS.getMARdVal());
  }
}

void RIPSimulator::memoryaccess(Memory &, PipelineStates &) {
  const auto &Inst = PS[STAGES::MA];
  const RegVal MARdVal = PS.getEXRdVal();
  RegVal Res = MARdVal;
  std::string Mnemo = Inst->getMnemo();

  if (Mnemo == "sw") {
    Mem.writeWord(MARdVal, PS.getEXRsVal());
  } else if (Mnemo == "lw") {
    Word V = Mem.readWord(PS.getEXRdVal());
    Res = (signed)V;
  }

  PS.setMARdVal(Res);
}

const std::set<std::string> INVALID_EX = {"lbu",  "lhu",   "beq",   "blt",
                                          "bge",  "bltu",  "bgeu",  "jal",
                                          "jalr", "ecall", "ebreak"};

void RIPSimulator::exec(PipelineStates &) {
  const auto &Inst = PS[STAGES::EX];
  // TODO: calc
  RegVal Res = 0;
  std::string Mnemo = Inst->getMnemo();
  // I-type
  if (Mnemo == "addi") {
    Res = PS.getDERs1Val() + PS.getDEImmVal();
  } else if (Mnemo == "slti") {
    Res = (signed)PS.getDERs1Val() < PS.getDEImmVal();
  } else if (Mnemo == "sltiu") {
    Res = (unsigned)PS.getDERs1Val() < (unsigned)PS.getDEImmVal();
  } else if (Mnemo == "xori") {
    Res = (unsigned)PS.getDERs1Val() ^ PS.getDEImmVal();
  } else if (Mnemo == "ori") {
    // FIXME: sext?
    Res = (unsigned)PS.getDERs1Val() | PS.getDEImmVal();
  } else if (Mnemo == "andi") {
    Res = (unsigned)PS.getDERs1Val() & PS.getDEImmVal();
    // } else if (Mnemo == "jalr") {
    //   // FIXME: should addresss calculation be wrapped?
    //   Address CurPC = PC;
    //   PC = (GPRegs[Rs1.to_ulong()] + ImmI) & ~1;
    //   GPRegs.write(Rd.to_ulong(), CurPC + 4);
    // } else if (Mnemo == "lb") {
    //   // FIXME: unsigned to signed safe cast (not implementation defined
    //   way) Byte V = Mem.readByte(GPRegs[Rs1.to_ulong()] + ImmI);
    //   GPRegs.write(Rd.to_ulong(), (signed char)V);
    //   PC += 4;
    // } else if (Mnemo == "lh") {
    //   // FIXME: unsigned to signed safe cast (not implementation defined
    //   way) HalfWord V = Mem.readHalfWord(GPRegs[Rs1.to_ulong()] + ImmI);
    //   GPRegs.write(Rd.to_ulong(), (signed short)V);
    //   PC += 4;
  } else if (Mnemo == "lw") {
    // FIXME: unsigned to signed safe cast (not implementation defined way)
    Res = PS.getDERs1Val() + PS.getDEImmVal();
    // } else if (Mnemo == "lbu") {
    //   // FIXME: unsigned to signed safe cast (not implementation defined way)
    //   Res = PS.getDERs1Val() + PS.getDEImmVal();
    // } else if (Mnemo == "lhu") {
    //   Res = PS.getDERs1Val() + PS.getDEImmVal();

  } else if (Mnemo == "slli") { // FIXME: shamt
    // FIXME: sext?
    Res = (unsigned)PS.getDERs1Val() << PS.getDEImmVal();
  } else if (Mnemo == "srli") {
    Res = (unsigned)PS.getDERs1Val() >> PS.getDEImmVal();
  } else if (Mnemo == "srai") {
    Res = (signed)PS.getDERs1Val() >> PS.getDEImmVal();
  } else if (Mnemo == "fence") {
    // FIXME: currently expected to be nop
  } else if (Mnemo == "fence.i") {
    // FIXME: currently expected to be nop
    // } else if (Mnemo == "csrrw") {
    //   CSRAddress CA = Imm.to_ulong() & 0xfff;
    //   CSRVal CV = States.read(CA);
    //   States.write(CA, GPRegs[Rs1.to_ulong()]);
    //   GPRegs.write(Rd.to_ulong(), CV);
    //   PC += 4;
    // } else if (Mnemo == "csrrs") {
    //   CSRAddress CA = Imm.to_ulong() & 0xfff;
    //   CSRVal CV = States.read(CA);
    //   States.write(CA, GPRegs[Rs1.to_ulong()] | CV);
    //   GPRegs.write(Rd.to_ulong(), CV);
    //   PC += 4;
    // } else if (Mnemo == "csrrc") {
    //   CSRAddress CA = Imm.to_ulong() & 0xfff;
    //   CSRVal CV = States.read(CA);
    //   States.write(CA, GPRegs[Rs1.to_ulong()] & !CV);
    //   GPRegs.write(Rd.to_ulong(), CV);
    //   PC += 4;
    // } else if (Mnemo == "csrrwi") {
    //   CSRAddress CA = Imm.to_ulong() & 0xfff;
    //   CSRVal CV = States.read(CA);
    //   CSRVal ZImm = Rs1.to_ulong();
    //   States.write(CA, ZImm);
    //   GPRegs.write(Rd.to_ulong(), CV);
    //   PC += 4;
    // } else if (Mnemo == "csrrsi") {
    //   CSRAddress CA = Imm.to_ulong() & 0xfff;
    //   CSRVal CV = States.read(CA);
    //   CSRVal ZImm = Rs1.to_ulong();
    //   States.write(CA, CV | ZImm);
    //   GPRegs.write(Rd.to_ulong(), CV);
    //   PC += 4;
    // } else if (Mnemo == "csrrci") {
    //   CSRAddress CA = Imm.to_ulong() & 0xfff;
    //   CSRVal CV = States.read(CA);
    //   CSRVal ZImm = Rs1.to_ulong();
    //   States.write(CA, CV & !ZImm);
    //   GPRegs.write(Rd.to_ulong(), CV);
    //   PC += 4;
    // } else if (Mnemo == "ecall") {
    //   if (Mode == ModeKind::User) {
    //     return Exception::EnvironmentCallFromUMode;
    //   } else if (Mode == ModeKind::Supervisor) {
    //     return Exception::EnvironmentCallFromSMode;
    //   } else if (Mode == ModeKind::Machine) {
    //     return Exception::EnvironmentCallFromMMode;
    //   } else {
    //     // FIXME: is this illegal inst?
    //     return Exception::IllegalInstruction;
    //   }
    // } else if (Mnemo == "ebreak") {
    //   return Exception::Breakpoint;

    // R-type
  } else if (Mnemo == "add") {
    Res = PS.getDERs1Val() + PS.getDERs2Val();
  } else if (Mnemo == "sub") {
    Res = PS.getDERs1Val() - PS.getDERs2Val();
  } else if (Mnemo == "sll") {
    Res = PS.getDERs1Val() << (PS.getDERs2Val() & 0b11111);
  } else if (Mnemo == "slt") {
    Res = PS.getDERs1Val() < PS.getDERs2Val();
  } else if (Mnemo == "sltu") {
    Res = (unsigned)PS.getDERs1Val() < (unsigned)PS.getDERs2Val();
  } else if (Mnemo == "xor") {
    Res = PS.getDERs1Val() ^ PS.getDERs2Val();
  } else if (Mnemo == "srl") {
    Res = (unsigned)PS.getDERs1Val() >> (PS.getDERs2Val() & 0b11111);
  } else if (Mnemo == "sra") {
    Res = (signed)PS.getDERs1Val() >> (signed)(PS.getDERs2Val() & 0b11111);
  } else if (Mnemo == "or") {
    Res = PS.getDERs1Val() | PS.getDERs2Val();
  } else if (Mnemo == "and") {
    Res = PS.getDERs1Val() & PS.getDERs2Val();
  } else if (Mnemo == "mul") {
    Res = ((signed long long)PS.getDERs1Val() *
           (signed long long)PS.getDERs2Val()) &
          0xffffffff;
  } else if (Mnemo == "mulh") {
    Res = ((signed long long)PS.getDERs1Val() *
           (signed long long)PS.getDERs2Val()) >>
          32;
  } else if (Mnemo == "mulhsu") {
    Res = ((signed long long)PS.getDERs1Val() *
           (unsigned long long)(unsigned int)PS.getDERs2Val()) >>
          32;
  } else if (Mnemo == "mulhu") {
    Res = ((unsigned long long)(unsigned int)PS.getDERs1Val() *
           (unsigned long long)(unsigned int)PS.getDERs2Val()) >>
          32;
  } else if (Mnemo == "div") {
    // FIXME: set DV zero register?
    RegVal Divisor = PS.getDERs2Val(), Dividend = PS.getDERs1Val();
    if (Divisor == 0) {
      Res = -1;
    } else if (Dividend == std::numeric_limits<std::int32_t>::min() &&
               Divisor == -1) {
      Res = std::numeric_limits<std::int32_t>::min();
    } else {
      Res = Dividend / Divisor;
    }
  } else if (Mnemo == "divu") {
    RegVal Divisor = PS.getDERs2Val(), Dividend = PS.getDERs1Val();
    if (Divisor == 0) {
      Res = std::numeric_limits<std::uint32_t>::max();
    } else {
      Res = (unsigned int)Dividend / (unsigned int)Divisor;
    }
  } else if (Mnemo == "rem") {
    RegVal Divisor = PS.getDERs2Val(), Dividend = PS.getDERs1Val();
    if (Divisor == 0) {
      Res = Dividend;
    } else if (Dividend == std::numeric_limits<std::int32_t>::min() &&
               Divisor == -1) {
      Res = 0;
    } else {
      Res = PS.getDERs1Val() % PS.getDERs2Val();
    }
  } else if (Mnemo == "remu") {
    RegVal Divisor = PS.getDERs2Val(), Dividend = PS.getDERs1Val();
    if (Divisor == 0) {
      Res = Dividend;
    } else {
      Res = (unsigned int)PS.getDERs1Val() % (unsigned int)PS.getDERs2Val();
    }

    // J-type
  } else if (Mnemo == "jal") {
    Res = PS.getPCs(EX) + 4;
    Address nextPC = PS.getPCs(EX) + signExtend(PS.getDEImmVal(), 20);
    PS.setBranchPC(nextPC);

    // B-type
  } else if (Mnemo == "beq") {
    if (PS.getDERs1Val() == PS.getDERs2Val()) {
      Address nextPC = PS.getPCs(EX) + PS.getDEImmVal();
      PS.setBranchPC(nextPC);
    } else {
      // pass
    }
  } else if (Mnemo == "bne") {
    if (PS.getDERs1Val() != PS.getDERs2Val()) {
      Address nextPC = PS.getPCs(EX) + PS.getDEImmVal();
      PS.setBranchPC(nextPC);
    } else {
      // pass
    }
  } else if (Mnemo == "blt") {
    if (PS.getDERs1Val() < PS.getDERs2Val()) {
      Address nextPC = PS.getPCs(EX) + PS.getDEImmVal();
      PS.setBranchPC(nextPC);
    } else {
      // pass
    }
  } else if (Mnemo == "bge") {
    if (PS.getDERs1Val() >= PS.getDERs2Val()) {
      Address nextPC = PS.getPCs(EX) + PS.getDEImmVal();
      PS.setBranchPC(nextPC);
    } else {
    }
  } else if (Mnemo == "bltu") {
    if ((unsigned)PS.getDERs1Val() < (unsigned)PS.getDERs2Val()) {
      Address nextPC = PS.getPCs(EX) + PS.getDEImmVal();
      PS.setBranchPC(nextPC);

    } else {
    }
  } else if (Mnemo == "bgeu") {
    if ((unsigned)PS.getDERs1Val() >= (unsigned)PS.getDERs2Val()) {
      Address nextPC = PS.getPCs(EX) + PS.getDEImmVal();
      PS.setBranchPC(nextPC);
    } else {
    }
    // S-type
    // } else if (Mnemo == "sb") {
    //   // FIXME: wrap add?
    //   Res = PS.getDERs1Val() + PS.getDEImmVal();
    // } else if (Mnemo == "sh") {
    //   // FIXME: wrap add?
    //   Res = PS.getDERs1Val() + PS.getDEImmVal();
  } else if (Mnemo == "sw") {
    Res = PS.getDERs1Val() + PS.getDEImmVal();
    RegVal Res2 = PS.getDERs2Val();
    PS.setEXRsVal(Res2);

  } else {
    assert(false && "unimplemented!");
  }

  if (INVALID_EX.find(Inst->getMnemo()) != INVALID_EX.end()) {
    // FIXME: invalide itself, is this right?
    PS.setInvalid(EX);
    PS.setInvalid(DE);
    PS.setInvalid(IF);
    // TODO: reset PC
  }
  // TODO: implemnet Branched PC
  if (false)
    PS.setBranchPC(PC + 4);

  PS.setEXRdVal(Res);
}

// register access shuold be done in this phase, exec shuoldn't access
// GPRegs directly.
void RIPSimulator::decode(GPRegisters &, PipelineStates &) {
  // FIXME: PS indexing seems not consistent(it's not only instructions)
  const auto &Inst = PS[STAGES::DE];

  // TODO:
  // FIXME: this is incorrect, because U-type and J-type instr's Rs1 or Rs2
  // is a part of immediate. Check if the inst is one of those.

  // Register access on Rs1
  if (PS[STAGES::MA] && Inst->getRs1() == PS[STAGES::MA]->getRd()) {
    // MA forward
    PS.setDERs1Val(PS.getMARdVal());
    std::cerr << "Forwarding from MA\n";
  } else if (PS[STAGES::EX] && Inst->getRs1() == PS[STAGES::EX]->getRd()) {
    // EX forward
    PS.setDERs1Val(PS.getEXRdVal());
    std::cerr << "Forwarding from EX\n";
  } else {
    PS.setDERs1Val(GPRegs[Inst->getRs1()]);
  }

  // Register access on Rs2
  if (PS[STAGES::MA] && Inst->getRs2() == PS[STAGES::MA]->getRd()) {
    PS.setDERs2Val(PS.getMARdVal());
    std::cerr << "Forwarding from MA\n";
  } else if (PS[STAGES::EX] && Inst->getRs2() == PS[STAGES::EX]->getRd()) {
    PS.setDERs2Val(PS.getEXRdVal());
    std::cerr << "Forwarding from EX\n";
  } else {
    PS.setDERs2Val(GPRegs[Inst->getRs2()]);
  }

  // TODO: more variants
  if (ITypeKinds.find(Inst->getMnemo()) != ITypeKinds.end()) {
    PS.setDEImmVal(signExtend(Inst->getImm(), 12));

  } else if (STypeKinds.find(Inst->getMnemo()) != STypeKinds.end()) {
    // FIXME: Need to be tested
    unsigned Offset =
        (Inst->getVal() & 0xfe000000) >> 20 | ((Inst->getVal() >> 7) & 0x1f);
    PS.setDEImmVal(signExtend(Offset, 12));

  } else if (RTypeKinds.find(Inst->getMnemo()) != RTypeKinds.end()) {
    // FIXME: pass

  } else if (JTypeKinds.find(Inst->getMnemo()) != JTypeKinds.end()) {
    unsigned Imm =
        ((Inst->getVal() & 0x80000000) >> 11) | (Inst->getVal() & 0xff000) |
        ((Inst->getVal() >> 9) & 0x800) | ((Inst->getVal() >> 20) & 0x7fe);
    PS.setDEImmVal(Imm);

  } else if (BTypeKinds.find(Inst->getMnemo()) != BTypeKinds.end()) {
    unsigned Imm =
        (Inst->getVal() > 0x80000000) >> 19 | ((Inst->getVal() & 0x80) << 4) |
        ((Inst->getVal() >> 20) & 0x7e0) | ((Inst->getVal() >> 7) & 0x1e);

    PS.setDEImmVal(Imm);
  } else {
    assert(false && "unimplemented!");
  }
  // TODO: stall 1 cycle if the inst is load;
}
void RIPSimulator::fetch(Memory &, PipelineStates &) {
  // const auto &Inst = PS[STAGES::IF];
  // FIXME: how and when can I change PC?
}

void RIPSimulator::runFromDRAMBASE() {
  PC = DRAM_BASE;

  while (true) {
    auto InstPtr = Dec.decode(Mem.readWord(PC));
    PS.pushPC(PC);
    if (InstPtr) {
      PC += 4;
    }
    PS.push(std::move(InstPtr));
    // FIXME: might this be wrong if branch prediction happens.
    CycleNum++;
    if (PS[STAGES::WB] != nullptr)
      writeback(GPRegs, PS);
    if (PS[STAGES::MA] != nullptr)
      memoryaccess(Mem, PS);
    if (PS[STAGES::EX] != nullptr)
      exec(PS);
    if (PS[STAGES::DE] != nullptr)
      decode(GPRegs, PS);
    if (PS[STAGES::IF] != nullptr)
      fetch(Mem, PS);
#ifdef DEBUG
    PS.dump();
    dumpGPRegs();
#endif
    if (PS.isEmpty()) {
      break;
    }
    // TODO:
    if (auto NextPC = PS.takeBranchPC()) {
      PC = *NextPC;
    }
  }
}

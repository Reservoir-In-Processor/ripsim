#include <RIPSimulator/RIPSimulator.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

enum BranchPredKind {
  No,
  One,
  Two,
  Gshare,
  // TODO: add interactive mode
  Interactive,
};
class Options {
private:
  std::string FileName;

  BranchPredKind BPKind;

  // cycle level interactive mode
  bool Interactive;

  // statistics dump
  bool Statistics;

  // statistics dump
  Address DRAMSize;

  // address where program starts.
  std::optional<Address> StartAddress;

  // address where program ends..
  std::optional<Address> EndAddress;

public:
  Options()
      : BPKind(No), Interactive(false), Statistics(false), DRAMSize(1 << 10),
        StartAddress(std::nullopt), EndAddress(std::nullopt) {}

  // return true if succeed.
  bool parse(int argc, char **argv) {
    if (argc < 2) {
      printHelp();
      return false;
    }

    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg[0] != '-') {
        FileName = arg;
      } else if (arg.substr(0, 3) == "-b=") {
        std::string BPStr = arg.substr(3);
        if (BPStr != "no" && BPStr != "onebit" && BPStr != "twobit" &&
            BPStr != "gshare" && BPStr != "interactive") {
          std::cerr << "Invalid option for -b. Allowed options: no, onebit, "
                       "twobit, gshare and interactive.\n";
          return false;
        }
      } else if (arg == "-i") {
        Interactive = true;
      } else if (arg.substr(0, 12) == "--dram-size=") {
        DRAMSize = std::stoll(arg.substr(12));
      } else if (arg.substr(0, 18) == "--start-address=0x") {
        StartAddress = std::stoll(arg.substr(18), nullptr, 16);
      } else if (arg.substr(0, 16) == "--end-address=0x") {
        EndAddress = std::stoll(arg.substr(16), nullptr, 16);
        std::cerr << "EndAddress" << *EndAddress << "\n";
      } else if (arg == "--stats") {
        Statistics = true;
      } else {
        std::cerr << "Unknown option: " << arg << "\n";
        return false;
      }
    }

    return !FileName.empty();
  }

  void printHelp() {
    std::cerr
        << "Usage: rip-sim"
        << " <baremetal binary file name> "
           "-b=<onebit/twobit/gshare/interactive> [--dram-size=N] [--stats]\n"
        << "-b=<option> : Set branch prediction type (no, onebit, twobit, "
           "gshare and interactive)\n"
        << "--dram-size=N : Set DRAM size in kilobytes (N)\n"
        << "--stats : print statistics\n"
        << "-i : Additional flag\n";
  }

  Options(const Options &) = delete;
  Options &operator=(const Options &) = delete;

  inline const std::string &getFileName() { return FileName; }

  inline const BranchPredKind &getBPKind() { return BPKind; }

  inline const bool &getInteractive() { return Interactive; }

  inline const bool &getStatistics() { return Statistics; }

  inline const Address &getDRAMSize() { return DRAMSize; }

  inline const std::optional<Address> &getStartAddress() {
    return StartAddress;
  }

  inline const std::optional<Address> &getEndAddress() { return EndAddress; }
};

int main(int argc, char **argv) {
  Options Ops;
  if (!Ops.parse(argc, argv)) {
    return 1;
  }
  std::string FileName = Ops.getFileName();
  std::string BaseNoExt = FileName.substr(0, FileName.find_last_of('.'));
  auto Files = std::ifstream(FileName);

  std::unique_ptr<BranchPredictor> BP = nullptr;
  if (Ops.getBPKind() == BranchPredKind::No) {
    BP = nullptr;
  } else if (Ops.getBPKind() == BranchPredKind::One) {
    BP = std::make_unique<OneBitBranchPredictor>();
  } else if (Ops.getBPKind() == BranchPredKind::Two) {
    BP = std::make_unique<TwoBitBranchPredictor>();
  } else if (Ops.getBPKind() == BranchPredKind::Gshare) {
    BP = std::make_unique<GshareBranchPredictor>();
  } else {
    assert(false && "unreachable!");
  }

  std::unique_ptr<Statistics> Stats = nullptr;
  if (Ops.getStatistics()) {
    Stats = std::make_unique<Statistics>();
  }
  DEBUG_ONLY(Stats = std::make_unique<Statistics>(););

  RIPSimulator RipSim(Files, std::move(BP), Ops.getDRAMSize(),
                      std::move(Stats));

  if (Ops.getInteractive())
    RipSim.runInteractively(Ops.getStartAddress(), Ops.getEndAddress());
  else
    RipSim.run(Ops.getStartAddress(), Ops.getEndAddress());

  return 0;
}

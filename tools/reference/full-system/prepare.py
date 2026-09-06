"""Generate resources and observation hooks without editing pinned Ares sources."""
from pathlib import Path
import re
import subprocess
import sys

REVISION = "0aafd85789215e84e1e43415c07d4c88461b7899"
source, generated = map(lambda p: Path(p).resolve(), sys.argv[1:3])
revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
changes = subprocess.check_output(
    ["git", "-C", str(source), "status", "--porcelain", "--untracked-files=all"], text=True
)
if revision != REVISION or changes:
    raise RuntimeError("The full-system reference requires unmodified Ares v148: " + REVISION)
if generated == source or source in generated.parents:
    raise RuntimeError("Generated files must be outside the reference checkout")
generated.mkdir(parents=True, exist_ok=True)
resources = generated / "ares/resource"
resources.mkdir(parents=True, exist_ok=True)

def save(path, text):
    if not path.exists() or path.read_text() != text:
        path.write_text(text)

header, implementation, stack = [], ['#include <ares/resource/resource.hpp>'], []
root = source / "ares/ares/resource"
for line in (root / "resource.bml").read_text().splitlines():
    if not line.strip():
        continue
    depth = (len(line) - len(line.lstrip())) // 2
    while len(stack) > depth:
        header.append("}")
        implementation.append("}")
        stack.pop()
    if match := re.fullmatch(r"\s*namespace name=(\w+)", line):
        name = match[1]
        header.append(f"namespace {name} {{")
        implementation.append(f"namespace {name} {{")
        stack.append(name)
    elif match := re.fullmatch(r"\s*binary name=(\w+) file=(.+)", line):
        name, filename = match.groups()
        data = (root / filename).read_bytes()
        header.append(f"extern const unsigned char {name}[{len(data)}];")
        implementation.append(
            f"const unsigned char {name}[{len(data)}] = {{" + ",".join(map(str, data)) + "};"
        )
    else:
        raise ValueError("Unrecognized resource declaration: " + line)
while stack:
    header.append("}")
    implementation.append("}")
    stack.pop()
save(resources / "resource.hpp", "\n".join(header) + "\n")
save(resources / "resource.cpp", "\n".join(implementation) + "\n")

metadata = (source / "ares/ares/ares.cpp.in").read_text()
for key, value in {
    "ARES_NAME": "ares",
    "ARES_VERSION": "148 (Star Fox development reference)",
    "ARES_LEGAL_COPYRIGHT_SHORT": "2004-2025 ares team, Near",
    "ARES_WEBSITE": "ares-emu.net",
}.items():
    metadata = metadata.replace("@" + key + "@", value)
if re.search(r"@\w+@", metadata):
    raise RuntimeError("Unresolved Ares metadata")
save(generated / "ares.cpp", metadata)

def observe(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError("Observation hook no longer matches pinned source: " + old)
    return text.replace(old, new)

cpu = (source / "ares/sfc/cpu/cpu.cpp").read_text()
cpu = re.sub(r'#include "([^"]+)"', r'#include <sfc/cpu/\1>', cpu)
cpu = observe(cpu, "namespace ares::SuperFamicom {",
              'extern "C" void sfc_audit_cpu(unsigned, unsigned);\n'
              'extern "C" void sfc_audit_cpu_step(unsigned, unsigned, unsigned);\n'
              'extern "C" void sfc_audit_cpu_step_end(unsigned, unsigned);\n'
              'extern "C" void sfc_audit_timing_read(unsigned, unsigned);\n'
              'extern "C" void sfc_audit_timing_write(unsigned, unsigned);\n'
              'namespace ares::SuperFamicom {\nstatic unsigned audit_refresh_depth = 0;\n'
              'static unsigned audit_dma_depth = 0;')
cpu = observe(cpu, '#include <sfc/cpu/timing.cpp>', '#include "cpu-timing.cpp"')
cpu = observe(cpu, '#include <sfc/cpu/memory.cpp>', '#include "cpu-memory.cpp"')
cpu = observe(cpu, "    debugger.instruction();",
              "    sfc_audit_cpu(r.pc.d, counter.cpu);\n    debugger.instruction();")
save(generated / "cpu.cpp", cpu)

memory = (source / "ares/sfc/cpu/memory.cpp").read_text()
memory = observe(memory, "  auto data = bus.read(address, r.mdr);",
    "  auto data = bus.read(address, r.mdr);\n  sfc_audit_timing_read(address, data);")
memory = observe(memory, "  bus.write(address, r.mdr = data);",
    "  bus.write(address, r.mdr = data);\n  sfc_audit_timing_write(address, data);")
save(generated / "cpu-memory.cpp", memory)

# Account for every existing step without adding or removing emulated clocks.
# DMA-active includes arbitration/alignment and any CPU cycle while that flag
# is set; it is deliberately not called DMA payload time. Refresh has priority
# so its recursively stepped clocks are counted once, in a disjoint category.
timing = (source / "ares/sfc/cpu/timing.cpp").read_text()
timing = observe(timing, "auto CPU::step(u32 clocks) -> void {",
    "auto CPU::step(u32 clocks) -> void {\n"
    "  sfc_audit_cpu_step(clocks, audit_refresh_depth ? 2 : status.dmaActive ? 1 : 0,\n"
    "    audit_refresh_depth ? 2 : audit_dma_depth ? 1 : 0);")
timing = observe(timing, "alwaysinline auto CPU::dmaEdge() -> void {",
    "alwaysinline auto CPU::dmaEdge() -> void {\n"
    "  struct AuditDmaScope {\n"
    "    AuditDmaScope() { ++audit_dma_depth; }\n"
    "    ~AuditDmaScope() { --audit_dma_depth; }\n"
    "  } audit_dma_scope;")
timing = observe(timing,
    "  if(!status.dramRefresh && hcounter() >= status.dramRefreshPosition) {",
    "  if(!status.dramRefresh && hcounter() >= status.dramRefreshPosition) {\n"
    "    ++audit_refresh_depth;")
timing = observe(timing,
    "  }\n\n  if(!status.hdmaSetupTriggered",
    "    --audit_refresh_depth;\n  }\n\n  if(!status.hdmaSetupTriggered")
timing = observe(timing, "  Thread::step(clocks);",
    "  if(!audit_refresh_depth) sfc_audit_cpu_step_end(counter.cpu, status.dramRefreshPosition);\n  Thread::step(clocks);")
save(generated / "cpu-timing.cpp", timing)

# Power-on WRAM is deliberately random in Ares. Keep the stock low-entropy
# pattern generator but seed it reproducibly for this diagnostic fixture.
system = (source / "ares/sfc/system/system.cpp").read_text()
system = re.sub(r'#include "([^"]+)"', r'#include <sfc/system/\1>', system)
system = observe(system, "  random.entropy(Random::Entropy::Low);",
                 "  random.entropy(Random::Entropy::Low);\n  random.seed(Random::Default);")
# The standalone MinGW link registers global destructors in a different
# order from the desktop front end. Thread destructors call scheduler.remove;
# the scheduler must therefore be initialized first and destroyed last.
system = observe(system, "Scheduler scheduler;",
                 "Scheduler scheduler __attribute__((init_priority(101)));")
save(generated / "system.cpp", system)

# The GSU hooks record launches, STOP, and elapsed coprocessor clocks. They
# leave the stock instruction, memory, refresh, DMA and scheduling bodies intact.
# The I/O callback preserves writes by default; explicit diagnostic policies
# may override only the two clock/multiply selection bits (see README).
gsu_root = source / "ares/sfc/coprocessor/superfx"
io = (gsu_root / "io.cpp").read_text()
io = observe(io, "auto SuperFX::writeIO(n24 address, n8 data) -> void {\n  cpu.synchronize(*this);\n  address = 0x3000 | address.bit(0,9);",
             "auto SuperFX::writeIO(n24 address, n8 data) -> void {\n  cpu.synchronize(*this);\n  address = 0x3000 | address.bit(0,9);\n"
             "  data = sfc_audit_gsu_write(address, data);")
launch = "sfc_audit_gsu_start((regs.pbr << 16) | regs.r[15], regs.clsr, regs.cfgr, regs.scmr);"
io = observe(io, "    if(address == 0x301f) regs.sfr.g = 1;",
             "    if(address == 0x301f) { regs.sfr.g = 1; " + launch + " }")
io = observe(io, "    if(g == 1 && regs.sfr.g == 0) {",
             "    if(g == 0 && regs.sfr.g == 1) { " + launch + " }\n"
             "    if(g == 1 && regs.sfr.g == 0) {")
save(generated / "gsu-io.cpp", io)
timing = (gsu_root / "timing.cpp").read_text()
timing = observe(timing, "  Thread::step(clocks);",
                 "  sfc_audit_gsu_step(clocks);\n  Thread::step(clocks);")
save(generated / "gsu-timing.cpp", timing)
gsu = (gsu_root / "superfx.cpp").read_text()
gsu = re.sub(r'#include "([^"]+)"', r'#include <sfc/coprocessor/superfx/\1>', gsu)
gsu = gsu.replace("#include <sfc/coprocessor/superfx/io.cpp>", '#include "gsu-io.cpp"')
gsu = gsu.replace("#include <sfc/coprocessor/superfx/timing.cpp>", '#include "gsu-timing.cpp"')
gsu = observe(gsu, "  instruction(opcode);",
              "  instruction(opcode);\n  if(!regs.sfr.g) sfc_audit_gsu_stop();")
save(generated / "gsu.cpp", gsu)
coprocessor = (source / "ares/sfc/coprocessor/coprocessor.cpp").read_text()
coprocessor = re.sub(r'#include "([^"]+)"', r'#include <sfc/coprocessor/\1>', coprocessor)
coprocessor = coprocessor.replace("#include <sfc/coprocessor/superfx/superfx.cpp>", '#include "gsu.cpp"')
coprocessor = observe(coprocessor, "namespace ares::SuperFamicom {", '''extern "C" void sfc_audit_gsu_start(unsigned, unsigned, unsigned, unsigned);
extern "C" unsigned sfc_audit_gsu_write(unsigned, unsigned);
extern "C" void sfc_audit_gsu_step(unsigned);
extern "C" void sfc_audit_gsu_stop();
namespace ares::SuperFamicom {''')
save(generated / "coprocessor.cpp", coprocessor)
print("Prepared pinned Ares resources and observation hooks outside its checkout.")

# Critical Review of the Proposed Vole Machine Design

## 1. Missing Responsibilities, Classes, Attributes, Methods, Relationships

**Genuinely missing:**

- **A cycle-result value.** `CPU::step()` is listed as returning nothing, but `VoleMachine::run()` needs to know whether to continue or halt. The executor returns an `ExecResult`, but that type was never defined, and there is no defined channel from executor → CPU → machine for "halt" vs "continue" vs "fault". This is a real gap.
- **`ExecResult` type** referenced in the PlantUML (`execute(...) : ExecResult`) but never declared. Missing.
- **No explicit `MachineFault` include path.** It appears in the executor's dependencies but is not owned or produced by anything in the diagram — it is just floating. It should be clearly thrown by `Decoder` (invalid opcode), `Memory`/`RegisterFile` (out-of-range) and caught in `VoleMachine::run`.
- **Loader must enforce capacity.** 256 bytes hold 128 instructions; the loader needs an explicit check that the program fits, plus a defined start address. Missing from the loader's method set.
- **No getter/observability for PC and IR.** A simulator (and tests) need `getPC()`, `getIR()`. `VoleMachine` only exposes registers and memory.
- **No reset contract for `CPU` referencing components.** `CPU::reset()` is listed but the PC/IR reset and the relationship to `Memory`/`RegisterFile` reset ordering is unspecified; in practice `VoleMachine::reset()` must reset all of them, and that ordering is a real responsibility (e.g., output clear should not be undone by memory reset).
- **No state for the last executed instruction** — useful for debugging, optional.

**Ambiguous / underspecified but present:**

- `Memory::read(0xFF)` policy is mentioned as "document it" — that is a missing decision, not a missing class.
- `CPU` "owns" decoder and executor by value in the diagram (`*--`), yet the text says "or holds references." Ownership needs to be decided.
- `VoleMachine` holds `OutputDevice` by value in the diagram but `Memory` holds `OutputDevice*`. Ownership inconsistency — the diagram uses `*--` for `VoleMachine` and association for `Memory`, which is fine if `VoleMachine` owns the device, but this should be stated explicitly.

**Relationships that should be added:**

- `ProgramLoader ..> LoadResult` (returns it).
- `VoleMachine ..> MachineFault` (catches it).
- `InstructionDecoder ..> MachineFault` (throws on invalid opcode).
- `InstructionExecutor ..> CPU` (needs PC mutation for jump and halt signalling) — the executor's handler list implies it but the diagram shows no dependency to `CPU` except through the method signature.

---

## 2. Unnecessary Classes or Complexity

Honestly, the original design is already fairly lean. Candidates for removal/simplification:

- **`InstructionExecutor` as a separate class** is defensible but is the weakest seam. The executor is stateless and its only "knowledge" is opcode semantics; it could be a set of free functions or a `static` method family, or folded into `CPU`. Keeping it as a class is fine if you value testability, but it is not strictly necessary.
- **`MemoryBufferOutput`** is useful for tests but is not part of the simulator's architecture per se. Keep it only if tests are in scope.
- **`VoleMachine` facade**: arguably `CPU` + components could be assembled by the test harness directly. But the facade earns its keep (lifecycle, reset ordering, run loop, error catching), so keep it.
- **`DecodedInstruction`** as a struct — correct call; do **not** upgrade to a class hierarchy.
- The **`getRegister`/`getMemory`** pair on `VoleMachine` is a leaky convenience; acceptable for a simulator but worth noting.

No class is *outright* unnecessary. The design is close to the right size.

---

## 3. Checking Against the Specification

| Spec item | Design coverage | Verdict |
|---|---|---|
| 16 × 8-bit registers R0–RF | `RegisterFile` with `array<uint8_t,16>` | ✔ Covered; must validate index 0–15. |
| 256 byte-addressable memory 00–FF | `Memory` with `array<uint8_t,256>` | ✔ Covered; must validate 0–255. |
| 4 hex digits = 2 bytes per instruction | `CPU::fetch` combines two bytes; loader writes two bytes per line | ✔ Covered, **but** the design never explicitly states the endianness convention (high byte first). Should be pinned down. |
| PC and IR | `CPU` attributes `pc`, `ir` | ✔ Covered; **but** PC unit (byte vs. instruction index) is left open. Must be fixed: PC counts **bytes**, increments by 2 per instruction. |
| Fetch-decode-execute cycle | `CPU::step` = fetch→decode→execute | ✔ Covered. |
| Conditional jump `BRXY` (jump if R == R0) | `opJumpEq` sets PC = XY | ✔ Semantics covered, **but** the target XY is a byte address; must be consistent with the byte-PC convention. |
| Halt `C000` | `opHalt` returns halt; CPU sets halted | ✔ Covered, **but** the channel from executor to CPU was undefined (gap noted in §1). |
| Screen at FF | `Memory::write(0xFF)` forwards to `OutputDevice` | ✔ Covered. **Read policy at FF is undefined** — must be documented (I recommend: reads return last written char). |
| Loader ignores blank lines / `;` comments | `ProgramLoader::parseLine` | ✔ Covered; **but** trailing-comment handling ("instruction ; comment") should be explicit. |
| Instruction set 1,2,3,4,5,6,7,8,9,A,B,C | Executor handlers cover all | ✔ Covered; `6RST` (float) still needs a documented numeric convention; `AR0X` rotate-by-X needs `%8` rule. |

**Conclusion:** spec coverage is good; the gaps are *unstated conventions* (PC units, endianness, FF read, float encoding, rotate modulus) and *the halt signalling channel*, not missing hardware concepts.

---

## 4. Advantages and Disadvantages of Major Decisions

**Facade (`VoleMachine`)**
- *Pro:* single entry point, clean reset ordering, easy to test/embed.
- *Con:* adds a layer that mostly forwards; can grow into a god-object if not disciplined.

**Separate `Decoder` and `Executor`**
- *Pro:* each is testable in isolation; opcode additions touch one place.
- *Con:* two extra collaborators inside `CPU`; the executor needs a writable `CPU` reference for PC changes and halt signalling, which slightly weakens encapsulation. For a machine this small, folding execution into `CPU` is a legitimate simplification.

**Tagged struct `DecodedInstruction` instead of a hierarchy**
- *Pro:* simple, cache-friendly, no virtual dispatch for a uniform operand shape.
- *Con:* handlers must switch on `Opcode`; no compile-time exhaustiveness unless you use a `switch` with no `default` plus warnings.

**Value-held `Memory` and `RegisterFile` arrays, no per-cell/per-register classes**
- *Pro:* minimal ceremony, easy reset/iterate, bounds-checked accessors preserve encapsulation.
- *Con:* if you ever need per-register observers or per-cell metadata, you'd refactor. Not needed here.

**`OutputDevice` interface**
- *Pro:* decouples screen mapping from memory; enables headless tests and alternative displays.
- *Con:* one more abstraction; only two implementations exist, so it is borderline but justified by the FF seam.

**Loader as separate class with `LoadResult`**
- *Pro:* orthogonal concern; line-numbered errors; atomic load feasible.
- *Con:* slightly more types; but the alternative (parsing inside `VoleMachine`) muddies responsibilities.

**`MachineFault` as exception type**
- *Pro:* clean separation of runtime faults from load-time errors.
- *Con:* exceptions for "invalid opcode" are fine here, but must be caught in `run()` or a bad program aborts the process.

---

## 5. Specific Improvements

1. **Define `ExecResult`** explicitly: `enum class ExecStatus { Continue, Halt }` plus an optional fault reason. `CPU::step()` returns `ExecStatus`; `VoleMachine::run()` loops while `Continue`.
2. **Pin down PC as a byte address**, incremented by 2 on fetch; document that `XY` operands in jump/load/store are byte addresses (consistent with memory addressing).
3. **Pin down instruction endianness:** first byte = high nibble pair, second = low pair; loader and fetch must agree.
4. **Document FF read policy** (recommend: last written byte, or 0 before first write) in the `Memory` class comment and in `read()`.
5. **Document float-add (`6RST`) convention** — e.g., treat the two bytes as a documented 8-bit mini-float, or a fixed-point pair; otherwise `6RST` is untestable.
6. **Document rotate (`AR0X`)**: rotate right by `X mod 8`.
7. **Add `CPU::getPC()`/`getIR()`** and expose them via `VoleMachine` for testing/debugging.
8. **Make `ProgramLoader` capacity-aware:** reject programs longer than 128 instructions; define start address (00).
9. **Clarify ownership:** `VoleMachine` owns `OutputDevice` by value; `Memory` holds a non-owning `OutputDevice*`; `CPU` holds non-owning pointers to `Memory`/`RegisterFile`. State this explicitly, since the original diagram mixed `*--` and `-->` without comment.
10. **Define reset ordering** in `VoleMachine::reset()`: registers → memory → output → CPU (pc=0, ir=0) → halted=false. Output cleared last so no stale screen byte.
11. **Catch `MachineFault` in `VoleMachine::run()`**, print a diagnostic, halt cleanly, and preserve state for inspection.
12. **Add `LoadResult` to diagram** and connect loader → result.
13. **Add `VoleMachine ..> MachineFault`** (catches) and `Decoder ..> MachineFault` (throws) to the diagram.
14. **Optionally fold `InstructionExecutor` into `CPU`** if simplicity is preferred; otherwise keep it and explicitly note the trade-off. I recommend keeping it, but making it hold a `CPU&` (or accepting it as a parameter) and returning `ExecResult`.
15. **Fix the executor's handler list** to exactly match the opcode set (currently the table omits `opMove`, `opLoadImmediate`, `opOr/And/Xor` in some places — make it exhaustive).

These improvements are worthwhile because they close the *semantic* gaps (PC units, endianness, FF reads, float, rotate, halt signalling) that otherwise make two "correct-looking" implementations behave differently, and they tidy the diagram's ownership/return-type ambiguities.

---

## 6. Revised PlantUML Class Diagram

```plantuml
@startuml
skinparam classAttributeIconSize 0

enum Opcode {
  Load
  LoadImmediate
  Store
  Move
  Add
  FloatAdd
  Or
  And
  Xor
  Rotate
  JumpEq
  Halt
  Invalid
}

enum ExecStatus {
  Continue
  Halt
}

struct DecodedInstruction {
  +Opcode op
  +uint8_t r
  +uint8_t s
  +uint8_t t
  +uint8_t xy
}

struct ExecResult {
  +ExecStatus status
}

struct LoadError {
  +int line
  +string message
}

struct LoadResult {
  +bool ok
  +vector<LoadError> errors
}

class MachineFault {
  +string message
}

class VoleMachine {
  -Memory memory
  -RegisterFile registers
  -CPU cpu
  -OutputDevice output
  -ProgramLoader loader
  -bool halted
  +loadProgram(path) : LoadResult
  +run()
  +step() : ExecStatus
  +reset()
  +getRegister(i) : uint8_t
  +getMemory(addr) : uint8_t
  +getPC() : uint8_t
  +getIR() : uint16_t
}

class Memory {
  -array<uint8_t,256> cells
  -OutputDevice* screen
  -uint8_t lastScreenByte
  +read(addr) : uint8_t
  +write(addr, byte)
  +reset()
}

class RegisterFile {
  -array<uint8_t,16> regs
  +read(index) : uint8_t
  +write(index, value)
  +reset()
}

class CPU {
  -uint8_t pc
  -uint16_t ir
  -Memory* memory
  -RegisterFile* registers
  -InstructionDecoder decoder
  -InstructionExecutor executor
  +step() : ExecStatus
  +fetch()
  +decode() : DecodedInstruction
  +execute(d) : ExecStatus
  +reset()
  +getPC() : uint8_t
  +getIR() : uint16_t
}

class InstructionDecoder {
  +decode(word) : DecodedInstruction
  -opcode(word) : uint8_t
  -nibble(word, n) : uint8_t
}

class InstructionExecutor {
  +execute(d, cpu, memory, registers) : ExecResult
  -opLoad(...)
  -opLoadImmediate(...)
  -opStore(...)
  -opMove(...)
  -opAdd(...)
  -opFloatAdd(...)
  -opOr(...)
  -opAnd(...)
  -opXor(...)
  -opRotate(...)
  -opJumpEq(...)
  -opHalt(...)
}

class ProgramLoader {
  +load(path, memory) : LoadResult
  -parseLine(line) : optional<uint16_t>
  -fitsInMemory(count) : bool
}

interface OutputDevice {
  +write(c : char)
  +clear()
  +getContents() : string
}

class ConsoleOutputDevice {
  +write(c : char)
  +clear()
  +getContents() : string
}

class MemoryBufferOutput {
  -string buffer
  +write(c : char)
  +clear()
  +getContents() : string
}

VoleMachine *-- Memory
VoleMachine *-- RegisterFile
VoleMachine *-- CPU
VoleMachine *-- OutputDevice
VoleMachine *-- ProgramLoader
VoleMachine ..> MachineFault : catches

CPU *-- InstructionDecoder
CPU *-- InstructionExecutor
CPU --> Memory
CPU --> RegisterFile
CPU ..> DecodedInstruction
CPU ..> ExecStatus

Memory --> OutputDevice
ProgramLoader ..> Memory
ProgramLoader ..> LoadResult
ProgramLoader ..> MachineFault

InstructionDecoder ..> DecodedInstruction
InstructionDecoder ..> MachineFault : throws
InstructionExecutor ..> DecodedInstruction
InstructionExecutor ..> ExecResult
InstructionExecutor ..> MachineFault : throws

OutputDevice <|.. ConsoleOutputDevice
OutputDevice <|.. MemoryBufferOutput

@enduml
```

---

## 7. Summary of Changes Between Original and Revised Design

| Change | Why |
|---|---|
| Added `ExecStatus` enum and `ExecResult` struct | Closes the undefined halt-signalling channel between executor, CPU, and `VoleMachine::run()`. |
| `CPU::step()` and `VoleMachine::step()` now return `ExecStatus` | Makes the run loop explicit and testable. |
| Added `getPC()`/`getIR()` on `CPU` and `VoleMachine` | Observability for tests and debugging; a simulator without visible PC/IR is hard to verify. |
| Added `lastScreenByte` to `Memory` | Defines a read policy for address FF (returns last written byte; 0 before first write) — removes an unstated decision. |
| Added `ProgramLoader::fitsInMemory(count)` and capacity check | 256 bytes = 128 instructions; loader must reject oversized programs. |
| Made ownership explicit (VoleMachine owns OutputDevice by value; Memory and CPU hold non-owning pointers) | The original diagram mixed `*--` and `-->` without stating ownership, which invites lifetime bugs. |
| Added `MachineFault` relationships: `VoleMachine ..> MachineFault` (catches), `Decoder ..> MachineFault` (throws), `Executor ..> MachineFault` (throws), `ProgramLoader ..> MachineFault` | The exception type existed but floated; now it is clearly produced and caught. |
| Added `ProgramLoader ..> LoadResult` | The loader returns it; the diagram previously omitted the return type. |
| Expanded executor handler list to all opcodes (`opMove`, `opLoadImmediate`, `opOr`, `opAnd`, `opXor`) | The original handler list was incomplete relative to the instruction set. |
| Added `CPU ..> ExecStatus` and `CPU ..> DecodedInstruction` | Makes value-type dependencies visible. |
| Kept `InstructionExecutor` as a class but documented the fold-into-CPU alternative | Preserves testability while acknowledging the trade-off. |
| Pinned down PC units (bytes, +2 per fetch), endianness, rotate modulus (`%8`), float convention, FF read policy in §3/§5 | These are spec-ambiguous points; stating them is part of the architecture, not an implementation detail. |
| Specified reset ordering in `VoleMachine::reset()` | Prevents subtle bugs (e.g., output cleared then repopulated by stale memory). |

**What did *not* change:** the overall layering (facade → memory/registers/CPU → decoder/executor → loader/output), the use of a tagged `DecodedInstruction` struct rather than a class hierarchy, the value-held arrays for memory and registers, and the absence of per-cell/per-register classes. These were the right calls and remain right; the review strengthened the design by closing *semantic and signalling* gaps rather than by adding layers.
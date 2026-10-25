# Critical review of the Vole Machine OOP design

My original proposal has a solid foundation, but after reviewing it against the machine specifications and the goal of keeping the architecture reasonably simple, I would revise several parts before treating it as the final design.

The biggest issue is not that the design has too few classes. It is that some responsibilities overlap, a few relationships are unnecessarily complicated, and certain machine behaviors are not fully specified.

In particular:

- `Machine` and `CPU` have overlapping execution responsibilities.
- The CPU has a direct dependency on `OutputDevice` even though memory-mapped output can be handled by `Memory`.
- `Memory` needs a clearer distinction between ordinary byte storage and the special behavior at address `FF`.
- The loader and decoder need a more precise boundary between textual validation, instruction validation, and execution.
- The program counter and instruction-fetch behavior need stronger rules around address boundaries and jumps.
- The UML's reference ownership and class relationships should be more consistent.

I'll evaluate the original design first, then present a revised architecture and class diagram.

## 1. What was good about the original design?

Separation of responsibilities

The CPU, memory, register file, loader, and output device had distinct purposes. This is the strongest part of the original design and should be preserved.

A single register file

Sixteen individual `Register` classes would be unnecessary for this machine. A private array with validated access is more appropriate.

A separate program loader

File parsing should not be mixed into the CPU's fetch-decode-execute cycle. This separation also makes the loader easier to test.

A lightweight decoded instruction

A simple instruction data structure is sufficient. The original proposal correctly avoided creating a subclass for every opcode without a clear need.

These are not changes I would reverse. The revised design keeps them.

## 2. Problems in the original design

### 2.1 `Machine` and `CPU` have overlapping responsibilities

The original design gave both classes `run()` and `step()`. That can be valid if `Machine` simply delegates to `CPU`, but the delegation was not made explicit.

Without that rule, it is easy to end up with two places that manage execution state or implement the fetch-decode-execute cycle.

Improvement: Make `CPU` the only class that implements execution. `Machine::run()` and `Machine::step()` may remain as convenience methods, but they must only delegate to the CPU.

This keeps the application interface simple without duplicating logic.

### 2.2 The CPU has an unnecessary output dependency

The original CPU retained references to `Memory`, `RegisterFile`, and `OutputDevice`.

However, the specified output behavior is memory-mapped: storing a value at address `FF` causes screen output.

If the CPU already writes through `Memory`, it does not need to know about `OutputDevice` directly. Having both the CPU and memory access the output device creates two possible routes to the same effect.

Improvement: Remove `CPU → OutputDevice`. Let `Memory` handle the mapped address and forward writes to the output device.

This reduces coupling and makes the output behavior consistent regardless of which component initiates a memory write.

### 2.3 Memory's behavior at `FF` is underspecified

The original diagram shows a 256-byte array and says writes to `FF` are forwarded to the output device. But it does not say whether the write also updates the stored byte or what a read from `FF` returns.

Those details affect correctness.

Improvement: Define `Memory` as the public interface to all 256 addresses, with address `FF` treated specially on writes. Do not promise that it behaves like ordinary RAM at that address. The read behavior should be documented according to the course specification; if the specification is silent, choose and document a consistent policy.

A related issue is program loading. If the loader writes instruction bytes directly into the underlying array, it can bypass memory-mapped behavior. It must use the public memory interface or a clearly defined initialization operation that enforces the same constraints.

### 2.4 The instruction representation is too ambiguous

The original `Instruction` used fields `r`, `x`, and `y`. These names are not sufficient to describe all instruction formats clearly.

Consider `40RS`. The second nibble is a reserved zero, and the final two nibbles represent registers. By contrast, in `2RXY`, the final two nibbles form an immediate value.

The meaning of a field depends on the opcode.

Improvement: Represent a decoded instruction using its opcode and three generic operand nibbles, named `a`, `b`, and `c`, or use clearly named decoded operands. The CPU interprets them according to the instruction format.

For instance:

| Instruction | Opcode | `a` | `b` | `c` |
| ----------- | ------ | --- | --- | --- |
| `2105`      | `2`    | `1` | `0` | `5` |
| `40A3`      | `4`    | `0` | `A` | `3` |
| `5ABC`      | `5`    | `A` | `B` | `C` |
| `C000`      | `C`    | `0` | `0` | `0` |

This representation is small, consistent, and does not require a hierarchy of instruction classes.

### 2.5 Validation is spread across too many methods

The original design included `parseLine()`, `validateInstructionText()`, `decode()`, `validateInstruction()`, and `isValidOpcode()`. These are not inherently wrong, but their responsibilities overlap unless clearly defined.

Improvement: Use three explicit stages:

1. Text parsing: `ProgramLoader` handles file access, comments, whitespace, and the four-hex-digit format.
2. Instruction validation and decoding: `InstructionDecoder` interprets the word and checks opcode-specific constraints.
3. Execution validation: `CPU` checks whether an operation can execute safely, including memory boundaries.

The decoder can have a private opcode-validation helper; it does not need several public validation methods for such a small instruction set.

There is one reasonable variation: the loader can invoke the decoder on every instruction before loading it, catching invalid instructions early. This is optional and should be implemented only if you want whole-program validation before execution.

### 2.6 The loader is unnecessarily tied to `Machine` ownership

The original diagram made `ProgramLoader` a component owned by `Machine`. Yet the loader can be stateless: it reads a file, returns parsed instruction words, and loads them into memory.

Improvement: Treat `ProgramLoader` as a service that `Machine` uses. It can be a local object or an externally supplied dependency rather than a permanent machine component.

This removes an unnecessary ownership relationship. If the application later needs different loading strategies, the loader can be replaced without redesigning the machine's state.

### 2.7 Word-level memory methods need stronger rules

`readWord()` and `writeWord()` are convenient, but they can accidentally obscure the byte-addressable nature of memory.

The CPU needs to know precisely which two addresses it reads and in what order. A word operation must not silently wrap around the end of memory or fetch an instruction byte from the memory-mapped output address.

Improvement: Keep `readByte()` and `writeByte()` as the fundamental memory operations. The CPU's fetch method explicitly reads the two instruction bytes and combines them in the correct order. A word helper can still exist, but only if it enforces the same boundary rules.

### 2.8 Reset behavior is not fully defined

The original design sensibly distinguished `resetCPU()` from `reset()`, but it did not specify whether reset also clears output or whether memory retains the loaded program.

Improvement: Define the reset contract:

- `CPU::reset()` resets `PC`, `IR`, and `halted`.
- `Machine::resetCPU()` restarts execution while preserving memory, registers, and output.
- `Machine::reset()` clears registers and memory, resets the CPU, and clears output.

This is a reasonable default, but the assignment's expected behavior should take precedence if specified elsewhere.

## 3. Specification compliance review

The revised design must preserve every required machine feature. Here is the audit.

| Requirement                           | Original design                     | Recommended correction                                                            |
| ------------------------------------- | ----------------------------------- | --------------------------------------------------------------------------------- |
| 16 registers, `R0`–`RF`               | Correctly modeled by `RegisterFile` | Retain a 16-byte private array and bounds checking.                               |
| 256 byte-addressable memory locations | Correctly modeled by `Memory`       | Keep byte-level access and explicitly handle `FF`.                                |
| Four-hex-digit instructions           | Correct in principle                | Separate textual syntax validation from semantic instruction validation.          |
| Two bytes per instruction             | Correct in principle                | Fetch the high byte first and low byte second; reject invalid fetch boundaries.   |
| Program counter                       | Present as an 8-bit value           | Define increment order, jump behavior, and boundary checks.                       |
| Instruction register                  | Present as a 16-bit value           | Update it during fetch and preserve it for inspection.                            |
| Conditional jump `BRXY`               | Correctly identified                | Compare `R` against `R0`; replace `PC` with `XY` only if equal.                   |
| Halt `C000`                           | Correctly identified                | Validate the reserved fields and stop execution when the instruction is executed. |
| Output at address `FF`                | Partially specified                 | Centralize write interception in `Memory` and define read semantics.              |
| Program file comments and blank lines | Correctly assigned to loader        | Strip semicolon comments, trim whitespace, and report line-specific errors.       |

### The important memory-boundary detail

Because instructions occupy two bytes and address `FF` is reserved for output, the loader must not treat the entire 256-byte address space as ordinary program storage.

If the program starts at `00` and instructions are loaded sequentially in two-byte units, the instruction starting at `FE` would need to use `FE` and `FF`. That conflicts with the mapped output address under the recommended policy.

Therefore, if `FF` cannot hold an instruction byte, the ordinary instruction-storage region is `00` through `FE`, but sequential two-byte instructions starting at `00` can only use complete pairs through `FC`–`FD`. That gives a maximum of 127 instructions in that contiguous layout.

This is a design assumption based on reserving `FF` from program storage, not a universal rule of every Vole Machine implementation. The course specification should determine whether a different memory-mapping policy is expected.

Similarly, the PC is an eight-bit address, but that does not automatically mean that every instruction fetch should wrap around. A fetch at `FF`, or a fetch requiring the mapped output address as an instruction byte, should be rejected under the proposed policy. A taken jump sets `PC` to the specified byte address; it should not silently round it down to an even address unless the specification explicitly requires alignment.

## 4. Recommended improvements

I would make six changes to the architecture.

1\. Make execution flow unambiguous

`CPU` owns the execution cycle. `Machine` exposes convenience methods that delegate to the CPU. This preserves a simple public API without duplicate execution logic.

2\. Remove the CPU's output reference

Route all mapped output through `Memory`. This reduces coupling and ensures that output is triggered by the address, not by a special case in the CPU's store instruction.

3\. Make loading a validation-first operation

Parse the entire file, validate its syntax, check its capacity and address range, and only then modify memory. This avoids leaving half a program loaded when a later line is invalid.

4\. Simplify instruction data

Use one decoded instruction structure with generic nibble fields. The decoder validates the instruction format, and the CPU interprets those fields during execution.

5\. Define boundaries and errors explicitly

Memory access, instruction fetching, program loading, and invalid opcodes should all fail predictably. No component should silently wrap an invalid address or turn malformed input into a valid instruction.

6\. Remove unnecessary loader ownership

`ProgramLoader` is a service used by `Machine`, not persistent machine state. Keep the machine's composition limited to the hardware-like components it actually owns.

### Should we introduce additional classes?

I would not add a `Register`, `MemoryCell`, `ArithmeticUnit`, `ControlUnit`, `InstructionExecutor`, or instruction subclass hierarchy at this stage.

The current requirements do not justify those abstractions. They would make the design larger without solving a demonstrated problem.

One optional addition is an `ExecutionResult` or `MachineError` type. This could provide structured outcomes such as halted, invalid opcode, invalid memory access, or instruction limit reached. However, for a relatively small assignment, exceptions and a clear error-reporting policy may be enough. Add a dedicated result type only if the UI needs to distinguish these outcomes programmatically.

The same principle applies to the floating-point instruction: an arithmetic helper might be useful once the encoding is known, but I would not introduce a separate floating-point class before its responsibilities are clear.

## 5. Revised PlantUML class diagram

This version retains the useful core classes while correcting the ownership model, removing the redundant output dependency, simplifying instruction fields, and clarifying the loader's role.

```

@startuml
title Revised Vole Machine OOP Architecture

skinparam classAttributeIconSize 0
hide empty members

class Machine {
  - cpu : CPU
  - memory : Memory
  - registers : RegisterFile
  - output : OutputDevice
  + loadProgram(path : string, start : int) : void
  + run() : void
  + step() : void
  + resetCPU() : void
  + reset() : void
}

class CPU {
  - pc : uint8
  - ir : uint16
  - halted : bool
  - memory : Memory&
  - registers : RegisterFile&
  - decoder : InstructionDecoder
  + fetch() : void
  + step() : void
  + run() : void
  + reset() : void
  + isHalted() : bool
  + getPC() : uint8
  + getIR() : uint16
  - execute(i : Instruction) : void
}

class Memory {
  - cells : byte[256]
  - output : OutputDevice&
  + readByte(address : uint8) : byte
  + writeByte(address : uint8, value : byte) : void
  + clear() : void
  - validateAddress(address : int) : void
}

class RegisterFile {
  - registers : byte[16]
  + read(index : uint8) : byte
  + write(index : uint8, value : byte) : void
  + clear() : void
  - validateIndex(index : int) : void
}

class InstructionDecoder {
  + decode(word : uint16) : Instruction
  - validateFormat(opcode : uint8, a : uint8, b : uint8, c : uint8) : void
}

class Instruction {
  + opcode : uint8
  + a : uint8
  + b : uint8
  + c : uint8
}

class ProgramLoader {
  + parseFile(path : string) : vector<uint16>
  + parseLine(line : string) : optional<uint16>
  + loadInto(memory : Memory, path : string, start : int) : void
  - validateText(text : string) : void
}

class OutputDevice {
  - outputBuffer : string
  + write(value : byte) : void
  + display() : void
  + clear() : void
}

Machine *-- "1" CPU : owns
Machine *-- "1" Memory : owns
Machine *-- "1" RegisterFile : owns
Machine *-- "1" OutputDevice : owns

CPU --> "1" Memory : accesses
CPU --> "1" RegisterFile : reads/writes
CPU *-- "1" InstructionDecoder : uses

Memory --> "1" OutputDevice : mapped writes at FF

Machine ..> ProgramLoader : uses
ProgramLoader ..> Memory : loads validated bytes into
InstructionDecoder ..> Instruction : creates
CPU ..> Instruction : executes

note right of Machine
  Coordinates components.
  run() and step() delegate to CPU.
  resetCPU preserves the loaded program.
  reset clears the complete machine state.
end note

note right of CPU
  PC is an 8-bit byte address.
  IR stores one 16-bit instruction.
  Fetch reads two bytes.
  Normal sequential PC increment is +2.
  A taken jump replaces PC with XY.
end note

note bottom of Memory
  256 addressable byte locations.
  Writes to FF trigger output behavior.
  Invalid fetch boundaries are rejected.
end note

note bottom of ProgramLoader
  Ignore blank lines and semicolon comments.
  Validate all input before changing memory.
  Reject programs that overlap reserved output
  storage or exceed available memory.
end note

@enduml

```

### Important implementation notes for the revised diagram

The diagram is intentionally conceptual, so a few details belong in the design report alongside it.

First, the `Machine` should construct the components in a safe order. Memory needs access to the output device, and the CPU needs access to memory and the register file. Construct those dependencies in an order that guarantees the referenced objects remain alive for the CPU's lifetime. In C++, references are non-owning and should not outlive the objects they reference.

Second, `ProgramLoader::loadInto()` should validate before writing. It can parse the entire file into a temporary collection, check the start address and capacity, and only then write the instruction bytes. This prevents partial loading caused by a malformed instruction near the end of a file. If the design later supports loading over an existing program, decide whether failed loading should preserve the previous program unchanged.

Third, `Memory` should be the only component that interprets the mapped output address. The loader must not write directly to the private `cells` array. It should use an approved loading operation that enforces the same memory-mapping and range constraints.

Fourth, `InstructionDecoder::decode()` should reject unsupported opcodes and invalid reserved fields. For example, the decoder should reject `C001` if the specification defines `C000` as the only valid halt encoding. The decoder should not, however, attempt to determine whether a memory address will be valid at runtime; that belongs to the relevant memory or execution operation.

Finally, the diagram assumes the decoder is a member of the CPU. This is composition because the CPU owns its decoder object. The decoder has no mutable state, so it could instead be a stateless service. Keeping it as a member is reasonable and makes the CPU's decoding dependency explicit.

## 6. What changed from the original design?

Here is the direct comparison between the original proposal and the revised version.

| Area               | Original design                                                                        | Revised design                                     | Reason                                                             |
| ------------------ | -------------------------------------------------------------------------------------- | -------------------------------------------------- | ------------------------------------------------------------------ |
| Execution          | Both `Machine` and `CPU` exposed execution methods without an explicit delegation rule | CPU owns execution; Machine delegates              | Prevents duplicated logic.                                         |
| Output             | CPU and Memory both referenced the output device                                       | Only Memory handles memory-mapped output           | Reduces coupling.                                                  |
| Instruction fields | `opcode`, `r`, `x`, `y`                                                                | `opcode`, `a`, `b`, `c`                            | Represents all instruction formats consistently.                   |
| Validation         | Several overlapping validation methods                                                 | Three clear stages: parsing, decoding, execution   | Makes responsibilities easier to explain and maintain.             |
| ProgramLoader      | Owned by Machine                                                                       | Used as a service                                  | Avoids owning a potentially stateless utility unnecessarily.       |
| Memory access      | Optional word-level helpers alongside byte access                                      | Byte-level access is fundamental                   | Makes instruction fetching and boundary checks explicit.           |
| Program loading    | Capacity and parsing checks were described separately                                  | Validate the entire program before writing         | Avoids partially loaded programs.                                  |
| Reset              | Two reset operations, with some ambiguity                                              | Explicit CPU-only and full-machine reset contracts | Makes restart behavior predictable.                                |
| Error handling     | General validation guidance                                                            | More explicit division of responsibility           | Makes invalid instructions and memory accesses easier to diagnose. |
| Class count        | Eight principal classes/data types                                                     | Same essential classes                             | Improves the architecture without unnecessary expansion.           |

### Final assessment

The original design was fundamentally reasonable. Its main weaknesses were unclear boundaries between responsibilities and some underspecified edge cases, rather than a fundamentally incorrect class structure.

The revised design is stronger because it makes three things explicit:

1. Ownership: `Machine` owns the hardware-like components, while the CPU accesses shared memory and registers without owning duplicates.
2. Control flow: loading, decoding, execution, and output each have a clear path through the system.
3. Correctness: memory mapping, instruction boundaries, conditional jumps, reset behavior, and validation have defined responsibilities.

One last detail I would correct when turning the UML into a formal implementation design: use a sufficiently wide integer type for input addresses to validation methods, or calculate `PC + 1` in a wider temporary type. If the parameter is already an eight-bit unsigned value, an out-of-range value may wrap before validation can detect it.

My recommendation: Use the revised diagram as your primary design. Keep the core classes small, explain the ownership relationships carefully, and document the unresolved machine-specific rules—especially floating-point encoding and reads from address `FF`—rather than inventing behavior that your assignment has not established.
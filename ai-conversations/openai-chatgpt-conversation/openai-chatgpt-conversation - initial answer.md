# CS213 OOP Assignment — AI Design 1

## Proposed architecture for the Vole Machine simulator

The main design goal is to model the Vole Machine as a small computer made up of cooperating objects, rather than turning every instruction into a separate class or putting the entire simulator inside one enormous `Machine` class.

I'd recommend a composition-based architecture with a central `Machine` object that owns its CPU, memory, register file, and output device. The CPU handles the fetch-decode-execute cycle, an `InstructionDecoder` translates machine-language words into meaningful instructions, and a `ProgramLoader` handles reading and validating program files.

This design is suitable for an OOP assignment because it gives you clear examples of encapsulation, composition, association, dependency, and responsibility separation without introducing unnecessary abstraction.

One important distinction: the machine's hardware state and its program-loading process should be separate. Loading a program is not the same thing as executing it, and resetting the CPU is not necessarily the same thing as erasing memory.

## 1. Overall architecture

ProgramLoader

Reads, parses and validates program files

Loads instruction words into memory

Machine

Coordinates the simulator's components

CPU

PC, IR, fetch, decode, execute

Memory

256 addressable bytes

RegisterFile

R0 through RF

OutputDevice

Captures screen output

InstructionDecoder

Interprets the opcode and instruction fields

The diagram is a conceptual overview, not the complete UML. The full class diagram later specifies the attributes, methods, and relationships.

The central design principle is that each class should own one coherent responsibility. For example, `Memory` should know how to read and write bytes, but it should not know how to add two registers or parse a file.

## 2. Classes and their responsibilities

I would use eight core classes or data types, plus an optional `Instruction` data structure. Some are full classes with behavior; others are deliberately simple.

### 2.1 `Machine`

Core class

The `Machine` represents the complete simulated computer and coordinates its major components.

Attributes

| Attribute   | Type           | Purpose                           |
| ----------- | -------------- | --------------------------------- |
| `cpu`       | `CPU`          | Processor state and execution     |
| `memory`    | `Memory`       | 256 bytes of storage              |
| `registers` | `RegisterFile` | Sixteen general-purpose registers |
| `output`    | `OutputDevice` | Screen output destination         |

Methods

- `loadProgram(path)` — delegates file parsing to `ProgramLoader` and loads the resulting instructions.
- `run()` — starts execution through the CPU.
- `step()` — executes one instruction.
- `reset()` — resets the machine to a defined initial state.
- `loadProgramData(instructions, startAddress)` — optionally loads already-parsed instruction words into memory.

The last method is useful because it separates reading a file from writing a program into machine memory.

Why it exists: Without `Machine`, the application entry point would need to construct and coordinate the CPU, memory, registers, loader, and output device itself. The `Machine` gives the rest of the application one convenient interface.

It should not implement arithmetic instructions, decode hexadecimal strings, or manage individual registers.

### 2.2 `CPU`

Core class

The CPU implements the fetch-decode-execute cycle and maintains processor state.

Attributes

| Attribute | Type                  | Purpose                                 |
| --------- | --------------------- | --------------------------------------- |
| `pc`      | 8-bit unsigned value  | Address of the next instruction         |
| `ir`      | 16-bit unsigned value | Currently fetched instruction           |
| `halted`  | `bool`                | Indicates whether execution has stopped |
| `decoder` | `InstructionDecoder`  | Interprets the current instruction      |

A separate `decoder` object is optional. It is useful here because instruction decoding is explicitly part of the assignment, but decoding could also be a private CPU method.

Methods

- `fetch()` — reads two consecutive bytes from memory and combines them into a 16-bit instruction.
- `decode()` — converts `ir` into an `Instruction`.
- `execute(instruction)` — carries out the decoded operation.
- `step()` — performs one fetch-decode-execute cycle.
- `run()` — repeats `step()` until the CPU halts or encounters an error.
- `reset()` — resets `pc`, `ir`, and `halted`.

The CPU also needs access to the `Memory`, `RegisterFile`, and `OutputDevice` objects. Those can be supplied through references or constructor dependencies.

Why `pc` is 8-bit: The machine has 256 byte addresses, so an address is eight bits. However, an instruction occupies two bytes, so normal sequential execution advances `pc` by two, not one.

Important: `ir` is 16-bit because it holds all four hexadecimal digits. A single memory location stores only one byte.

### 2.3 `Memory`

Core class

The `Memory` class models the machine's byte-addressable storage.

Attributes

| Attribute | Type               | Purpose                                      |
| --------- | ------------------ | -------------------------------------------- |
| `cells`   | Array of 256 bytes | Stores values at addresses `00` through `FF` |

Methods

- `readByte(address)` — returns one byte.
- `writeByte(address, value)` — writes one byte.
- `readWord(address)` — reads two bytes and combines them into a 16-bit instruction word, if the design supports this convenience method.
- `writeWord(address, value)` — optionally writes a word as two bytes.
- `clear()` — initializes every memory cell to zero.
- `loadBytes(startAddress, bytes)` — copies program bytes into memory after checking bounds.

I recommend implementing the fundamental memory model in terms of `readByte()` and `writeByte()`. The CPU can use those methods to fetch instructions, so `readWord()` is convenient rather than mandatory.

Memory-mapped output

Address `FF` is special. When a value is written there, the write should be forwarded to `OutputDevice` instead of treated as an ordinary memory write.

There are two reasonable designs:

1. `Memory` owns a reference to `OutputDevice` and intercepts writes to `FF`.
2. `CPU` checks for address `FF` before storing a value.

I recommend the first approach because it centralizes memory-address behavior. Every component that writes to memory will observe the same mapping.

Consequently, `Memory` has a dependency or association with `OutputDevice`, depending on whether the device is passed temporarily or retained as a member reference. In the UML proposal below, it retains a reference.

The read behavior at `FF` must also be defined. A simple simulator can return the stored byte value, but a more realistic device could define different read semantics. Your assignment only specifies screen output on the write side, so do not invent more behavior than necessary.

### 2.4 `RegisterFile`

Core class

The `RegisterFile` manages all sixteen general-purpose registers.

Attributes

| Attribute   | Type              | Purpose                  |
| ----------- | ----------------- | ------------------------ |
| `registers` | Array of 16 bytes | Stores `R0` through `RF` |

Methods

- `read(index)` — returns a register's value.
- `write(index, value)` — updates a register.
- `clear()` — resets all registers to zero.
- `validateIndex(index)` — checks that the register index is between `0` and `15`.

Register names are hexadecimal identifiers:

| Hex index | Register | Hex index | Register |
| --------- | -------- | --------- | -------- |
| `0`       | R0       | `8`       | R8       |
| `1`       | R1       | `9`       | R9       |
| `2`       | R2       | `A`       | RA       |
| `3`       | R3       | `B`       | RB       |
| `4`       | R4       | `C`       | RC       |
| `5`       | R5       | `D`       | RD       |
| `6`       | R6       | `E`       | RE       |
| `7`       | R7       | `F`       | RF       |

Should each register have its own class?

Usually, no. All sixteen registers have identical behavior and are accessed by a four-bit index. An array inside `RegisterFile` is simpler and avoids creating sixteen objects with the same attributes and methods.

A separate `Register` class becomes more worthwhile if registers have different behavior, debugging metadata, change notifications, or other unique responsibilities. None of those are required by your current specification.

Also, do not assume `R0` is permanently zero. The assignment describes sixteen general-purpose registers and uses `R0` as the comparison register for conditional jumps. Unless your instructor's machine specification explicitly says otherwise, `R0` should be writable like the others.

### 2.5 `InstructionDecoder`

Decoding component

This class converts a 16-bit instruction into its logical fields.

For example, the instruction `2A3F` means:

- Opcode: `2`
- Register: `A`
- Immediate value: `3F`

Attributes

None are necessary. A stateless decoder is sufficient.

Methods

- `decode(word)` — extracts the opcode and operand fields and returns an `Instruction`.
- `validateInstruction(instruction)` — validates opcode-specific constraints.
- Optionally, `isValidOpcode(opcode)` — checks whether an opcode is supported.

The decoded `Instruction` can contain:

| Field    | Type                             | Purpose                                          |
| -------- | -------------------------------- | ------------------------------------------------ |
| `opcode` | 4-bit value stored in an integer | Operation to execute                             |
| `r`      | Integer, 0–15                    | First register field                             |
| `x`      | Integer, 0–15                    | First hexadecimal nibble of a remaining operand  |
| `y`      | Integer, 0–15                    | Second hexadecimal nibble of a remaining operand |

You could instead store a complete `address` or `immediate` byte. I prefer keeping the raw nibble fields in the decoded structure, because some instructions interpret the fields differently.

For example, `1RXY` uses `XY` as a memory address, while `40RS` uses the final two nibbles as register identifiers. The decoder extracts the fields; the CPU interprets them according to the opcode.

Why not one class per instruction?

You could create `LoadInstruction`, `AddInstruction`, `JumpInstruction`, and so on, each implementing an `execute()` method. That is a valid polymorphic design, but for twelve simple instructions it introduces more classes, allocation or construction logic, and UML complexity.

For this assignment, a single `Instruction` data type and a decoder, with opcode-specific execution logic in `CPU`, are a more proportionate design. The polymorphic approach becomes more attractive if instructions have complex or independently extensible behavior.

### 2.6 `ProgramLoader`

Core class

The loader is responsible for converting the text file into instruction words and placing the program into memory.

Attributes

No persistent attributes are required. File paths, parsed lines, and the program's starting address can be local variables or method parameters.

Methods

- `parseFile(path)` — reads the file and returns validated instruction words.
- `parseLine(line)` — ignores blank lines and semicolon comments, then validates a non-empty instruction line.
- `validateInstructionText(text)` — verifies that an instruction contains exactly four hexadecimal digits.
- `loadInto(memory, path, startAddress)` — optionally combines parsing and loading into a single convenience operation.

I recommend separating parsing from memory writes, even if both operations are exposed through one higher-level method.

A valid line such as `2A3F` becomes the word `0x2A3F`, which is loaded as two bytes: `0x2A` followed by `0x3F`.

The loader should reject malformed lines rather than silently turning them into zeroes. It should also report the line number to make debugging easier.

Comments: The stated format says semicolon comments are ignored. A practical parser can remove everything from the first `;` to the end of the line, trim the remaining text, and skip the line if nothing remains.

### 2.7 `OutputDevice`

I/O component

The output device models the screen associated with memory address `FF`.

Attributes

| Attribute      | Type                    | Purpose                                       |
| -------------- | ----------------------- | --------------------------------------------- |
| `outputBuffer` | String or byte sequence | Retains emitted output for display or testing |

Methods

- `write(value)` — emits or records a byte written to the mapped output address.
- `display()` — presents buffered output to the user.
- `clear()` — clears the output buffer during a reset, if that reset mode is intended to clear the screen.

The exact rendering of a byte needs to be decided. For example, `0x41` could be displayed as the character `A`, or as the hexadecimal value `41`, depending on the simulator's output specification. It should not be assumed that hexadecimal text is the required output format.

Separating this class from `Memory` makes the simulator easier to test and lets you change the output behavior without modifying instruction execution.

### 2.8 `Instruction`

This is a small data structure, not necessarily a behavior-heavy class.

Attributes

- `opcode`
- `r`
- `x`
- `y`

Methods

- `getOpcode()` and operand accessors, if you want conventional encapsulation.
- Alternatively, use a simple C++ `struct` with public fields because the decoder constructs it and the CPU consumes it.

There is no need for a complicated hierarchy here. The instruction represents decoded data; the CPU decides what that data means.

## 3. Relationships between the classes

The relationships should express actual ownership and communication, rather than merely connecting every class to every other class.

| Relationship                         | Type                      | Explanation                                                 |
| ------------------------------------ | ------------------------- | ----------------------------------------------------------- |
| `Machine` → `CPU`                    | Composition               | The machine owns its processor.                             |
| `Machine` → `Memory`                 | Composition               | The machine owns its memory.                                |
| `Machine` → `RegisterFile`           | Composition               | The machine owns its register file.                         |
| `Machine` → `OutputDevice`           | Composition               | The machine owns its output device.                         |
| `CPU` → `InstructionDecoder`         | Composition or dependency | The CPU can own a stateless decoder or use a temporary one. |
| `CPU` → `Memory`                     | Association               | The CPU accesses the machine's memory.                      |
| `CPU` → `RegisterFile`               | Association               | The CPU reads and writes registers.                         |
| `Memory` → `OutputDevice`            | Association               | Memory forwards writes to the mapped output address.        |
| `ProgramLoader` → `Memory`           | Dependency or association | The loader needs memory when loading a program.             |
| `InstructionDecoder` → `Instruction` | Dependency                | The decoder creates decoded instruction data.               |
| `CPU` → `Instruction`                | Dependency                | The CPU consumes decoded instruction data.                  |

Composition versus association: Composition means that the owning object controls the lifetime of the component. Association means that an object knows about or accesses another object without necessarily owning it.

A CPU should not own a second independent copy of the machine's memory. Otherwise, the loader might write a program into one memory instance while the CPU executes another. That is a classic design bug.

The UML should therefore show `Machine` owning the components, while `CPU` and `Memory` hold non-owning references to the shared objects they need.

## 4. How the system works together

Let's trace the entire lifecycle of a program. This is where the architecture becomes more than a collection of classes.

### Step 1 — Program loading

Suppose the input file contains:

```
; Load 5 into R1
2105

; Store R1 at address FF
31FF

C000
```

The `ProgramLoader` processes the file:

1. Reads each line.
2. Removes comments and whitespace.
3. Skips blank lines.
4. Validates that each instruction contains exactly four hexadecimal digits.
5. Converts each instruction to a 16-bit word.
6. Splits the word into two bytes.
7. Loads those bytes into memory starting at the chosen program address.

Assuming the program starts at address `00`, the memory becomes:

| Address | Stored byte | Meaning               |
| ------- | ----------- | --------------------- |
| `00`    | `21`        | First half of `2105`  |
| `01`    | `05`        | Second half of `2105` |
| `02`    | `31`        | First half of `31FF`  |
| `03`    | `FF`        | Second half of `31FF` |
| `04`    | `C0`        | First half of `C000`  |
| `05`    | `00`        | Second half of `C000` |

The loader does not execute the program. It only prepares memory.

### Step 2 — Fetch

The CPU starts with `PC = 00`.

It reads memory addresses `00` and `01`, combines their bytes into `0x2105`, stores that word in `IR`, and advances `PC` to `02`.

The important invariant is:

\\[ IR = (\text{memory}[PC] \ll 8) \\;|\\; \text{memory}[PC+1] \\]

Here, the old value of `PC` is used for the fetch, and the increment happens afterward. This order matters when implementing jumps.

### Step 3 — Decode

The `InstructionDecoder` receives `0x2105` and extracts:

- Opcode `2`: load an immediate value.
- Register `1`: destination is `R1`.
- Value `05`: immediate operand.

The decoder returns an `Instruction` representing those fields.

### Step 4 — Execute

The CPU interprets the instruction and calls:

`registers.write(1, 0x05)`

Now `R1 = 05`.

On the next instruction, `31FF`, the CPU stores the value of `R1` at address `FF`. Since that address is memory-mapped, `Memory` forwards the byte to `OutputDevice`.

Finally, `C000` halts execution.

The output device receives the byte `05`. Whether it displays a character, a hexadecimal number, or some other representation depends on the chosen output convention.

### Step 5 — Reset

A reset should restore the machine to a known state, but resetting is not necessarily erasing everything.

I would define two operations:

- `resetCPU()` — sets the program counter and instruction register to their initial values and clears the halted flag.
- `reset()` — resets the CPU, clears registers, clears memory, and clears output.

If the simulator should retain a loaded program while restarting execution, use `resetCPU()` instead of the full reset.

This distinction avoids a frustrating behavior: pressing "restart" should not unexpectedly erase the program the user just loaded.

## 5. Mapping all twelve instructions to the architecture

The CPU's `execute()` method can dispatch on the decoded opcode. The following table outlines each operation's responsibility.

| Opcode | Operation               | Required interaction                                                                                 |
| ------ | ----------------------- | ---------------------------------------------------------------------------------------------------- |
| `1RXY` | Load from memory        | Read byte at `XY`, write to register `R`.                                                            |
| `2RXY` | Load immediate          | Write byte `XY` to register `R`.                                                                     |
| `3RXY` | Store to memory         | Read register `R`, write to address `XY`.                                                            |
| `40RS` | Move register           | Read `R`, write its value to `S`.                                                                    |
| `5RST` | Integer addition        | Add `S` and `T`, store result in `R`.                                                                |
| `6RST` | Floating-point addition | Decode operands according to the machine's floating-point representation, add, encode result in `R`. |
| `7RST` | Bitwise OR              | Compute `S OR T`, store in `R`.                                                                      |
| `8RST` | Bitwise AND             | Compute `S AND T`, store in `R`.                                                                     |
| `9RST` | Bitwise XOR             | Compute `S XOR T`, store in `R`.                                                                     |
| `AR0X` | Rotate right            | Rotate register `R` right by `X` bits.                                                               |
| `BRXY` | Conditional jump        | If register `R` equals `R0`, set `PC` to `XY`.                                                       |
| `C000` | Halt                    | Set the CPU's halted flag.                                                                           |

A few details deserve special attention.

Integer arithmetic. Registers hold eight-bit values. Adding two registers should produce an eight-bit result, so the result is reduced modulo 256. The two's-complement interpretation determines how that bit pattern is interpreted as a signed number; it does not require storing a separate signed integer in the register file.

Floating-point addition. The instruction list does not specify the machine's floating-point encoding. The implementation must follow the encoding from the assignment's textbook or simulator reference. Do not assume IEEE 754 single precision: each register is only eight bits.

Rotation. A rotate is not the same as a logical right shift. Bits that leave the right side re-enter at the left. A shift would discard those bits.

Conditional jump. If `R` equals `R0`, set `PC = XY`. Otherwise, keep the already-incremented `PC`. The jump target is an address in bytes, not an instruction index.

Instruction validation. `AR0X` and `C000` have constrained fields. For instance, the halt instruction should be exactly `C000`, not an arbitrary word beginning with `C`. The loader can validate hexadecimal syntax, while the decoder validates opcode-specific structure.

## 6. Encapsulation, validation, and error handling

A good OOP design should make invalid states difficult to create and errors easy to diagnose.

### Encapsulation rules

- Keep the CPU's `pc`, `ir`, and `halted` private. Expose read-only accessors for the UI or debugger.
- Keep the memory array private. Require callers to use `readByte()` and `writeByte()`.
- Keep register storage private. Require callers to use `read()` and `write()`.
- Make the decoder responsible for decoding, not for modifying CPU state.
- Make the loader responsible for parsing and loading, not for executing instructions.
- Give `OutputDevice` control over how output is recorded and presented.

The `Machine` can expose a simple public API while hiding the details of how its components interact.

### Validation checklist

| Layer          | Validation                                                                                                            |
| -------------- | --------------------------------------------------------------------------------------------------------------------- |
| File parser    | File exists and opens; comments and blank lines are handled; each instruction has four hexadecimal digits.            |
| Program loader | Program fits in available memory; start address is valid; no unintended overwrite of protected regions, if any exist. |
| Decoder        | Opcode is supported; reserved fields satisfy instruction-specific constraints.                                        |
| CPU            | Program counter is valid; execution stops safely when halted; fetch does not read past memory.                        |
| Memory         | Address is within `00`–`FF`; values written to memory fit in one byte.                                                |
| Register file  | Register index is between `0` and `15`; register values fit in eight bits.                                            |
| Output device  | Output is represented consistently and its buffer is managed safely.                                                  |

### Error-handling strategy

For a student simulator, exceptions are reasonable for failures that prevent an operation from succeeding, such as a missing file or invalid memory access. For expected user errors, such as an invalid instruction line, the loader can return a structured error containing the line number and explanation.

For example:

```
Error on line 4: expected four hexadecimal digits.
Received: 21G5
```

The key is consistency. Avoid silently substituting zero for invalid input or allowing an invalid address to corrupt memory.

You should also decide whether execution errors halt the machine, return an error to the caller, or both. A clean approach is for `CPU::step()` to report an execution error and for `Machine` or the application layer to present it to the user.

## 7. Important edge cases and potential design problems

These are the issues I would explicitly mention in your design report because they demonstrate that you understand the machine's constraints, not just its class names.

1\. Program counter overflow

`PC` is eight bits, so adding two at address `FE` would wrap around to `00` if arithmetic is performed modulo 256. But fetching an instruction at `FE` requires reading both `FE` and `FF`, and `FF` is mapped to output. The design must decide whether instructions can cross the memory boundary. I recommend rejecting instruction fetches that do not have two valid, ordinary instruction bytes, unless the machine specification explicitly allows wrapping.

2\. Program overlap and memory capacity

A 256-byte memory holds at most 128 two-byte instructions if the program occupies the entire memory in aligned words. The output address `FF` complicates that assumption, since it is mapped to a device. The loader should reject programs that exceed available storage, and the architecture should define whether loading over an existing program is allowed.

3\. Invalid instructions

The decoder must reject unknown opcodes and invalid reserved fields. `C000` is a halt instruction, but `C001` must not silently become another halt instruction unless the specification says otherwise.

4\. Jump behavior

Conditional jumps must use the address in `XY` directly. The CPU should not add another two bytes after a taken jump, or the jump target will be wrong. A jump to an invalid instruction location should produce a controlled execution error on the next fetch.

5\. Arithmetic and bitwise operations

Arithmetic must preserve the eight-bit register width. Rotation counts should be handled deliberately, including `0` and counts greater than or equal to `8`. For an eight-bit register, a rotation by eight bits is equivalent to no rotation.

6\. The meaning of `R0`

`R0` is used as the comparison register for `BRXY`, but the given specification does not say that it is a constant zero. Do not make it immutable without confirmation.

7\. Floating-point encoding

Opcode `6RST` cannot be correctly implemented until the encoding is known. This is a machine-specification question, not something that can be solved by choosing an arbitrary C++ floating-point type.

8\. Self-modifying programs

The architecture permits the CPU to store into memory locations that might contain instructions. Unless forbidden by the specification, the design should allow this. The next fetch reads the current memory contents rather than an old cached copy.

9\. Infinite loops

A valid program might never reach `C000`. The UI should have a stop mechanism or an optional instruction execution limit so the simulator does not run indefinitely.

### What I would avoid overengineering

- A `MemoryCell` class for each byte: 256 objects add complexity without adding meaningful behavior.
- A `Register` class for each register: the register file already provides a natural abstraction.
- Twelve instruction subclasses: not needed unless polymorphic instruction execution is a specific design goal.
- A separate arithmetic unit, logic unit, control unit, bus, and clock: potentially useful in a hardware simulator, but unnecessary for this assignment's requirements.
- A separate `Screen`, `Console`, `DisplayManager`, and `OutputController`: one `OutputDevice` is enough for the current output requirement.
- A `HexadecimalManager` class: hexadecimal conversion is a small parsing or formatting concern and does not warrant its own class unless reused extensively.
- Exposing raw memory or register arrays: this weakens encapsulation and makes validation difficult.

The best architecture is not the one with the most classes. It is the one in which each class has a clear reason to exist.

## 8. Complete PlantUML class diagram

The following diagram includes the main classes, their important attributes and methods, and their relationships. You can paste it into a PlantUML renderer.

```

@startuml
title CS213 Vole Machine Simulator - OOP Design

skinparam classAttributeIconSize 0
hide empty members

class Machine {
  - cpu : CPU
  - memory : Memory
  - registers : RegisterFile
  - output : OutputDevice
  - loader : ProgramLoader
  + loadProgram(path : string) : void
  + loadProgramData(words : vector, start : int) : void
  + run() : void
  + step() : void
  + reset() : void
  + resetCPU() : void
}

class CPU {
  - pc : uint8
  - ir : uint16
  - halted : bool
  - memory : Memory&
  - registers : RegisterFile&
  - decoder : InstructionDecoder
  - output : OutputDevice&
  + fetch() : uint16
  + decode() : Instruction
  + execute(i : Instruction) : void
  + step() : void
  + run() : void
  + reset() : void
  + isHalted() : bool
  + getPC() : uint8
  + getIR() : uint16
}

class Memory {
  - cells : byte[256]
  - output : OutputDevice&
  + readByte(address : uint8) : byte
  + writeByte(address : uint8, value : byte) : void
  + readWord(address : uint8) : uint16
  + writeWord(address : uint8, value : uint16) : void
  + loadBytes(start : int, bytes : vector) : void
  + clear() : void
}

class RegisterFile {
  - registers : byte[16]
  + read(index : uint8) : byte
  + write(index : uint8, value : byte) : void
  + clear() : void
  - validateIndex(index : uint8) : void
}

class InstructionDecoder {
  + decode(word : uint16) : Instruction
  + validateInstruction(i : Instruction) : void
  + isValidOpcode(opcode : uint8) : bool
}

class Instruction {
  + opcode : uint8
  + r : uint8
  + x : uint8
  + y : uint8
}

class ProgramLoader {
  + parseFile(path : string) : vector
  + parseLine(line : string) : optional
  + validateInstructionText(text : string) : bool
  + loadInto(memory : Memory, path : string, start : int) : void
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
Machine *-- "1" ProgramLoader : owns

CPU --> "1" Memory : accesses
CPU --> "1" RegisterFile : reads/writes
CPU --> "1" InstructionDecoder : uses
CPU --> "1" OutputDevice : accesses

Memory --> "1" OutputDevice : forwards writes to FF

InstructionDecoder ..> Instruction : creates
CPU ..> Instruction : executes
ProgramLoader ..> Memory : loads program into

note right of Memory
  256 byte-addressable locations.
  Address FF is memory-mapped output.
  Word operations must validate boundaries.
end note

note right of CPU
  Fetch two bytes per instruction.
  PC normally advances by 2.
  Taken branches replace PC.
  C000 halts execution.
end note

note bottom of Instruction
  Four hexadecimal digits.
  Opcode and operand nibbles
  are interpreted by instruction type.
end note

@enduml

```

### A note about translating the diagram into C++

The UML uses conceptual types such as `uint8`, `uint16`, `byte`, `vector`, and `optional` to keep the diagram readable. When implementing, choose actual C++ types such as `std::uint8_t`, `std::uint16_t`, `std::vector`, and `std::optional` as appropriate.

There is one design choice worth making explicit: `CPU` has a reference to `OutputDevice` even though memory also forwards writes to it. This is useful if the CPU needs to interact with output for another reason, but if the only output mechanism is a store to `FF`, you can remove the CPU's direct output reference. That gives you a slightly cleaner design with less coupling.

Likewise, the loader can either be owned by `Machine` or instantiated by the application and passed a reference to `Memory`. Owning it inside `Machine` is convenient for a self-contained simulator; a stateless loader outside the machine is also a valid design.

## 9. Why I would choose this design

The design decisions can be summarized by the problems they solve.

| Design decision                       | Reasoning                                                            |
| ------------------------------------- | -------------------------------------------------------------------- |
| One `Machine` coordinator             | Gives the application a single entry point into the simulator.       |
| Separate `CPU` and `Memory`           | Separates instruction execution from storage behavior.               |
| One `RegisterFile`                    | Avoids sixteen redundant classes while preserving encapsulation.     |
| One `InstructionDecoder`              | Separates interpreting machine-language words from executing them.   |
| A simple `Instruction` structure      | Keeps decoded instruction data lightweight.                          |
| Separate `ProgramLoader`              | Prevents file parsing from leaking into CPU execution logic.         |
| Separate `OutputDevice`               | Encapsulates output formatting and memory-mapped I/O behavior.       |
| Shared memory and register references | Ensures all components interact with the same machine state.         |
| Explicit reset operations             | Distinguishes restarting execution from clearing the entire machine. |

### An alternative: polymorphic instructions

If your instructor expects you to demonstrate runtime polymorphism, you could introduce an abstract `Instruction` base class with a virtual `execute()` method, then create subclasses such as `LoadInstruction`, `AddInstruction`, and `JumpInstruction`.

That architecture has a genuine benefit: each instruction encapsulates its own execution behavior, and adding new instruction types can require fewer changes to the CPU.

However, it also introduces a class for each instruction, additional object-management concerns, and more complicated dependencies between instructions and machine components. For twelve straightforward instructions, I would not choose that approach unless polymorphism is a specific learning objective or grading requirement.

The proposed design already demonstrates OOP through composition, encapsulation, associations, and responsibility separation. It does not need inheritance merely to qualify as object-oriented.

## 10. Final recommendation

I would submit the architecture above, with three refinements before finalizing the report:

1. Confirm the floating-point encoding from your course reference. This is the main unresolved machine-specific detail.
2. Document the memory-boundary and output semantics, especially whether `FF` can contain program bytes and what a read from that address returns.
3. Make the UML match your intended ownership model. In particular, the CPU should access the machine's shared memory and registers rather than owning independent copies.

The most important principle behind this proposal is that the architecture mirrors the actual responsibilities of a computer: the loader prepares the program, memory stores bytes, registers store working values, the decoder interprets instructions, the CPU executes them, and the output device handles external effects.

That gives you a design that is straightforward to explain, test, implement, and extend without creating classes that exist only to make the UML look more complicated.
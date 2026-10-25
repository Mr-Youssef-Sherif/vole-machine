
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

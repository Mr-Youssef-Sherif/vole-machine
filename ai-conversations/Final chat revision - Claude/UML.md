@startuml
title Final Vole Machine Architecture (spec-checked)
skinparam classAttributeIconSize 0
hide empty members

enum Opcode {
  LOAD_MEM = 1
  LOAD_IMM = 2
  STORE = 3
  MOVE = 4
  ADD_INT = 5
  ADD_FLOAT = 6
  OR = 7
  AND = 8
  XOR = 9
  ROTATE = 10
  JUMP_EQ = 11
  HALT = 12
}

enum CpuStatus {
  RUNNING
  HALTED
  FAULTED
}

class ConsoleUI {
  - machine : VoleMachine
  + start() : void
  - showMenu() : void
  - handleLoad() : void
  - handleEdit() : void
  - handleRun() : void
  - displayState() : void
}

class VoleMachine {
  - output : OutputDevice
  - memory : Memory
  - registers : RegisterFile
  - cpu : CPU
  - lastError : string
  + VoleMachine()
  + loadProgram(path : string, start : uint8) : void
  + setRegister(index : int, value : uint8) : void
  + setMemory(address : int, value : uint8) : void
  + step() : CpuStatus
  + run(maxSteps : int = 0) : CpuStatus
  + reset() : void
  + getLastError() : string
  + getCpu() : const CPU&
  + getMemory() : const Memory&
  + getRegisters() : const RegisterFile&
  + getOutput() : const OutputDevice&
}

class CPU {
  - pc : uint8
  - ir : uint16
  - status : CpuStatus
  - memory : Memory&
  - registers : RegisterFile&
  + CPU(m : Memory&, r : RegisterFile&)
  + step() : void
  + startAt(address : uint8) : void
  + reset() : void
  + markFaulted() : void
  + getPC() : uint8
  + getIR() : uint16
  + getStatus() : CpuStatus
  - fetch() : void
  - execute(i : Instruction) : void
}

class InstructionDecoder <<stateless>> {
  + {static} decode(word : uint16) : Instruction
  - {static} checkFormat(i : Instruction) : void
}

class Instruction <<value>> {
  + opcode : Opcode
  + a : uint8
  + b : uint8
  + c : uint8
  + xy() : uint8
}

class ALU <<stateless>> {
  + {static} addTwosComplement(x : uint8, y : uint8) : uint8
  + {static} addFloat(x : uint8, y : uint8) : uint8
  + {static} bitOr(x : uint8, y : uint8) : uint8
  + {static} bitAnd(x : uint8, y : uint8) : uint8
  + {static} bitXor(x : uint8, y : uint8) : uint8
  + {static} rotateRight(v : uint8, count : uint8) : uint8
}

class Memory {
  + {static} SIZE : int = 256
  + {static} OUTPUT_ADDRESS : int = 0xFF
  - cells : array<uint8, 256>
  - output : OutputDevice&
  + Memory(out : OutputDevice&)
  + read(address : int) : uint8
  + write(address : int, value : uint8) : void
  + canLoad(start : int, byteCount : int) : bool
  + loadBytes(start : int, data : vector<uint8>) : void
  + reset() : void
}

class RegisterFile {
  + {static} COUNT : int = 16
  - regs : array<uint8, 16>
  + read(index : int) : uint8
  + write(index : int, value : uint8) : void
  + reset() : void
}

class ProgramLoader <<stateless>> {
  + {static} loadFile(path : string) : vector<uint16>
  - {static} parseLine(line : string, lineNo : int) : optional<uint16>
}

class OutputDevice {
  - history : vector<uint8>
  + write(value : uint8) : void
  + clear() : void
  + getHistory() : vector<uint8>
}

class SimulatorError <<exception>> {
  + what() : string
}

ConsoleUI *-- "1" VoleMachine

VoleMachine *-- "1" OutputDevice
VoleMachine *-- "1" Memory
VoleMachine *-- "1" RegisterFile
VoleMachine *-- "1" CPU
VoleMachine ..> ProgramLoader : uses
VoleMachine ..> SimulatorError : throws / catches

CPU --> "1" Memory : reads/writes bytes
CPU --> "1" RegisterFile : reads/writes
CPU ..> InstructionDecoder : uses
CPU ..> ALU : uses
CPU ..> Instruction : executes
CPU ..> CpuStatus

ProgramLoader ..> InstructionDecoder : validates each word
InstructionDecoder ..> Instruction : creates
InstructionDecoder ..> Opcode
Instruction --> Opcode

Memory --> "1" OutputDevice : write at FF

ConsoleUI ..> SimulatorError : catches
CPU ..> SimulatorError : throws
Memory ..> SimulatorError : throws
RegisterFile ..> SimulatorError : throws
ProgramLoader ..> SimulatorError : throws
InstructionDecoder ..> SimulatorError : throws

note right of VoleMachine
  Owns the hardware. loadProgram: parse+validate file,
  require even start < FF, check fit, write bytes,
  cpu.startAt(start). Memory and registers are NOT wiped.
  Runtime faults are caught here -> cpu.markFaulted().
end note
note right of CPU
  PC = byte address. fetch reads 2 bytes (high first), PC += 2.
  JUMP_EQ: if R[a] == R[0] then PC = xy.
  HALT: status = HALTED.
end note
note bottom of Memory
  FF: write stores the byte AND appends it to the screen.
  loadBytes never prints and never touches FF.
end note
@enduml
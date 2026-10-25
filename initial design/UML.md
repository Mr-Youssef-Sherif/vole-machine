@startuml
skinparam classAttributeIconSize 0

class VoleMachine {
  - isInitialized: bool
  - cpu: CPU
  - memory: Memory
  - registerFile: RegisterFile
  - outputDevice: OutputDevice
  - programLoader: ProgramLoader
  + initialize(): void
  + reset(): void
  + loadProgram(filePath: string): bool
  + run(): void
  + step(): void
  + getMachineState(): string
}

class Memory {
  - cells: byte[256]
  - memorySize: int
  - outputAddress: byte
  + read(address: byte): byte
  + write(address: byte, value: byte): void
  + loadBytes(startAddress: byte, data): void
  + clear(): void
  + isValidAddress(address: int): bool
}

class RegisterFile {
  - registers: byte[16]
  - registerCount: int
  + read(registerIndex: int): byte
  + write(registerIndex: int, value: byte): void
  + clear(): void
  + isValidRegister(registerIndex: int): bool
}

class CPU {
  - programCounter: byte
  - instructionRegister: short
  - isHalted: bool
  + fetch(): short
  + decode(instruction: short)
  + execute(instruction: short): void
  + step(): void
  + run(): void
  + updateProgramCounter(): void
  + halt(): void
  + reset(): void
}

class InstructionDecoder {
  - supportedOpcodes
  + decode(instruction: short): Instruction
  + validate(instruction: short): bool
  + getInstructionFormat(opcode: byte): string
}

class ProgramLoader {
  - sourceFilePath: string
  - loadedInstructionCount: int
  + loadFromFile(filePath: string): bool
  + parseLine(line: string): Instruction
  + validateLine(line: string): bool
  + loadIntoMemory(instructions, startAddress): void
}

class OutputDevice {
  - outputBuffer
  - lastOutput: byte
  + display(value: byte): void
  + clear(): void
  + getOutput()
}

VoleMachine *-- CPU
VoleMachine *-- Memory
VoleMachine *-- RegisterFile
VoleMachine *-- ProgramLoader
VoleMachine *-- OutputDevice

CPU --> Memory : fetch / data access
CPU --> RegisterFile : read / write
CPU --> InstructionDecoder : decode
Memory ..> OutputDevice : address FF triggers output
ProgramLoader --> Memory : load program

@enduml
@startuml
skinparam classAttributeIconSize 0

interface IMemoryListener {
    + onByteWritten(address: uint8_t, value: uint8_t): void
}

class Machine {
    - running: bool
    + loadProgram(filepath: string): void
    + step(): void
    + run(): void
    + reset(): void
    + isHalted(): bool
}

class CPU {
    - pc: uint8_t
    - ir: uint16_t
    - halted: bool
    + fetch(memory: Memory): void
    + decode(): Instruction
    + execute(inst: Instruction, memory: Memory): void
    + step(memory: Memory): void
    + reset(): void
    + isHalted(): bool
}

class RegisterFile {
    - registers: std::array<uint8_t, 16>
    + read(index: uint8_t): uint8_t
    + write(index: uint8_t, value: uint8_t): void
    + reset(): void
}

class ALU {
    + addTwoComplement(a: uint8_t, b: uint8_t): uint8_t
    + addFloat(a: uint8_t, b: uint8_t): uint8_t
    + bitwiseOr(a: uint8_t, b: uint8_t): uint8_t
    + bitwiseAnd(a: uint8_t, b: uint8_t): uint8_t
    + bitwiseXor(a: uint8_t, b: uint8_t): uint8_t
    + rotateRight(val: uint8_t, n: uint8_t): uint8_t
}

class Memory {
    - storage: std::array<uint8_t, 256>
    - listeners: std::vector<IMemoryListener*>
    + readByte(address: uint8_t): uint8_t
    + writeByte(address: uint8_t, value: uint8_t): void
    + reset(): void
    + addListener(listener: IMemoryListener*): void
}

class Instruction {
    - opcode: uint8_t
    - r: uint8_t
    - s: uint8_t
    - t: uint8_t
    - x: uint8_t
    - y: uint8_t
    + getOpcode(): uint8_t
    + getR(): uint8_t
    + getS(): uint8_t
    + getT(): uint8_t
    + getX(): uint8_t
    + getY(): uint8_t
}

class InstructionDecoder {
    + decode(highByte: uint8_t, lowByte: uint8_t): Instruction
}

class Loader {
    + parseFile(filepath: string, memory: Memory): void
}

class DisplayDevice {
    - outputBuffer: std::vector<char>
    + update(value: uint8_t): void
    + render(): void
    + clear(): void
}

class SimulatorException <<exception>> {
    - message: string
}

Machine *-- CPU
Machine *-- Memory
CPU *-- RegisterFile
CPU --> ALU
CPU --> InstructionDecoder
CPU --> Instruction
Memory o--> "1..*" IMemoryListener
DisplayDevice ..up|> IMemoryListener
Loader ..> Memory : populates
Machine ..> SimulatorException : throws

@enduml
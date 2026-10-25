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
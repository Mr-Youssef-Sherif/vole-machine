#include "vole_machine.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace vole
{

    // -----------------------------------------------------------------------------
    // Shared types
    // -----------------------------------------------------------------------------

    std::uint8_t Instruction::address() const
    {
        return static_cast<std::uint8_t>((b << 4U) | c);
    }

    SimulatorError::SimulatorError(const std::string &message)
        : std::runtime_error(message) {}

    // -----------------------------------------------------------------------------
    // OutputDevice
    // -----------------------------------------------------------------------------

    void OutputDevice::display(std::uint8_t value)
    {
        displayedBytes_.push_back(value);
    }

    void OutputDevice::clear()
    {
        displayedBytes_.clear();
    }

    const std::vector<std::uint8_t> &OutputDevice::history() const
    {
        return displayedBytes_;
    }

    // -----------------------------------------------------------------------------
    // Memory
    // -----------------------------------------------------------------------------

    Memory::Memory(OutputDevice &outputDevice)
        : outputDevice_(outputDevice)
    {
        reset();
    }

    std::uint8_t Memory::read(int address) const
    {
        validateAddress(address);
        return cells_[static_cast<std::size_t>(address)];
    }

    void Memory::write(int address, std::uint8_t value)
    {
        validateAddress(address);

        cells_[static_cast<std::size_t>(address)] = value;

        if (address == OUTPUT_ADDRESS)
        {
            outputDevice_.display(value);
        }
    }

    bool Memory::canStoreProgram(int startAddress, std::size_t byteCount) const
    {
        if (startAddress < 0 || startAddress >= OUTPUT_ADDRESS)
        {
            return false;
        }

        // FF is reserved for the output device, so program bytes must end at FE
        // or earlier. This subtraction-based check also avoids integer overflow.
        const std::size_t availableBytes =
            static_cast<std::size_t>(OUTPUT_ADDRESS - startAddress);
        return byteCount <= availableBytes;
    }

    void Memory::storeProgram(int startAddress,
                              const std::vector<std::uint8_t> &bytes)
    {
        if (!canStoreProgram(startAddress, bytes.size()))
        {
            throw SimulatorError("Program does not fit in memory before address FF.");
        }

        // Program loading is not a CPU store instruction; it must not activate
        // the output device even if the memory map changes in the future.
        for (std::size_t offset = 0; offset < bytes.size(); ++offset)
        {
            cells_[static_cast<std::size_t>(startAddress) + offset] = bytes[offset];
        }
    }

    void Memory::reset()
    {
        cells_.fill(0);
    }

    void Memory::validateAddress(int address) const
    {
        if (address < 0 || address >= CELL_COUNT)
        {
            std::ostringstream message;
            message << "Memory address is out of range: " << address
                    << " (valid range is 00-FF).";
            throw SimulatorError(message.str());
        }
    }

    // -----------------------------------------------------------------------------
    // RegisterFile
    // -----------------------------------------------------------------------------

    std::uint8_t RegisterFile::read(int registerIndex) const
    {
        validateIndex(registerIndex);
        return values_[static_cast<std::size_t>(registerIndex)];
    }

    void RegisterFile::write(int registerIndex, std::uint8_t value)
    {
        validateIndex(registerIndex);
        values_[static_cast<std::size_t>(registerIndex)] = value;
    }

    void RegisterFile::reset()
    {
        values_.fill(0);
    }

    void RegisterFile::validateIndex(int registerIndex) const
    {
        if (registerIndex < 0 || registerIndex >= REGISTER_COUNT)
        {
            std::ostringstream message;
            message << "Register index is out of range: " << registerIndex
                    << " (valid registers are R0-RF).";
            throw SimulatorError(message.str());
        }
    }

    // -----------------------------------------------------------------------------
    // InstructionDecoder
    // -----------------------------------------------------------------------------

    Instruction InstructionDecoder::decode(std::uint16_t machineWord) const
    {
        const auto opcodeValue = static_cast<std::uint8_t>((machineWord >> 12U) & 0x0FU);

        Instruction instruction;
        instruction.opcode = static_cast<Opcode>(opcodeValue);
        instruction.a = static_cast<std::uint8_t>((machineWord >> 8U) & 0x0FU);
        instruction.b = static_cast<std::uint8_t>((machineWord >> 4U) & 0x0FU);
        instruction.c = static_cast<std::uint8_t>(machineWord & 0x0FU);

        if (opcodeValue < 0x1U || opcodeValue > 0xCU)
        {
            std::ostringstream message;
            message << "Unsupported opcode in instruction 0x"
                    << std::uppercase << std::hex << std::setw(4)
                    << std::setfill('0') << machineWord << '.';
            throw SimulatorError(message.str());
        }

        validateFormat(instruction, machineWord);
        return instruction;
    }

    void InstructionDecoder::validateFormat(const Instruction &instruction,
                                            std::uint16_t originalWord) const
    {
        bool valid = true;

        switch (instruction.opcode)
        {
        case Opcode::MOVE:
            // 40RS: the second nibble is reserved and must be zero.
            valid = (instruction.a == 0);
            break;

        case Opcode::ROTATE_RIGHT:
            // AR0X: the third nibble is reserved and must be zero.
            valid = (instruction.b == 0);
            break;

        case Opcode::HALT:
            // C000: all three operand nibbles are zero.
            valid = (instruction.a == 0 && instruction.b == 0 && instruction.c == 0);
            break;

        default:
            break;
        }

        if (!valid)
        {
            std::ostringstream message;
            message << "Invalid instruction format: 0x"
                    << std::uppercase << std::hex << std::setw(4)
                    << std::setfill('0') << originalWord << '.';
            throw SimulatorError(message.str());
        }
    }

    // -----------------------------------------------------------------------------
    // ALU
    // -----------------------------------------------------------------------------

    std::uint8_t ALU::addInteger(std::uint8_t left, std::uint8_t right) const
    {
        // The result is stored in an 8-bit register, so overflow wraps around.
        return static_cast<std::uint8_t>(static_cast<unsigned int>(left) + right);
    }

    std::uint8_t ALU::addFloat(std::uint8_t, std::uint8_t) const
    {
        // The provided assignment architecture does not define the 8-bit float
        // representation. Do not invent one; this operation needs confirmation.
        throw SimulatorError(
            "Opcode 6 (floating-point addition) needs the assignment's 8-bit "
            "floating-point format before it can be implemented.");
    }

    std::uint8_t ALU::bitwiseOr(std::uint8_t left, std::uint8_t right) const
    {
        return static_cast<std::uint8_t>(left | right);
    }

    std::uint8_t ALU::bitwiseAnd(std::uint8_t left, std::uint8_t right) const
    {
        return static_cast<std::uint8_t>(left & right);
    }

    std::uint8_t ALU::bitwiseXor(std::uint8_t left, std::uint8_t right) const
    {
        return static_cast<std::uint8_t>(left ^ right);
    }

    std::uint8_t ALU::rotateRight(std::uint8_t value,
                                  std::uint8_t bitCount) const
    {
        const unsigned int shift = static_cast<unsigned int>(bitCount) % 8U;
        if (shift == 0U)
        {
            return value;
        }

        const unsigned int byteValue = value;
        return static_cast<std::uint8_t>(
            (byteValue >> shift) | (byteValue << (8U - shift)));
    }

    // -----------------------------------------------------------------------------
    // ProgramLoader
    // -----------------------------------------------------------------------------

    std::vector<std::uint16_t> ProgramLoader::load(
        const std::string &filePath,
        const InstructionDecoder &decoder) const
    {
        std::ifstream input(filePath);

        if (!input.is_open())
        {
            throw SimulatorError("Could not open program file: " + filePath);
        }

        std::ostringstream contents;
        contents << input.rdbuf();

        if (input.bad())
        {
            throw SimulatorError("An error occurred while reading: " + filePath);
        }

        return parseText(contents.str(), decoder);
    }

    std::vector<std::uint16_t> ProgramLoader::parseText(
        const std::string &source,
        const InstructionDecoder &decoder) const
    {
        std::vector<std::uint16_t> words;
        std::istringstream input(source);

        std::string line;
        std::size_t lineNumber = 0;

        while (std::getline(input, line))
        {
            ++lineNumber;

            std::uint16_t machineWord = 0;

            if (!parseLine(line, lineNumber, machineWord))
            {
                continue;
            }

            try
            {
                decoder.decode(machineWord);
            }
            catch (const SimulatorError &error)
            {
                throw SimulatorError(
                    "Line " + std::to_string(lineNumber) + ": " + error.what());
            }

            words.push_back(machineWord);
        }

        if (words.empty())
        {
            throw SimulatorError("The program contains no instructions.");
        }

        return words;
    }

    bool ProgramLoader::parseLine(
        const std::string &line,
        std::size_t lineNumber,
        std::uint16_t &machineWord) const
    {
        // Remove everything after the first semicolon.
        const std::size_t commentPosition = line.find(';');

        std::string instructionText = line.substr(0, commentPosition);

        // Trim leading and trailing whitespace.
        const std::size_t first = instructionText.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
        {
            return false;
        }

        const std::size_t last = instructionText.find_last_not_of(" \t\r\n");

        instructionText = instructionText.substr(first, last - first + 1);

        // Each instruction must contain exactly four hexadecimal digits.
        if (instructionText.size() != 4)
        {
            throw SimulatorError(
                "Line " + std::to_string(lineNumber) +
                ": expected exactly four hexadecimal digits.");
        }

        machineWord = 0;

        for (const unsigned char character : instructionText)
        {
            if (!std::isxdigit(character))
            {
                throw SimulatorError(
                    "Line " + std::to_string(lineNumber) +
                    ": invalid hexadecimal instruction.");
            }

            const char upper = static_cast<char>(std::toupper(character));

            const std::uint16_t digit =
                (upper >= '0' && upper <= '9')
                    ? static_cast<std::uint16_t>(upper - '0')
                    : static_cast<std::uint16_t>(upper - 'A' + 10);

            machineWord = static_cast<std::uint16_t>(
                (machineWord << 4U) | digit);
        }

        return true;
    }

    // -----------------------------------------------------------------------------
    // CPU
    // -----------------------------------------------------------------------------

    CPU::CPU(Memory &memory, RegisterFile &registers)
        : memory_(memory), registers_(registers) {}

    void CPU::startAt(std::uint8_t address)
    {
        pc_ = address;
        ir_ = 0;
        status_ = CpuStatus::READY;
    }

    void CPU::start()
    {
        if (status_ == CpuStatus::READY)
        {
            status_ = CpuStatus::RUNNING;
        }
    }

    void CPU::step()
    {
        if (status_ != CpuStatus::RUNNING)
        {
            return;
        }

        fetch();
        const Instruction currentInstruction = decoder_.decode(ir_);
        execute(currentInstruction);
    }

    void CPU::reset()
    {
        pc_ = 0;
        ir_ = 0;
        status_ = CpuStatus::READY;
    }

    void CPU::markFaulted()
    {
        status_ = CpuStatus::FAULTED;
    }

    std::uint8_t CPU::programCounter() const
    {
        return pc_;
    }

    std::uint16_t CPU::instructionRegister() const
    {
        return ir_;
    }

    CpuStatus CPU::status() const
    {
        return status_;
    }

    void CPU::fetch()
    {
        // Instructions occupy two bytes. FF is reserved for output, so the low
        // byte of an instruction cannot be fetched from FF.
        if (pc_ > 0xFDU)
        {
            std::ostringstream message;
            message << "Instruction fetch out of range at PC=0x"
                    << std::uppercase << std::hex << static_cast<int>(pc_) << '.';
            throw SimulatorError(message.str());
        }

        const std::uint8_t highByte = memory_.read(pc_);
        const std::uint8_t lowByte = memory_.read(static_cast<int>(pc_) + 1);

        ir_ = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(highByte) << 8U) | lowByte);

        // PC counts bytes, not instructions.
        pc_ = static_cast<std::uint8_t>(pc_ + 2U);
    }

    void CPU::execute(const Instruction &instruction)
    {
        const int r = instruction.a;
        const int s = instruction.b;
        const int t = instruction.c;

        switch (instruction.opcode)
        {
        case Opcode::LOAD_FROM_MEMORY:
            registers_.write(r, memory_.read(instruction.address()));
            break;

        case Opcode::LOAD_IMMEDIATE:
            registers_.write(r, instruction.address());
            break;

        case Opcode::STORE_TO_MEMORY:
            memory_.write(instruction.address(), registers_.read(r));
            break;

        case Opcode::MOVE:
            // 40RS: R (nibble b) is the source; S (nibble c) is the destination.
            registers_.write(t, registers_.read(s));
            break;

        case Opcode::ADD_INTEGER:
            registers_.write(r, alu_.addInteger(registers_.read(s), registers_.read(t)));
            break;

        case Opcode::ADD_FLOAT:
            registers_.write(r, alu_.addFloat(registers_.read(s), registers_.read(t)));
            break;

        case Opcode::BITWISE_OR:
            registers_.write(r, alu_.bitwiseOr(registers_.read(s), registers_.read(t)));
            break;

        case Opcode::BITWISE_AND:
            registers_.write(r, alu_.bitwiseAnd(registers_.read(s), registers_.read(t)));
            break;

        case Opcode::BITWISE_XOR:
            registers_.write(r, alu_.bitwiseXor(registers_.read(s), registers_.read(t)));
            break;

        case Opcode::ROTATE_RIGHT:
            registers_.write(r, alu_.rotateRight(registers_.read(r), instruction.c));
            break;

        case Opcode::JUMP_IF_EQUAL:
            if (registers_.read(r) == registers_.read(0))
            {
                pc_ = instruction.address();
            }
            break;

        case Opcode::HALT:
            status_ = CpuStatus::HALTED;
            break;
        }
    }

    // -----------------------------------------------------------------------------
    // VoleMachine
    // -----------------------------------------------------------------------------

    VoleMachine::VoleMachine()
        : output_(),
          memory_(output_),
          registers_(),
          cpu_(memory_, registers_) {}

    void VoleMachine::loadProgram(
        const std::string &filePath,
        std::uint8_t startAddress)
    {
        if ((startAddress % 2U) != 0U ||
            startAddress >= Memory::OUTPUT_ADDRESS)
        {
            throw SimulatorError(
                "Program start address must be even and less than FF.");
        }

        const auto words = loader_.load(filePath, decoder_);
        const auto bytes = convertWordsToBytes(words);

        if (!memory_.canStoreProgram(startAddress, bytes.size()))
        {
            throw SimulatorError(
                "Program does not fit in memory before address FF.");
        }

        memory_.storeProgram(startAddress, bytes);
        cpu_.startAt(startAddress);
        programLoaded_ = true;

        lastError_.clear();
    }

    void VoleMachine::setRegister(int registerIndex, std::uint8_t value)
    {
        registers_.write(registerIndex, value);
    }

    void VoleMachine::setMemory(int address, std::uint8_t value)
    {
        memory_.write(address, value);
    }

    void VoleMachine::clearOutput()
    {
        output_.clear();
    }

    bool VoleMachine::hasProgramLoaded() const
    {
        return programLoaded_;
    }

    CpuStatus VoleMachine::step()
    {
        if (cpu_.status() == CpuStatus::READY && programLoaded_)
        {
            cpu_.start();
        }

        if (cpu_.status() != CpuStatus::RUNNING)
        {
            return cpu_.status();
        }

        try
        {
            cpu_.step();
            lastError_.clear();
        }
        catch (const std::exception &error)
        {
            recordRuntimeFault(error);
        }

        return cpu_.status();
    }

    CpuStatus VoleMachine::run(std::size_t maximumSteps)
    {
        if (cpu_.status() == CpuStatus::READY && programLoaded_)
        {
            cpu_.start();
        }

        std::size_t executedSteps = 0;

        while (cpu_.status() == CpuStatus::RUNNING)
        {
            if (maximumSteps != 0 && executedSteps >= maximumSteps)
            {
                break;
            }

            step();
            ++executedSteps;
        }

        return cpu_.status();
    }

    void VoleMachine::reset()
    {
        memory_.reset();
        registers_.reset();
        output_.clear();
        cpu_.reset();
        programLoaded_ = false;
        lastError_.clear();
    }

    CpuStatus VoleMachine::status() const
    {
        return cpu_.status();
    }

    std::string VoleMachine::lastError() const
    {
        return lastError_;
    }

    const CPU &VoleMachine::cpu() const
    {
        return cpu_;
    }

    const Memory &VoleMachine::memory() const
    {
        return memory_;
    }

    const RegisterFile &VoleMachine::registers() const
    {
        return registers_;
    }

    const OutputDevice &VoleMachine::output() const
    {
        return output_;
    }

    std::vector<std::uint8_t> VoleMachine::convertWordsToBytes(
        const std::vector<std::uint16_t> &words) const
    {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(words.size() * 2U);

        for (const std::uint16_t word : words)
        {
            bytes.push_back(static_cast<std::uint8_t>((word >> 8U) & 0xFFU));
            bytes.push_back(static_cast<std::uint8_t>(word & 0xFFU));
        }

        return bytes;
    }

    void VoleMachine::recordRuntimeFault(const std::exception &error)
    {
        std::ostringstream message;
        message << error.what()
                << " [PC=0x" << std::uppercase << std::hex
                << static_cast<int>(cpu_.programCounter())
                << ", IR=0x" << std::setw(4) << std::setfill('0')
                << cpu_.instructionRegister() << "]";

        lastError_ = message.str();
        cpu_.markFaulted();
    }

    void VoleMachine::validateProgramText(
        const std::string &source,
        std::uint8_t startAddress) const
    {
        if ((startAddress % 2U) != 0U ||
            startAddress >= Memory::OUTPUT_ADDRESS)
        {
            throw SimulatorError(
                "Program start address must be even and less than FF.");
        }

        const auto words = loader_.parseText(source, decoder_);
        const auto bytes = convertWordsToBytes(words);

        if (!memory_.canStoreProgram(startAddress, bytes.size()))
        {
            throw SimulatorError(
                "Program does not fit in memory before address FF.");
        }
    }

    void VoleMachine::loadProgramText(
        const std::string &source,
        std::uint8_t startAddress)
    {
        validateProgramText(source, startAddress);

        const auto words = loader_.parseText(source, decoder_);
        const auto bytes = convertWordsToBytes(words);

        memory_.storeProgram(startAddress, bytes);
        cpu_.startAt(startAddress);
        programLoaded_ = true;

        lastError_.clear();
    }

} // namespace vole

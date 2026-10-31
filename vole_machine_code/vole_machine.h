#ifndef VOLE_MACHINE_H
#define VOLE_MACHINE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace vole
{

    // -----------------------------------------------------------------------------
    // Shared machine types
    // -----------------------------------------------------------------------------

    enum class Opcode : std::uint8_t
    {
        LOAD_FROM_MEMORY = 0x1,
        LOAD_IMMEDIATE = 0x2,
        STORE_TO_MEMORY = 0x3,
        MOVE = 0x4,
        ADD_INTEGER = 0x5,
        ADD_FLOAT = 0x6,
        BITWISE_OR = 0x7,
        BITWISE_AND = 0x8,
        BITWISE_XOR = 0x9,
        ROTATE_RIGHT = 0xA,
        JUMP_IF_EQUAL = 0xB,
        HALT = 0xC
    };

    enum class CpuStatus
    {
        READY,
        RUNNING,
        HALTED,
        FAULTED
    };

    // A decoded instruction consists of four hexadecimal nibbles:
    // [opcode][a][b][c]. The meaning of a/b/c depends on the opcode.
    struct Instruction
    {
        Opcode opcode{};
        std::uint8_t a{};
        std::uint8_t b{};
        std::uint8_t c{};

        std::uint8_t address() const;
    };

    class SimulatorError : public std::runtime_error
    {
    public:
        explicit SimulatorError(const std::string &message);
    };

    // -----------------------------------------------------------------------------
    // Hardware components
    // -----------------------------------------------------------------------------

    // Stores the bytes sent to the simulated screen. It does not print them;
    // the user interface decides how to display the captured output.
    class OutputDevice
    {
    public:
        void display(std::uint8_t value);
        void clear();
        const std::vector<std::uint8_t> &history() const;

    private:
        std::vector<std::uint8_t> displayedBytes_;
    };

    // The machine has 256 byte-addressable memory cells. Address FF is mapped to
    // the output device. Memory is the only component that knows this rule.
    class Memory
    {
    public:
        static constexpr int CELL_COUNT = 256;
        static constexpr int OUTPUT_ADDRESS = 0xFF;

        explicit Memory(OutputDevice &outputDevice);

        std::uint8_t read(int address) const;
        void write(int address, std::uint8_t value);

        bool canStoreProgram(int startAddress, std::size_t byteCount) const;
        void storeProgram(int startAddress, const std::vector<std::uint8_t> &bytes);

        void reset();

    private:
        std::array<std::uint8_t, CELL_COUNT> cells_{};
        OutputDevice &outputDevice_; // Memory uses it; VoleMachine owns it.

        void validateAddress(int address) const;
    };

    // Holds the sixteen 8-bit CPU registers, R0 through RF.
    class RegisterFile
    {
    public:
        static constexpr int REGISTER_COUNT = 16;

        std::uint8_t read(int registerIndex) const;
        void write(int registerIndex, std::uint8_t value);
        void reset();

    private:
        std::array<std::uint8_t, REGISTER_COUNT> values_{};

        void validateIndex(int registerIndex) const;
    };

    // -----------------------------------------------------------------------------
    // Instruction support
    // -----------------------------------------------------------------------------

    // Converts a raw 16-bit instruction into its opcode and three nibbles.
    // It also checks that the opcode and reserved fields use supported formats.
    class InstructionDecoder
    {
    public:
        Instruction decode(std::uint16_t machineWord) const;

    private:
        void validateFormat(const Instruction &instruction,
                            std::uint16_t originalWord) const;
    };

    // Performs the arithmetic and bitwise work requested by CPU instructions.
    // This class does not own registers and does not change machine state.
    class ALU
    {
    public:
        std::uint8_t addInteger(std::uint8_t left, std::uint8_t right) const;
        std::uint8_t addFloat(std::uint8_t left, std::uint8_t right) const;
        std::uint8_t bitwiseOr(std::uint8_t left, std::uint8_t right) const;
        std::uint8_t bitwiseAnd(std::uint8_t left, std::uint8_t right) const;
        std::uint8_t bitwiseXor(std::uint8_t left, std::uint8_t right) const;
        std::uint8_t rotateRight(std::uint8_t value, std::uint8_t bitCount) const;
    };

    // Reads the text representation of a program and converts each valid line
    // into a 16-bit machine word. It does not write to memory or start the CPU.
    class ProgramLoader
    {
    public:
        std::vector<std::uint16_t> parseText(
            const std::string &source,
            const InstructionDecoder &decoder) const;

        std::vector<std::uint16_t> load(
            const std::string &filePath,
            const InstructionDecoder &decoder) const;

    private:
        bool parseLine(
            const std::string &line,
            std::size_t lineNumber,
            std::uint16_t &machineWord) const;
    };

    // -----------------------------------------------------------------------------
    // CPU
    // -----------------------------------------------------------------------------

    // CPU owns its execution state (PC, IR and status), but works with the
    // machine's Memory and RegisterFile through references.
    class CPU
    {
    public:
        CPU(Memory &memory, RegisterFile &registers);

        void startAt(std::uint8_t address);
        void start();
        void step();
        void reset();
        void markFaulted();

        std::uint8_t programCounter() const;
        std::uint16_t instructionRegister() const;
        CpuStatus status() const;

    private:
        Memory &memory_;
        RegisterFile &registers_;

        InstructionDecoder decoder_;
        ALU alu_;

        std::uint8_t pc_ = 0;
        std::uint16_t ir_ = 0;
        CpuStatus status_ = CpuStatus::READY;

        void fetch();
        void execute(const Instruction &instruction);
    };

    // -----------------------------------------------------------------------------
    // Whole machine
    // -----------------------------------------------------------------------------

    // VoleMachine is the public entry point for the backend. It creates and owns
    // the hardware components, coordinates loading/execution, and captures faults.
    class VoleMachine
    {
    public:
        VoleMachine();

        void loadProgram(const std::string &filePath, std::uint8_t startAddress);
        void setRegister(int registerIndex, std::uint8_t value);
        void setMemory(int address, std::uint8_t value);
        void clearOutput();
        bool hasProgramLoaded() const;
        void validateProgramText(
            const std::string &source,
            std::uint8_t startAddress) const;

        void loadProgramText(
            const std::string &source,
            std::uint8_t startAddress);

        CpuStatus step();
        CpuStatus run(std::size_t maximumSteps = 0);
        void reset();

        CpuStatus status() const;
        std::string lastError() const;

        const CPU &cpu() const;
        const Memory &memory() const;
        const RegisterFile &registers() const;
        const OutputDevice &output() const;

    private:
        // Declaration order matters: Memory needs OutputDevice, and CPU needs
        // Memory and RegisterFile. C++ constructs members in this order.
        OutputDevice output_;
        Memory memory_;
        RegisterFile registers_;
        CPU cpu_;

        InstructionDecoder decoder_;
        ProgramLoader loader_;

        std::string lastError_;
        bool programLoaded_ = false;

        std::vector<std::uint8_t> convertWordsToBytes(
            const std::vector<std::uint16_t> &words) const;
        void recordRuntimeFault(const std::exception &error);
    };

} // namespace vole

#endif // VOLE_MACHINE_H

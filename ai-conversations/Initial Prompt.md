# CS213 OOP Assignment — AI Design 1

I am working on a C++ Object-Oriented Programming assignment to design a simulator for the Vole Machine, a simple computer with hexadecimal machine-language instructions.

I need to produce an OOP design, not a complete implementation. Please focus on the architecture, classes, responsibilities, attributes, methods, relationships, and UML.

## Machine specifications

- 16 general-purpose 8-bit registers: R0 through RF.
- 256 byte-addressable memory locations: 00 through FF.
- Each instruction is four hexadecimal digits and occupies two bytes.
- The CPU maintains a program counter (PC) and an instruction register (IR).
- Execution follows a fetch-decode-execute cycle.
- Memory address FF is mapped to screen output.
- The program loader should handle files containing one four-hex-digit instruction per line, with blank lines and semicolon comments ignored.

## Supported instructions

- `1RXY`: Load register R from memory address XY.
- `2RXY`: Load immediate value XY into register R.
- `3RXY`: Store register R to memory address XY.
- `40RS`: Move the contents of register R into register S.
- `5RST`: Add registers S and T and store the result in R using two's-complement arithmetic.
- `6RST`: Floating-point addition.
- `7RST`: Bitwise OR.
- `8RST`: Bitwise AND.
- `9RST`: Bitwise XOR.
- `AR0X`: Rotate register R right by X bits.
- `BRXY`: Jump to address XY if register R equals R0.
- `C000`: Halt execution.

## Your task

Please independently propose an OOP architecture for this simulator.

1. Identify the classes needed and explain the responsibility of each.
2. Specify the important attributes and methods of every class.
3. Explain the relationships between the classes, including composition, association, and dependency where appropriate.
4. Explain how program loading, memory access, register operations, instruction decoding, execution, output, and resetting work together.
5. Explain how encapsulation, validation, and error handling should be handled.
6. Consider whether separate classes for individual registers, memory cells, instruction decoding, and output devices are justified.
7. Identify likely design problems, unnecessary complexity, and important edge cases.
8. Produce a complete PlantUML class diagram that represents your proposal.
9. Explain the reasoning behind the major design choices.

Please do not implement the simulator in C++. This is an architectural design task.

Develop your own design independently rather than assuming there is one correct architecture. Where multiple approaches are reasonable, explain the trade-offs rather than presenting an arbitrary choice as mandatory.
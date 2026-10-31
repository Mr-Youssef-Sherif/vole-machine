#include "vole_machine.h"

#include "imgui-master/imgui.h"
#include "imgui-master/backends/imgui_impl_glfw.h"
#include "imgui-master/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace
{
    constexpr std::size_t EDITOR_CAPACITY = 64 * 1024;
    constexpr std::size_t MAX_RUN_STEPS = 10000;

    vole::VoleMachine machine;

    char currentFilePath[1024] = "";
    char programEditor[EDITOR_CAPACITY] =
        "; Example: load 5 and 3, add them, display the result, then halt\n"
        "2005\n"
        "2103\n"
        "5201\n"
        "32FF\n"
        "C000\n";

    int startAddress = 0;
    int runStepLimit = static_cast<int>(MAX_RUN_STEPS);
    int selectedRegister = 0;
    int registerValue = 0;
    int memoryAddress = 0;
    int memoryValue = 0;

    std::string uiMessage = "Create a program, open a file, or edit the example.";
    bool uiMessageIsError = false;

    ImFont *editorFont = nullptr;

    void SetMessage(const std::string &message, bool isError = false)
    {
        uiMessage = message;
        uiMessageIsError = isError;
    }

    const char *StatusName(vole::CpuStatus status)
    {
        switch (status)
        {
        case vole::CpuStatus::READY:
            return "READY";
        case vole::CpuStatus::RUNNING:
            return "RUNNING";
        case vole::CpuStatus::HALTED:
            return "HALTED";
        case vole::CpuStatus::FAULTED:
            return "FAULTED";
        }
        return "UNKNOWN";
    }

    bool ReadFileIntoEditor(const char *path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            SetMessage(std::string("Could not open file: ") + path, true);
            return false;
        }

        std::ostringstream contents;
        contents << file.rdbuf();

        if (file.bad())
        {
            SetMessage("An error occurred while reading the selected file.", true);
            return false;
        }

        const std::string source = contents.str();
        if (source.size() >= EDITOR_CAPACITY)
        {
            SetMessage("The selected file is too large for the program editor.", true);
            return false;
        }

        std::fill(programEditor, programEditor + EDITOR_CAPACITY, '\0');
        std::copy(source.begin(), source.end(), programEditor);

        std::snprintf(currentFilePath, sizeof(currentFilePath), "%s", path);
        SetMessage(std::string("Opened ") + currentFilePath +
                   ". You can edit the imported code before loading it.");
        return true;
    }

    bool SaveEditorToPath(const char *path)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            SetMessage(std::string("Could not save file: ") + path, true);
            return false;
        }

        file << programEditor;
        if (!file.good())
        {
            SetMessage(std::string("An error occurred while saving: ") + path, true);
            return false;
        }

        std::snprintf(currentFilePath, sizeof(currentFilePath), "%s", path);
        SetMessage(std::string("Saved ") + currentFilePath);
        return true;
    }

    bool ChooseOpenFile(char *path, std::size_t capacity)
    {
#ifdef _WIN32
        OPENFILENAMEA dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFilter =
            "Vole and text programs (*.vole;*.txt)\0*.vole;*.txt\0"
            "Vole programs (*.vole)\0*.vole\0"
            "Text files (*.txt)\0*.txt\0"
            "All files (*.*)\0*.*\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = static_cast<DWORD>(capacity);
        dialog.lpstrTitle = "Open Vole Program";
        dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        return GetOpenFileNameA(&dialog) == TRUE;
#else
        (void)path;
        (void)capacity;
        SetMessage("The native file picker is currently implemented for Windows only.", true);
        return false;
#endif
    }

    bool ChooseSaveFile(char *path, std::size_t capacity)
    {
#ifdef _WIN32
        OPENFILENAMEA dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFilter =
            "Vole program (*.vole)\0*.vole\0"
            "Text file (*.txt)\0*.txt\0"
            "All files (*.*)\0*.*\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = static_cast<DWORD>(capacity);
        dialog.lpstrDefExt = "vole";
        dialog.lpstrTitle = "Save Vole Program";
        dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        return GetSaveFileNameA(&dialog) == TRUE;
#else
        (void)path;
        (void)capacity;
        SetMessage("The native file picker is currently implemented for Windows only.", true);
        return false;
#endif
    }

    void NewProgram()
    {
        std::fill(programEditor, programEditor + EDITOR_CAPACITY, '\0');
        currentFilePath[0] = '\0';
        SetMessage("New untitled program. Start writing instructions in the editor.");
    }

    void OpenProgram()
    {
        char selectedPath[1024] = "";
        if (currentFilePath[0] != '\0')
        {
            std::snprintf(selectedPath, sizeof(selectedPath), "%s", currentFilePath);
        }

        if (ChooseOpenFile(selectedPath, sizeof(selectedPath)))
        {
            ReadFileIntoEditor(selectedPath);
        }
    }

    void SaveProgramAs()
    {
        char selectedPath[1024] = "";
        if (currentFilePath[0] != '\0')
        {
            std::snprintf(selectedPath, sizeof(selectedPath), "%s", currentFilePath);
        }

        if (ChooseSaveFile(selectedPath, sizeof(selectedPath)))
        {
            SaveEditorToPath(selectedPath);
        }
    }

    void SaveProgram()
    {
        if (currentFilePath[0] == '\0')
        {
            SaveProgramAs();
            return;
        }

        SaveEditorToPath(currentFilePath);
    }

    bool ValidStartAddress()
    {
        if (startAddress < 0 || startAddress > 254 || (startAddress % 2) != 0)
        {
            SetMessage("Start address must be an even decimal value from 0 to 254 (00 to FE).", true);
            return false;
        }
        return true;
    }

    void ValidateProgram()
    {
        if (!ValidStartAddress())
            return;

        try
        {
            machine.validateProgramText(
                std::string(programEditor),
                static_cast<std::uint8_t>(startAddress));
            SetMessage("Validation successful: all instructions are valid and the program fits in memory.");
        }
        catch (const std::exception &error)
        {
            SetMessage(error.what(), true);
        }
    }

    void LoadProgramIntoMemory()
    {
        if (!ValidStartAddress())
            return;

        try
        {
            machine.loadProgramText(
                std::string(programEditor),
                static_cast<std::uint8_t>(startAddress));
            SetMessage("Program loaded into memory at " +
                       [&]()
                       {
                           std::ostringstream out;
                           out << "0x" << std::uppercase << std::hex
                               << std::setw(2) << std::setfill('0') << startAddress;
                           return out.str();
                       }() +
                       ". CPU is READY; press Step or Run to execute.");
        }
        catch (const std::exception &error)
        {
            SetMessage(error.what(), true);
        }
    }

    void DrawTopBar()
    {
        ImGui::Text("VOLE MACHINE");
        ImGui::SameLine();
        ImGui::TextDisabled("|  8-bit educational computer simulator");
        ImGui::Separator();

        const auto status = machine.status();

        const ImVec4 statusColor =
            status == vole::CpuStatus::HALTED
                ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f)
            : status == vole::CpuStatus::FAULTED
                ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
            : status == vole::CpuStatus::RUNNING
                ? ImVec4(0.95f, 0.75f, 0.25f, 1.0f)
                : ImVec4(0.65f, 0.72f, 0.82f, 1.0f);

        ImGui::Text("CPU:");
        ImGui::SameLine();
        ImGui::TextColored(statusColor, "%s", StatusName(status));

        ImGui::SameLine();
        ImGui::Text("PC: %02X", machine.cpu().programCounter());

        ImGui::SameLine();
        ImGui::Text("IR: %04X", machine.cpu().instructionRegister());

        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s",
            machine.hasProgramLoaded() ? "Program loaded" : "No program loaded");

        ImGui::Separator();
    }

    void DrawProgramPanel()
    {
        ImGui::BeginChild(
            "ProgramEditorPanel",
            ImVec2(0, 0),
            ImGuiChildFlags_Borders);

        ImGui::Text("Program workspace");
        ImGui::TextWrapped(
            "Write instructions directly, or open an existing .vole or .txt file.");

        ImGui::Spacing();

        // File controls.
        // Wrap onto another line when the available width is insufficient.
        const float buttonHeight = 36.0f;
        const float buttonWidth = 90.0f;

        if (ImGui::Button("New", ImVec2(buttonWidth, buttonHeight)))
            NewProgram();

        ImGui::SameLine();
        if (ImGui::Button("Open...", ImVec2(buttonWidth, buttonHeight)))
            OpenProgram();

        ImGui::SameLine();
        if (ImGui::Button("Save", ImVec2(buttonWidth, buttonHeight)))
            SaveProgram();

        ImGui::SameLine();
        if (ImGui::Button("Save As...", ImVec2(buttonWidth + 15.0f, buttonHeight)))
            SaveProgramAs();

        ImGui::Spacing();

        ImGui::TextDisabled(
            "%s",
            currentFilePath[0] != '\0'
                ? currentFilePath
                : "Untitled program");

        ImGui::Separator();

        ImGui::Text("Source code");
        ImGui::TextDisabled(
            "One 4-digit hexadecimal instruction per line. Use ; for comments.");

        ImGui::Spacing();

        if (editorFont)
            ImGui::PushFont(editorFont);

        // Reserve room for the editor label, start address and action buttons.
        const float reservedHeight = 165.0f;
        const float editorHeight = std::max(
            180.0f,
            ImGui::GetContentRegionAvail().y - reservedHeight);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 12.0f));

        ImGui::InputTextMultiline(
            "##program_editor",
            programEditor,
            sizeof(programEditor),
            ImVec2(-1.0f, editorHeight),
            ImGuiInputTextFlags_AllowTabInput);

        ImGui::PopStyleVar();

        if (editorFont)
            ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Text("Program loading");

        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputInt("Start address (decimal)", &startAddress, 2, 16);
        startAddress = std::clamp(startAddress, 0, 254);

        ImGui::Spacing();

        if (ImGui::Button("Validate program", ImVec2(145.0f, 38.0f)))
            ValidateProgram();

        ImGui::SameLine();

        if (ImGui::Button("Load into memory", ImVec2(175.0f, 38.0f)))
            LoadProgramIntoMemory();

        ImGui::EndChild();
    }

    void DrawExecutionPanel()
    {
        ImGui::BeginChild("ExecutionPanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::Text("Execution");
        ImGui::TextDisabled("Loading prepares the CPU. Execution starts only when you press Step or Run.");

        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputInt("Maximum run steps", &runStepLimit);
        runStepLimit = std::clamp(runStepLimit, 1, 1000000);

        const bool canExecute = machine.hasProgramLoaded() &&
                                machine.status() != vole::CpuStatus::HALTED &&
                                machine.status() != vole::CpuStatus::FAULTED;

        ImGui::BeginDisabled(!canExecute);
        if (ImGui::Button("Step", ImVec2(95, 38)))
        {
            const auto result = machine.step();
            if (result == vole::CpuStatus::HALTED)
                SetMessage("HALT instruction executed. Program completed successfully.");
            else if (result == vole::CpuStatus::FAULTED)
                SetMessage(machine.lastError(), true);
            else
                SetMessage("Executed one instruction.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Run", ImVec2(95, 38)))
        {
            const auto result = machine.run(static_cast<std::size_t>(runStepLimit));
            if (result == vole::CpuStatus::FAULTED)
                SetMessage(machine.lastError(), true);
            else if (result == vole::CpuStatus::HALTED)
                SetMessage("HALT instruction executed. Program completed successfully.");
            else
                SetMessage("Execution limit reached. The CPU is still RUNNING; this is not a completed program.");
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Reset machine", ImVec2(135, 38)))
        {
            machine.reset();
            SetMessage("Machine reset. Memory, registers, output and loaded-program state were cleared.");
        }

        ImGui::Separator();
        ImGui::Text("CPU state");
        ImGui::BulletText("Program Counter: 0x%02X", machine.cpu().programCounter());
        ImGui::BulletText("Instruction Register: 0x%04X", machine.cpu().instructionRegister());
        ImGui::BulletText("Status: %s", StatusName(machine.status()));

        if (machine.status() == vole::CpuStatus::READY && machine.hasProgramLoaded())
            ImGui::TextWrapped("The program is loaded and waiting. It is not executing yet.");
        else if (machine.status() == vole::CpuStatus::RUNNING)
            ImGui::TextWrapped("The CPU is executing. A program normally finishes when it reaches C000 (HALT).");
        else if (machine.status() == vole::CpuStatus::HALTED)
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.55f, 1.0f), "Program halted normally.");
        else if (machine.status() == vole::CpuStatus::FAULTED)
            ImGui::TextColored(ImVec4(1.0f, 0.38f, 0.35f, 1.0f), "Execution stopped because of a runtime fault.");

        if (!machine.lastError().empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.38f, 0.35f, 1.0f), "Runtime fault");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(machine.lastError().c_str());
            ImGui::PopTextWrapPos();
        }

        ImGui::Separator();
        ImGui::Text("Manual hardware access");

        ImGui::SetNextItemWidth(105.0f);
        ImGui::InputInt("Register index (0-15)", &selectedRegister);
        selectedRegister = std::clamp(selectedRegister, 0, 15);
        ImGui::SetNextItemWidth(105.0f);
        ImGui::InputInt("Register value (0-255)", &registerValue);
        registerValue = std::clamp(registerValue, 0, 255);
        if (ImGui::Button("Write register"))
        {
            machine.setRegister(selectedRegister, static_cast<std::uint8_t>(registerValue));
            SetMessage("Register updated.");
        }

        ImGui::Spacing();
        ImGui::SetNextItemWidth(105.0f);
        ImGui::InputInt("Memory address (0-255)", &memoryAddress);
        memoryAddress = std::clamp(memoryAddress, 0, 255);
        ImGui::SetNextItemWidth(105.0f);
        ImGui::InputInt("Memory value (0-255)", &memoryValue);
        memoryValue = std::clamp(memoryValue, 0, 255);
        if (ImGui::Button("Write memory"))
        {
            try
            {
                machine.setMemory(memoryAddress, static_cast<std::uint8_t>(memoryValue));
                SetMessage(memoryAddress == 255
                               ? "Memory FF written; the output device received the byte."
                               : "Memory updated.");
            }
            catch (const std::exception &error)
            {
                SetMessage(error.what(), true);
            }
        }

        ImGui::EndChild();
    }

    void DrawRegisterPanel()
    {
        ImGui::BeginChild("RegisterPanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::Text("Registers");
        if (ImGui::BeginTable("registers", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchSame,
                              ImVec2(0, 0)))
        {
            for (int row = 0; row < 4; ++row)
            {
                ImGui::TableNextRow();
                for (int column = 0; column < 4; ++column)
                {
                    const int index = row * 4 + column;
                    ImGui::TableSetColumnIndex(column);
                    ImGui::Text("R%X", index);
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.45f, 0.78f, 1.0f, 1.0f),
                                       "%02X", machine.registers().read(index));
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    void DrawMemoryPanel()
    {
        ImGui::BeginChild("MemoryPanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::Text("Memory");
        ImGui::TextDisabled("256 bytes. Address FF is mapped to the output device.");

        if (ImGui::BeginTable("memory", 17,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
                              ImVec2(0, ImGui::GetContentRegionAvail().y)))
        {
            ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, 48.0f);
            for (int col = 0; col < 16; ++col)
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 34.0f);
            ImGui::TableHeadersRow();

            for (int row = 0; row < 16; ++row)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%X0", row);
                for (int col = 0; col < 16; ++col)
                {
                    const int address = row * 16 + col;
                    ImGui::TableSetColumnIndex(col + 1);
                    const auto value = machine.memory().read(address);
                    if (address == vole::Memory::OUTPUT_ADDRESS)
                        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%02X", value);
                    else
                        ImGui::Text("%02X", value);
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    void DrawOutputPanel()
    {
        ImGui::BeginChild("OutputPanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::Text("FF output screen");
        ImGui::TextDisabled("Every CPU write to memory address FF is captured by the output device.");

        const auto &history = machine.output().history();
        if (history.empty())
        {
            ImGui::BeginChild("screen_preview", ImVec2(0, 115), ImGuiChildFlags_Borders);
            ImGui::TextDisabled("SCREEN IS EMPTY");
            ImGui::EndChild();
            ImGui::TextDisabled("No bytes have been sent to address FF yet.");
        }
        else
        {
            const std::uint8_t latest = history.back();
            ImGui::BeginChild("screen_preview", ImVec2(0, 115), ImGuiChildFlags_Borders);
            ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.62f, 1.0f), "LATEST SCREEN OUTPUT");
            ImGui::Spacing();
            ImGui::Text("Hexadecimal:  0x%02X", latest);
            ImGui::Text("Decimal:      %u", static_cast<unsigned int>(latest));
            if (latest >= 32 && latest <= 126)
                ImGui::Text("ASCII:        '%c'", static_cast<char>(latest));
            else
                ImGui::Text("ASCII:        Non-printable byte");
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::Text("Screen content");
            std::string screenContent;
            for (const std::uint8_t byte : history)
            {
                if (byte >= 32 && byte <= 126)
                    screenContent.push_back(static_cast<char>(byte));
                else
                {
                    std::ostringstream token;
                    token << '[' << std::uppercase << std::hex
                          << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned int>(byte) << ']';
                    screenContent += token.str();
                }
                screenContent.push_back(' ');
            }
            ImGui::BeginChild("screen_content", ImVec2(0, 65), ImGuiChildFlags_Borders);
            ImGui::TextWrapped("%s", screenContent.c_str());
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::Text("Output history (%zu bytes)", history.size());
            if (ImGui::BeginTable("output_history", 3,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_ScrollY,
                                  ImVec2(0, ImGui::GetContentRegionAvail().y - 45.0f)))
            {
                ImGui::TableSetupColumn("#");
                ImGui::TableSetupColumn("Hex");
                ImGui::TableSetupColumn("Decimal");
                ImGui::TableHeadersRow();
                for (std::size_t i = 0; i < history.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", i + 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%02X", history[i]);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", static_cast<unsigned int>(history[i]));
                }
                ImGui::EndTable();
            }
        }

        if (ImGui::Button("Clear output history", ImVec2(170, 34)))
        {
            machine.clearOutput();
            SetMessage("Output history cleared. Memory contents were not changed.");
        }
        ImGui::EndChild();
    }

    void DrawInstructionReference()
    {
        ImGui::BeginChild("InstructionReference", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::Text("Instruction reference");
        if (ImGui::BeginTable("instructions", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp,
                              ImVec2(0, 0)))
        {
            ImGui::TableSetupColumn("Opcode", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Meaning");
            ImGui::TableHeadersRow();

            const char *rows[][3] = {
                {"1", "1RXY", "Load memory[XY] into R"},
                {"2", "2RXY", "Load immediate XY into R"},
                {"3", "3RXY", "Store R at memory[XY]; FF sends output"},
                {"4", "40RS", "Reference: copy S into R; current backend copies R into S"},
                {"5", "5RST", "Add R[S] and R[T] into R"},
                {"6", "6RST", "Floating-point add (encoding not yet defined)"},
                {"7", "7RST", "Bitwise OR"},
                {"8", "8RST", "Bitwise AND"},
                {"9", "9RST", "Bitwise XOR"},
                {"A", "AR0X", "Rotate R right by X bits"},
                {"B", "BRXY", "Jump to XY if R equals R0"},
                {"C", "C000", "Halt execution"}};

            for (const auto &row : rows)
            {
                ImGui::TableNextRow();
                for (int col = 0; col < 3; ++col)
                {
                    ImGui::TableSetColumnIndex(col);
                    ImGui::TextWrapped("%s", row[col]);
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    void ApplyStyle()
    {
        ImGui::StyleColorsDark();
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 8.0f;
        style.ChildRounding = 7.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 7.0f;
        style.GrabRounding = 5.0f;
        style.WindowPadding = ImVec2(18.0f, 16.0f);
        style.FramePadding = ImVec2(11.0f, 9.0f);
        style.ItemSpacing = ImVec2(10.0f, 10.0f);
        style.CellPadding = ImVec2(8.0f, 6.0f);
        style.ScrollbarSize = 18.0f;
        style.GrabMinSize = 14.0f;
        style.TabRounding = 5.0f;
    }

    void LoadFonts()
    {
#ifdef _WIN32
        ImFont *mainFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            "C:/Windows/Fonts/segoeui.ttf", 18.0f);
        editorFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            "C:/Windows/Fonts/consola.ttf", 18.0f);
        if (mainFont)
            ImGui::GetIO().FontDefault = mainFont;
#endif
        if (!editorFont)
            editorFont = ImGui::GetIO().Fonts->AddFontDefault();
    }
} // namespace

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW.\n");
        return 1;
    }

#if defined(__APPLE__)
    const char *glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char *glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow *window = glfwCreateWindow(
        1600, 1000, "Vole Machine Simulator", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        std::fprintf(stderr, "Failed to create GLFW window.\n");
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    LoadFonts();
    ApplyStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
        ImGui::Begin("Vole Machine Workbench", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        DrawTopBar();

        if (ImGui::BeginTabBar("main_tabs"))
        {
            if (ImGui::BeginTabItem("Program"))
            {
                const float availableWidth = ImGui::GetContentRegionAvail().x;

                if (ImGui::BeginTable(
                        "program_layout",
                        2,
                        ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_BordersInnerV,
                        ImVec2(availableWidth, 0)))
                {
                    ImGui::TableSetupColumn(
                        "Editor",
                        ImGuiTableColumnFlags_WidthStretch,
                        1.65f);

                    ImGui::TableSetupColumn(
                        "Execution",
                        ImGuiTableColumnFlags_WidthStretch,
                        1.0f);

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    DrawProgramPanel();

                    ImGui::TableNextColumn();
                    DrawExecutionPanel();

                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Machine state"))
            {
                ImGui::BeginChild("machine_tab", ImVec2(0, 0));
                ImGui::Columns(2, "machine_layout", true);
                DrawRegisterPanel();
                ImGui::NextColumn();
                DrawMemoryPanel();
                ImGui::Columns(1);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Output & instructions"))
            {
                ImGui::BeginChild("output_tab", ImVec2(0, 0));
                ImGui::Columns(2, "output_layout", true);
                DrawOutputPanel();
                ImGui::NextColumn();
                DrawInstructionReference();
                ImGui::Columns(1);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();
        ImGui::BeginChild(
            "StatusMessage",
            ImVec2(0, 48),
            ImGuiChildFlags_None);

        if (uiMessageIsError)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.38f, 0.35f, 1.0f),
                "%s",
                uiMessage.c_str());
        }
        else
        {
            ImGui::TextColored(
                ImVec4(0.45f, 0.82f, 0.62f, 1.0f),
                "%s",
                uiMessage.c_str());
        }

        ImGui::EndChild();

        ImGui::End();
        ImGui::Render();

        int displayWidth = 0;
        int displayHeight = 0;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.055f, 0.065f, 0.085f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#include "cpu.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief Affiche l'état actuel des registres du CPU d'une manière formatée.
 * @param state La structure contenant l'état du CPU.
 * @param message Un message optionnel à afficher avant l'état.
 */
void PrintCpuState(const CpuDebugState& state, const std::string& message = "")
{
    std::cout << std::left << std::setw(20) << message;

    std::cout << std::hex << std::uppercase << std::setfill('0');
    std::cout << "PC: " << std::setw(4) << state.pc
        << " | A: " << std::setw(4) << state.a
        << " | X: " << std::setw(4) << state.x
        << " | Y: " << std::setw(4) << state.y
        << " | SP: " << std::setw(4) << state.sp
        << " | Flags: " << (state.n ? 'N' : '-') << (state.v ? 'V' : '-')
        << (state.mf ? 'M' : '-') << (state.xf ? 'X' : '-') << (state.d ? 'D' : '-')
        << (state.i ? 'I' : '-') << (state.z ? 'Z' : '-') << (state.c ? 'C' : '-')
        << (state.e ? 'E' : '-')
        << std::endl;
}

void CheckState(bool condition, const std::string& successMessage)
{
    std::cout << "  [CHECK] " << successMessage << "... ";
    if (condition)
    {
        std::cout << "OK" << std::endl;
    }
    else
    {
        std::cout << "ECHEC" << std::endl;
        assert(condition && "L'assertion a echoué !");
    }
}


int main()
{
    // Simuler la mémoire système (64 KB)
    std::vector<uint8_t> memory(0x10000, 0);

    auto readHandler = [&](uint32_t address) -> uint8_t {
        return memory[address & 0xFFFF];
        };
    auto writeHandler = [&](uint32_t address, uint8_t value) {
        memory[address & 0xFFFF] = value;
        };
    auto idleHandler = [](bool isWaiting) {};

    Cpu cpu(readHandler, writeHandler, idleHandler);
    std::cout << "Instance du CPU créée." << std::endl;

    /*
        $8000: 18        CLC          ; Efface la retenue (pour XCE)
        $8001: FB        XCE          ; Echange C(0) et E(1) -> Passe en mode natif (E=0)
        $8002: 78        SEI          ; Désactive les interruptions
        $8003: C2 18     REP #$18     ; Met X/Y en mode 16-bit (XF=0, D=0)
        $8005: E2 20     SEP #$20     ; Met A en mode 8-bit (MF=1)
        $8007: A2 EF 01  LDX #$01EF   ; Charge X avec la valeur 16-bit $01EF
        $800A: 9A        TXS          ; Transfère X vers le Stack Pointer S
        $800B: 00        BRK          ; Fin
    */

    // Vecteur de reset pointe vers $8000
    memory[0xFFFC] = 0x00;
    memory[0xFFFD] = 0x80;

    // Code du programme
    uint16_t pc = 0x8000;
    memory[pc++] = 0x18; // CLC
    memory[pc++] = 0xFB; // XCE
    memory[pc++] = 0x78; // SEI
    memory[pc++] = 0xC2; memory[pc++] = 0x18; // REP #$18
    memory[pc++] = 0xE2; memory[pc++] = 0x20; // SEP #$20
    memory[pc++] = 0xA2; memory[pc++] = 0xEF; memory[pc++] = 0x01; // LDX #$01EF
    memory[pc++] = 0x9A; // TXS
    memory[pc++] = 0x00; // BRK

    std::cout << "Programme de test simple charge en memoire à $8000.\n" << std::endl;

    cpu.Reset(true);
    cpu.RunOpcode();
    PrintCpuState(cpu.GetDebugState(), "After RESET");
    CheckState(cpu.GetDebugState().pc == 0x8000, "PC initialise au vecteur de reset");
    CheckState(cpu.GetDebugState().e == true, "Demarrage en mode Emulation (E=1)");
    CheckState(cpu.GetDebugState().mf == true && cpu.GetDebugState().xf == true, "Drapeaux M et X forces a 1");
    CheckState(cpu.GetDebugState().sp == 0x01FD, "Stack Pointer initialise correctement");

    cpu.RunOpcode(); // CLC
    PrintCpuState(cpu.GetDebugState(), "After CLC");
    CheckState(cpu.GetDebugState().c == false, "Drapeau Carry efface");

    cpu.RunOpcode(); // XCE
    PrintCpuState(cpu.GetDebugState(), "After XCE");
    CheckState(cpu.GetDebugState().e == false, "Passe en mode Natif (E=0)");
    CheckState(cpu.GetDebugState().c == true, "Carry a recu l'ancienne valeur de E");
    CheckState(cpu.GetDebugState().mf == false && cpu.GetDebugState().xf == false, "Drapeaux M et X effaces par la sortie du mode E");

    cpu.RunOpcode(); // SEI
    PrintCpuState(cpu.GetDebugState(), "After SEI");
    CheckState(cpu.GetDebugState().i == true, "Drapeau d'interruption active (I=1)");

    cpu.RunOpcode(); // REP #$18
    PrintCpuState(cpu.GetDebugState(), "After REP #$18");
    CheckState(cpu.GetDebugState().d == false, "Drapeau Decimal efface (D=0)");
    CheckState(cpu.GetDebugState().xf == false, "Drapeau X efface (X/Y -> 16-bit)");

    cpu.RunOpcode(); // SEP #$20
    PrintCpuState(cpu.GetDebugState(), "After SEP #$20");
    CheckState(cpu.GetDebugState().mf == true, "Drapeau M active (A -> 8-bit)");

    cpu.RunOpcode(); // LDX #$01EF
    PrintCpuState(cpu.GetDebugState(), "After LDX #$01EF");
    CheckState(cpu.GetDebugState().x == 0x01EF, "Registre X charge avec la valeur 16-bit correcte");

    cpu.RunOpcode(); // TXS
    PrintCpuState(cpu.GetDebugState(), "After TXS");
    CheckState(cpu.GetDebugState().sp == 0x01EF, "Stack Pointer mis a jour depuis X");

    cpu.RunOpcode(); // BRK
    PrintCpuState(cpu.GetDebugState(), "After BRK");
    CheckState(cpu.GetDebugState().sp == 0x01EB, "Stack Pointer decremente par BRK natif (-4)");

    return 0;
}
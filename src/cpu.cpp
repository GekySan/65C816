#include "cpu.hpp"

#include <utility>
#include <tuple>
#include <algorithm>

/**
 * @struct Cpu::PImpl
 * @brief Contient tous les membres et méthodes privés de la classe Cpu.
 */
struct Cpu::PImpl
{
    // Membres de Données
    ReadHandler m_readHandler;
    WriteHandler m_writeHandler;
    IdleHandler m_idleHandler;

    // Registres
    uint16_t m_a = 0, m_x = 0, m_y = 0, m_sp = 0, m_pc = 0, m_dp = 0;
    uint8_t  m_k = 0, m_db = 0;

    // Drapeaux (Flags)
    bool m_c = false, m_z = false, m_v = false, m_n = false;
    bool m_i = false, m_d = false, m_xf = false, m_mf = false, m_e = false;

    // État
    bool m_waiting = false, m_stopped = false;

    // Interruptions
    bool m_irqWanted = false, m_nmiWanted = false, m_intWanted = false, m_resetWanted = true;

    // Constructeur
    PImpl(ReadHandler read, WriteHandler write, IdleHandler idle)
        : m_readHandler(std::move(read))
        , m_writeHandler(std::move(write))
        , m_idleHandler(std::move(idle))
    {
        Reset(true);
    }

    // Coeur
    void Reset(bool hard);
    void RunOpcode();

    // Bus / Mémoire
    uint8_t Read(uint32_t address);
    void Write(uint32_t address, uint8_t value);
    void Idle();
    void IdleWait();

    // Interruptions
    void CheckInterrupts();
    void DoInterrupt();

    // Récupération des Opcodes
    uint8_t ReadOpcode();
    uint16_t ReadOpcodeWord(bool intCheck);

    // Drapeaux
    uint8_t GetFlags();
    void SetFlags(uint8_t value);
    void SetZnFlags(uint16_t value, bool isByte);

    // Pile
    void PushByte(uint8_t value);
    uint8_t PullByte();
    void PushWord(uint16_t value, bool intCheck);
    uint16_t PullWord(bool intCheck);

    // Accès Mémoire
    uint16_t ReadWord(uint32_t adrL, uint32_t adrH, bool intCheck);
    void WriteWord(uint32_t adrL, uint32_t adrH, uint16_t value, bool reversed, bool intCheck);
    void DoBranch(bool condition);

    // Modes d'adressage
    void AdrImp();
    std::pair<uint32_t, uint32_t> AdrImm(bool xFlag);
    std::pair<uint32_t, uint32_t> AdrDp();
    std::pair<uint32_t, uint32_t> AdrDpx();
    std::pair<uint32_t, uint32_t> AdrDpy();
    std::pair<uint32_t, uint32_t> AdrIdp();
    std::pair<uint32_t, uint32_t> AdrIdx();
    std::pair<uint32_t, uint32_t> AdrIdy(bool write);
    std::pair<uint32_t, uint32_t> AdrIdl();
    std::pair<uint32_t, uint32_t> AdrIly();
    std::pair<uint32_t, uint32_t> AdrSr();
    std::pair<uint32_t, uint32_t> AdrIsy();
    std::pair<uint32_t, uint32_t> AdrAbs();
    std::pair<uint32_t, uint32_t> AdrAbx(bool write);
    std::pair<uint32_t, uint32_t> AdrAby(bool write);
    std::pair<uint32_t, uint32_t> AdrAbl();
    std::pair<uint32_t, uint32_t> AdrAlx();

    // Logique des OpCodes
    void And(uint32_t low, uint32_t high);
    void Ora(uint32_t low, uint32_t high);
    void Eor(uint32_t low, uint32_t high);
    void Adc(uint32_t low, uint32_t high);
    void Sbc(uint32_t low, uint32_t high);
    void Cmp(uint32_t low, uint32_t high);
    void Cpx(uint32_t low, uint32_t high);
    void Cpy(uint32_t low, uint32_t high);
    void Bit(uint32_t low, uint32_t high);
    void Lda(uint32_t low, uint32_t high);
    void Ldx(uint32_t low, uint32_t high);
    void Ldy(uint32_t low, uint32_t high);
    void Sta(uint32_t low, uint32_t high);
    void Stx(uint32_t low, uint32_t high);
    void Sty(uint32_t low, uint32_t high);
    void Stz(uint32_t low, uint32_t high);
    void Ror(uint32_t low, uint32_t high);
    void Rol(uint32_t low, uint32_t high);
    void Lsr(uint32_t low, uint32_t high);
    void Asl(uint32_t low, uint32_t high);
    void Inc(uint32_t low, uint32_t high);
    void Dec(uint32_t low, uint32_t high);
    void Tsb(uint32_t low, uint32_t high);
    void Trb(uint32_t low, uint32_t high);

    void DoOpcode(uint8_t opcode);
};


Cpu::Cpu(ReadHandler readHandler, WriteHandler writeHandler, IdleHandler idleHandler)
    : m_pimpl(std::make_unique<PImpl>(std::move(readHandler), std::move(writeHandler), std::move(idleHandler)))
{
}

Cpu::~Cpu() = default;

void Cpu::Reset(bool hard)
{
    m_pimpl->Reset(hard);
}

void Cpu::RunOpcode()
{
    m_pimpl->RunOpcode();
}

void Cpu::Nmi()
{
    m_pimpl->m_nmiWanted = true;
}

void Cpu::SetIrq(bool state)
{
    m_pimpl->m_irqWanted = state;
}

CpuDebugState Cpu::GetDebugState() const
{
    return {
        .a = m_pimpl->m_a, .x = m_pimpl->m_x, .y = m_pimpl->m_y, .sp = m_pimpl->m_sp, .pc = m_pimpl->m_pc, .dp = m_pimpl->m_dp,
        .k = m_pimpl->m_k, .db = m_pimpl->m_db,
        .c = m_pimpl->m_c, .z = m_pimpl->m_z, .v = m_pimpl->m_v, .n = m_pimpl->m_n,
        .i = m_pimpl->m_i, .d = m_pimpl->m_d, .xf = m_pimpl->m_xf, .mf = m_pimpl->m_mf, .e = m_pimpl->m_e
    };
}

void Cpu::PImpl::Reset(bool hard)
{
    if (hard)
    {
        m_a = 0; m_x = 0; m_y = 0; m_sp = 0; m_pc = 0; m_dp = 0;
        m_k = 0; m_db = 0;
        m_c = false; m_z = false; m_v = false; m_n = false;
        m_i = false; m_d = false; m_xf = false; m_mf = false;
        m_e = false; m_irqWanted = false;
    }
    m_waiting = false;
    m_stopped = false;
    m_nmiWanted = false;
    m_intWanted = false;
    m_resetWanted = true;
}

void Cpu::PImpl::RunOpcode()
{
    if (m_resetWanted)
    {
        m_resetWanted = false;
        Read((static_cast<uint32_t>(m_k) << 16) | m_pc);
        Idle();
        Read(0x100 | (m_sp-- & 0xff));
        Read(0x100 | (m_sp-- & 0xff));
        Read(0x100 | (m_sp-- & 0xff));
        m_sp = (m_sp & 0xff) | 0x100;
        m_e = true;
        m_i = true;
        m_d = false;
        SetFlags(GetFlags());
        m_k = 0;
        m_pc = ReadWord(0xfffc, 0xfffd, false);
        return;
    }

    if (m_stopped)
    {
        IdleWait();
        return;
    }

    if (m_waiting)
    {
        if (m_irqWanted || m_nmiWanted)
        {
            m_waiting = false;
            Idle();
            CheckInterrupts();
            Idle();
        }
        else
        {
            IdleWait();
        }
        return;
    }

    CheckInterrupts();
    if (m_intWanted)
    {
        Read((static_cast<uint32_t>(m_k) << 16) | m_pc);
        DoInterrupt();
    }
    else
    {
        uint8_t opcode = ReadOpcode();
        DoOpcode(opcode);
    }
}

uint8_t Cpu::PImpl::Read(uint32_t address) { return m_readHandler(address); }
void Cpu::PImpl::Write(uint32_t address, uint8_t value) { m_writeHandler(address, value); }
void Cpu::PImpl::Idle() { m_idleHandler(false); }
void Cpu::PImpl::IdleWait() { m_idleHandler(true); }

void Cpu::PImpl::CheckInterrupts() { m_intWanted = m_nmiWanted || (m_irqWanted && !m_i); }
uint8_t Cpu::PImpl::ReadOpcode() { return Read((static_cast<uint32_t>(m_k) << 16) | m_pc++); }

uint16_t Cpu::PImpl::ReadOpcodeWord(bool intCheck)
{
    uint16_t low = ReadOpcode();
    if (intCheck) { CheckInterrupts(); }
    uint16_t high = ReadOpcode();
    return low | (high << 8);
}

uint8_t Cpu::PImpl::GetFlags()
{
    uint8_t val = 0;
    if (m_n) val |= 0x80; if (m_v) val |= 0x40; if (m_mf) val |= 0x20;
    if (m_xf) val |= 0x10; if (m_d) val |= 0x08; if (m_i) val |= 0x04;
    if (m_z) val |= 0x02; if (m_c) val |= 0x01;
    return val;
}

void Cpu::PImpl::SetFlags(uint8_t val)
{
    m_n = (val & 0x80) != 0;
    m_v = (val & 0x40) != 0;
    m_d = (val & 0x08) != 0;
    m_i = (val & 0x04) != 0;
    m_z = (val & 0x02) != 0;
    m_c = (val & 0x01) != 0;

    if (!m_e)
    {
        m_mf = (val & 0x20) != 0;
        m_xf = (val & 0x10) != 0;
    }
    else
    {
        m_mf = true;
        m_xf = true;
    }

    if (m_xf)
    {
        m_x &= 0xff;
        m_y &= 0xff;
    }
}

void Cpu::PImpl::SetZnFlags(uint16_t value, bool isByte)
{
    if (isByte)
    {
        m_z = (value & 0xff) == 0;
        m_n = (value & 0x80) != 0;
    }
    else
    {
        m_z = value == 0;
        m_n = (value & 0x8000) != 0;
    }
}

void Cpu::PImpl::PushByte(uint8_t value)
{
    Write(m_sp, value);
    m_sp--;
    if (m_e) m_sp = (m_sp & 0xff) | 0x100;
}

uint8_t Cpu::PImpl::PullByte()
{
    m_sp++;
    if (m_e) m_sp = (m_sp & 0xff) | 0x100;
    return Read(m_sp);
}

void Cpu::PImpl::PushWord(uint16_t value, bool intCheck)
{
    PushByte(value >> 8);
    if (intCheck) CheckInterrupts();
    PushByte(value & 0xff);
}

uint16_t Cpu::PImpl::PullWord(bool intCheck)
{
    uint8_t low = PullByte();
    if (intCheck) CheckInterrupts();
    return low | (static_cast<uint16_t>(PullByte()) << 8);
}

uint16_t Cpu::PImpl::ReadWord(uint32_t adrL, uint32_t adrH, bool intCheck)
{
    uint16_t low = Read(adrL);
    if (intCheck) CheckInterrupts();
    uint16_t high = Read(adrH);
    return low | (high << 8);
}

void Cpu::PImpl::WriteWord(uint32_t adrL, uint32_t adrH, uint16_t value, bool reversed, bool intCheck)
{
    if (reversed)
    {
        Write(adrH, value >> 8);
        if (intCheck) CheckInterrupts();
        Write(adrL, value & 0xff);
    }
    else
    {
        Write(adrL, value & 0xff);
        if (intCheck) CheckInterrupts();
        Write(adrH, value >> 8);
    }
}

void Cpu::PImpl::DoBranch(bool condition)
{
    if (!condition)
    {
        CheckInterrupts();
    }
    uint8_t value = ReadOpcode();
    if (condition)
    {
        CheckInterrupts();
        Idle();
        m_pc += static_cast<int8_t>(value);
    }
}

void Cpu::PImpl::DoInterrupt()
{
    Idle();
    if (!m_e)
    {
        PushByte(m_k);
    }
    PushWord(m_pc, false);
    uint8_t flags = GetFlags() & 0xEF; // Effacer le drapeau B pour les interruptions matérielles
    PushByte(flags);

    m_i = true;
    m_d = false;
    m_k = 0;
    m_intWanted = false;

    uint32_t vectorL, vectorH;
    if (m_e)
    {
        vectorL = m_nmiWanted ? 0xfffa : 0xfffe;
        vectorH = m_nmiWanted ? 0xfffb : 0xffff;
    }
    else
    {
        vectorL = m_nmiWanted ? 0xffea : 0xffee;
        vectorH = m_nmiWanted ? 0xffeb : 0xffef;
    }

    m_nmiWanted = false;
    m_pc = ReadWord(vectorL, vectorH, false);
}

void Cpu::PImpl::AdrImp() { CheckInterrupts(); if (m_intWanted) { Read((static_cast<uint32_t>(m_k) << 16) | m_pc); } else { Idle(); } }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrImm(bool xFlag) { uint32_t low, high = 0; bool is8bit = (xFlag && m_xf) || (!xFlag && m_mf); low = (static_cast<uint32_t>(m_k) << 16) | m_pc++; if (!is8bit) { high = (static_cast<uint32_t>(m_k) << 16) | m_pc++; } return { low, high }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrDp() { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); uint32_t low = (m_dp + adr) & 0xffff; return { low, (low + 1) & 0xffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrDpx() { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); Idle(); uint32_t low = (m_dp + adr + m_x) & 0xffff; return { low, (low + 1) & 0xffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrDpy() { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); Idle(); uint32_t low = (m_dp + adr + m_y) & 0xffff; return { low, (low + 1) & 0xffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrIdp() { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); uint16_t pointer = ReadWord((m_dp + adr) & 0xffff, (m_dp + adr + 1) & 0xffff, false); uint32_t low = (static_cast<uint32_t>(m_db) << 16) + pointer; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrIdx() { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); Idle(); uint16_t pointer = ReadWord((m_dp + adr + m_x) & 0xffff, (m_dp + adr + m_x + 1) & 0xffff, false); uint32_t low = (static_cast<uint32_t>(m_db) << 16) + pointer; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrIdy(bool write) { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); uint16_t pointer = ReadWord((m_dp + adr) & 0xffff, (m_dp + adr + 1) & 0xffff, false); if (write || !m_xf || ((pointer >> 8) != ((pointer + m_y) >> 8))) Idle(); uint32_t low = ((static_cast<uint32_t>(m_db) << 16) + pointer + m_y) & 0xffffff; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrIdl() { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); uint32_t pointer = ReadWord((m_dp + adr) & 0xffff, (m_dp + adr + 1) & 0xffff, false); pointer |= (static_cast<uint32_t>(Read((m_dp + adr + 2) & 0xffff))) << 16; return { pointer, (pointer + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrIly() { uint8_t adr = ReadOpcode(); if (m_dp & 0xff) Idle(); uint32_t pointer = ReadWord((m_dp + adr) & 0xffff, (m_dp + adr + 1) & 0xffff, false); pointer |= (static_cast<uint32_t>(Read((m_dp + adr + 2) & 0xffff))) << 16; uint32_t low = (pointer + m_y) & 0xffffff; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrSr() { uint8_t adr = ReadOpcode(); Idle(); uint32_t low = (m_sp + adr) & 0xffff; return { low, (low + 1) & 0xffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrIsy() { uint8_t adr = ReadOpcode(); Idle(); uint16_t pointer = ReadWord((m_sp + adr) & 0xffff, (m_sp + adr + 1) & 0xffff, false); Idle(); uint32_t low = ((static_cast<uint32_t>(m_db) << 16) + pointer + m_y) & 0xffffff; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrAbs() { uint16_t adr = ReadOpcodeWord(false); uint32_t low = (static_cast<uint32_t>(m_db) << 16) + adr; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrAbx(bool write) { uint16_t adr = ReadOpcodeWord(false); if (write || !m_xf || ((adr >> 8) != ((adr + m_x) >> 8))) Idle(); uint32_t low = ((static_cast<uint32_t>(m_db) << 16) + adr + m_x) & 0xffffff; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrAby(bool write) { uint16_t adr = ReadOpcodeWord(false); if (write || !m_xf || ((adr >> 8) != ((adr + m_y) >> 8))) Idle(); uint32_t low = ((static_cast<uint32_t>(m_db) << 16) + adr + m_y) & 0xffffff; return { low, (low + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrAbl() { uint32_t adr = ReadOpcodeWord(false); adr |= (static_cast<uint32_t>(ReadOpcode())) << 16; return { adr, (adr + 1) & 0xffffff }; }
std::pair<uint32_t, uint32_t> Cpu::PImpl::AdrAlx() { uint32_t adr = ReadOpcodeWord(false); adr |= (static_cast<uint32_t>(ReadOpcode())) << 16; uint32_t low = (adr + m_x) & 0xffffff; return { low, (low + 1) & 0xffffff }; }

void Cpu::PImpl::And(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); uint8_t value = Read(low); m_a = (m_a & 0xff00) | ((m_a & value) & 0xff); } else { uint16_t value = ReadWord(low, high, true); m_a &= value; } SetZnFlags(m_a, m_mf); }
void Cpu::PImpl::Ora(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); uint8_t value = Read(low); m_a = (m_a & 0xff00) | ((m_a | value) & 0xff); } else { uint16_t value = ReadWord(low, high, true); m_a |= value; } SetZnFlags(m_a, m_mf); }
void Cpu::PImpl::Eor(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); uint8_t value = Read(low); m_a = (m_a & 0xff00) | ((m_a ^ value) & 0xff); } else { uint16_t value = ReadWord(low, high, true); m_a ^= value; } SetZnFlags(m_a, m_mf); }
void Cpu::PImpl::Adc(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); uint8_t value = Read(low); uint16_t result = 0; if (m_d) { result = (m_a & 0xf) + (value & 0xf) + m_c; if (result > 0x9) result = ((result + 0x6) & 0xf) + 0x10; result = (m_a & 0xf0) + (value & 0xf0) + result; } else { result = (m_a & 0xff) + value + m_c; } m_v = !((m_a ^ value) & 0x80) && ((m_a ^ result) & 0x80); if (m_d && result > 0x9f) result += 0x60; m_c = result > 0xff; m_a = (m_a & 0xff00) | (result & 0xff); } else { uint16_t value = ReadWord(low, high, true); uint32_t result = 0; if (m_d) { result = (m_a & 0xf) + (value & 0xf) + m_c; if (result > 0x9) result = ((result + 0x6) & 0xf) + 0x10; result = (m_a & 0xf0) + (value & 0xf0) + result; if (result > 0x9f) result = ((result + 0x60) & 0xff) + 0x100; result = (m_a & 0xf00) + (value & 0xf00) + result; if (result > 0x9ff) result = ((result + 0x600) & 0xfff) + 0x1000; result = (m_a & 0xf000) + (value & 0xf000) + result; } else { result = m_a + value + m_c; } m_v = !((m_a ^ value) & 0x8000) && ((m_a ^ result) & 0x8000); if (m_d && result > 0x9fff) result += 0x6000; m_c = result > 0xffff; m_a = result; } SetZnFlags(m_a, m_mf); }
void Cpu::PImpl::Sbc(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); uint8_t operand = Read(low); uint8_t a_val = m_a & 0xFF; uint16_t result = a_val - operand - (1 - m_c); m_v = ((a_val ^ operand) & (a_val ^ result) & 0x80) != 0; if (m_d) { uint16_t temp = (a_val & 0x0F) - (operand & 0x0F) - (1 - m_c); if (temp & 0x10) { temp -= 6; } temp = (a_val & 0xF0) - (operand & 0xF0) + temp; if (temp & 0x100) { temp -= 0x60; } result = temp; } m_c = (result & 0xFF00) == 0; m_a = (m_a & 0xFF00) | (result & 0xFF); } else { uint16_t operand = ReadWord(low, high, true); uint16_t a_val = m_a; uint32_t result = a_val - operand - (1 - m_c); m_v = ((a_val ^ operand) & (a_val ^ result) & 0x8000) != 0; if (m_d) { uint32_t temp = (a_val & 0x000F) - (operand & 0x000F) - (1 - m_c); if (temp & 0x10) temp -= 6; temp = (a_val & 0x00F0) - (operand & 0x00F0) + temp; if (temp & 0x100) temp -= 0x60; temp = (a_val & 0x0F00) - (operand & 0x0F00) + temp; if (temp & 0x1000) temp -= 0x600; temp = (a_val & 0xF000) - (operand & 0xF000) + temp; if (temp & 0x10000) temp -= 0x6000; result = temp; } m_c = (result & 0xFFFF0000) == 0; m_a = result & 0xFFFF; } SetZnFlags(m_a, m_mf); }
void Cpu::PImpl::Cmp(uint32_t low, uint32_t high) { uint32_t result; if (m_mf) { CheckInterrupts(); uint8_t value = Read(low); result = (m_a & 0xff) - value; m_c = result < 0x100; } else { uint16_t value = ReadWord(low, high, true); result = m_a - value; m_c = result < 0x10000; } SetZnFlags(result, m_mf); }
void Cpu::PImpl::Cpx(uint32_t low, uint32_t high) { uint32_t result; if (m_xf) { CheckInterrupts(); uint8_t value = Read(low); result = (m_x & 0xff) - value; m_c = result < 0x100; } else { uint16_t value = ReadWord(low, high, true); result = m_x - value; m_c = result < 0x10000; } SetZnFlags(result, m_xf); }
void Cpu::PImpl::Cpy(uint32_t low, uint32_t high) { uint32_t result; if (m_xf) { CheckInterrupts(); uint8_t value = Read(low); result = (m_y & 0xff) - value; m_c = result < 0x100; } else { uint16_t value = ReadWord(low, high, true); result = m_y - value; m_c = result < 0x10000; } SetZnFlags(result, m_xf); }
void Cpu::PImpl::Bit(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); uint8_t value = Read(low); m_z = ((m_a & 0xff) & value) == 0; m_n = (value & 0x80) != 0; m_v = (value & 0x40) != 0; } else { uint16_t value = ReadWord(low, high, true); m_z = (m_a & value) == 0; m_n = (value & 0x8000) != 0; m_v = (value & 0x4000) != 0; } }
void Cpu::PImpl::Lda(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); m_a = (m_a & 0xff00) | Read(low); } else { m_a = ReadWord(low, high, true); } SetZnFlags(m_a, m_mf); }
void Cpu::PImpl::Ldx(uint32_t low, uint32_t high) { if (m_xf) { CheckInterrupts(); m_x = Read(low); } else { m_x = ReadWord(low, high, true); } SetZnFlags(m_x, m_xf); }
void Cpu::PImpl::Ldy(uint32_t low, uint32_t high) { if (m_xf) { CheckInterrupts(); m_y = Read(low); } else { m_y = ReadWord(low, high, true); } SetZnFlags(m_y, m_xf); }
void Cpu::PImpl::Sta(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); Write(low, m_a); } else { WriteWord(low, high, m_a, false, true); } }
void Cpu::PImpl::Stx(uint32_t low, uint32_t high) { if (m_xf) { CheckInterrupts(); Write(low, m_x); } else { WriteWord(low, high, m_x, false, true); } }
void Cpu::PImpl::Sty(uint32_t low, uint32_t high) { if (m_xf) { CheckInterrupts(); Write(low, m_y); } else { WriteWord(low, high, m_y, false, true); } }
void Cpu::PImpl::Stz(uint32_t low, uint32_t high) { if (m_mf) { CheckInterrupts(); Write(low, 0); } else { WriteWord(low, high, 0, false, true); } }
void Cpu::PImpl::Ror(uint32_t low, uint32_t high) { bool carry; uint16_t result; if (m_mf) { uint8_t value = Read(low); Idle(); carry = (value & 1) != 0; result = (value >> 1) | (m_c << 7); CheckInterrupts(); Write(low, result); } else { uint16_t value = ReadWord(low, high, false); Idle(); carry = (value & 1) != 0; result = (value >> 1) | (m_c << 15); WriteWord(low, high, result, true, true); } SetZnFlags(result, m_mf); m_c = carry; }
void Cpu::PImpl::Rol(uint32_t low, uint32_t high) { uint32_t result; if (m_mf) { result = (Read(low) << 1) | m_c; Idle(); m_c = (result & 0x100) != 0; CheckInterrupts(); Write(low, result); } else { result = (ReadWord(low, high, false) << 1) | m_c; Idle(); m_c = (result & 0x10000) != 0; WriteWord(low, high, result, true, true); } SetZnFlags(result, m_mf); }
void Cpu::PImpl::Lsr(uint32_t low, uint32_t high) { uint16_t result; if (m_mf) { uint8_t value = Read(low); Idle(); m_c = (value & 1) != 0; result = value >> 1; CheckInterrupts(); Write(low, result); } else { uint16_t value = ReadWord(low, high, false); Idle(); m_c = (value & 1) != 0; result = value >> 1; WriteWord(low, high, result, true, true); } SetZnFlags(result, m_mf); }
void Cpu::PImpl::Asl(uint32_t low, uint32_t high) { uint32_t result; if (m_mf) { result = Read(low) << 1; Idle(); m_c = (result & 0x100) != 0; CheckInterrupts(); Write(low, result); } else { result = ReadWord(low, high, false) << 1; Idle(); m_c = (result & 0x10000) != 0; WriteWord(low, high, result, true, true); } SetZnFlags(result, m_mf); }
void Cpu::PImpl::Inc(uint32_t low, uint32_t high) { uint16_t result; if (m_mf) { result = Read(low) + 1; Idle(); CheckInterrupts(); Write(low, result); } else { result = ReadWord(low, high, false) + 1; Idle(); WriteWord(low, high, result, true, true); } SetZnFlags(result, m_mf); }
void Cpu::PImpl::Dec(uint32_t low, uint32_t high) { uint16_t result; if (m_mf) { result = Read(low) - 1; Idle(); CheckInterrupts(); Write(low, result); } else { result = ReadWord(low, high, false) - 1; Idle(); WriteWord(low, high, result, true, true); } SetZnFlags(result, m_mf); }
void Cpu::PImpl::Tsb(uint32_t low, uint32_t high) { if (m_mf) { uint8_t value = Read(low); Idle(); m_z = ((m_a & 0xff) & value) == 0; CheckInterrupts(); Write(low, value | (m_a & 0xff)); } else { uint16_t value = ReadWord(low, high, false); Idle(); m_z = (m_a & value) == 0; WriteWord(low, high, value | m_a, true, true); } }
void Cpu::PImpl::Trb(uint32_t low, uint32_t high) { if (m_mf) { uint8_t value = Read(low); Idle(); m_z = ((m_a & 0xff) & value) == 0; CheckInterrupts(); Write(low, value & ~(m_a & 0xff)); } else { uint16_t value = ReadWord(low, high, false); Idle(); m_z = (m_a & value) == 0; WriteWord(low, high, value & ~m_a, true, true); } }

void Cpu::PImpl::DoOpcode(uint8_t opcode)
{
    // Ce n'est pas parfait, mais fonctionne sur les jeux les plus connus.
    switch (opcode)
    {
        case 0x00: { ReadOpcode(); if (!m_e) { PushByte(m_k); } PushWord(m_pc, false); PushByte(GetFlags() | 0x10); m_i = true; m_d = false; m_k = 0; uint16_t vectorAddr = m_e ? 0xFFFE : 0xFFE6; m_pc = ReadWord(vectorAddr, vectorAddr + 1, true); break; }
        case 0x01: { auto [low, high] = AdrIdx(); Ora(low, high); break; }
        case 0x02: { ReadOpcode(); if (!m_e) PushByte(m_k); PushWord(m_pc, false); PushByte(GetFlags()); m_i = true; m_d = false; m_k = 0; m_pc = ReadWord(m_e ? 0xfff4 : 0xffe4, m_e ? 0xfff5 : 0xffe5, true); break; }
        case 0x03: { auto [low, high] = AdrSr(); Ora(low, high); break; }
        case 0x04: { auto [low, high] = AdrDp(); Tsb(low, high); break; }
        case 0x05: { auto [low, high] = AdrDp(); Ora(low, high); break; }
        case 0x06: { auto [low, high] = AdrDp(); Asl(low, high); break; }
        case 0x07: { auto [low, high] = AdrIdl(); Ora(low, high); break; }
        case 0x08: { AdrImp(); PushByte(GetFlags()); break; }
        case 0x09: { auto [low, high] = AdrImm(false); Ora(low, high); break; }
        case 0x0a: { AdrImp(); if (m_mf) { m_c = (m_a & 0x80) != 0; m_a = (m_a & 0xff00) | ((m_a << 1) & 0xff); } else { m_c = (m_a & 0x8000) != 0; m_a <<= 1; } SetZnFlags(m_a, m_mf); break; }
        case 0x0b: { AdrImp(); PushWord(m_dp, true); break; }
        case 0x0c: { auto [low, high] = AdrAbs(); Tsb(low, high); break; }
        case 0x0d: { auto [low, high] = AdrAbs(); Ora(low, high); break; }
        case 0x0e: { auto [low, high] = AdrAbs(); Asl(low, high); break; }
        case 0x0f: { auto [low, high] = AdrAbl(); Ora(low, high); break; }
        case 0x10: { DoBranch(!m_n); break; }
        case 0x11: { auto [low, high] = AdrIdy(false); Ora(low, high); break; }
        case 0x12: { auto [low, high] = AdrIdp(); Ora(low, high); break; }
        case 0x13: { auto [low, high] = AdrIsy(); Ora(low, high); break; }
        case 0x14: { auto [low, high] = AdrDp(); Trb(low, high); break; }
        case 0x15: { auto [low, high] = AdrDpx(); Ora(low, high); break; }
        case 0x16: { auto [low, high] = AdrDpx(); Asl(low, high); break; }
        case 0x17: { auto [low, high] = AdrIly(); Ora(low, high); break; }
        case 0x18: { AdrImp(); m_c = false; break; }
        case 0x19: { auto [low, high] = AdrAby(false); Ora(low, high); break; }
        case 0x1a: { AdrImp(); if (m_mf) m_a = (m_a & 0xff00) | ((m_a + 1) & 0xff); else m_a++; SetZnFlags(m_a, m_mf); break; }
        case 0x1b: { AdrImp(); m_sp = m_a; if (m_e) m_sp = (m_sp & 0xff) | 0x100; break; }
        case 0x1c: { auto [low, high] = AdrAbs(); Trb(low, high); break; }
        case 0x1d: { auto [low, high] = AdrAbx(false); Ora(low, high); break; }
        case 0x1e: { auto [low, high] = AdrAbx(true); Asl(low, high); break; }
        case 0x1f: { auto [low, high] = AdrAlx(); Ora(low, high); break; }
        case 0x20: { uint16_t value = ReadOpcodeWord(false); Idle(); PushWord(m_pc - 1, true); m_pc = value; break; }
        case 0x21: { auto [low, high] = AdrIdx(); And(low, high); break; }
        case 0x22: { uint32_t value = ReadOpcodeWord(false); value |= (static_cast<uint32_t>(ReadOpcode()) << 16); PushWord(m_pc - 1, true); m_k = value >> 16; m_pc = value & 0xffff; break; }
        case 0x23: { auto [low, high] = AdrSr(); And(low, high); break; }
        case 0x24: { auto [low, high] = AdrDp(); Bit(low, high); break; }
        case 0x25: { auto [low, high] = AdrDp(); And(low, high); break; }
        case 0x26: { auto [low, high] = AdrDp(); Rol(low, high); break; }
        case 0x27: { auto [low, high] = AdrIdl(); And(low, high); break; }
        case 0x28: { AdrImp(); Idle(); SetFlags(PullByte()); break; }
        case 0x29: { auto [low, high] = AdrImm(false); And(low, high); break; }
        case 0x2a: { AdrImp(); uint32_t result = (m_a << 1) | m_c; if (m_mf) { m_c = (result & 0x100) != 0; m_a = (m_a & 0xff00) | (result & 0xff); } else { m_c = (result & 0x10000) != 0; m_a = result; } SetZnFlags(m_a, m_mf); break; }
        case 0x2b: { AdrImp(); Idle(); m_dp = PullWord(true); SetZnFlags(m_dp, false); break; }
        case 0x2c: { auto [low, high] = AdrAbs(); Bit(low, high); break; }
        case 0x2d: { auto [low, high] = AdrAbs(); And(low, high); break; }
        case 0x2e: { auto [low, high] = AdrAbs(); Rol(low, high); break; }
        case 0x2f: { auto [low, high] = AdrAbl(); And(low, high); break; }
        case 0x30: { DoBranch(m_n); break; }
        case 0x31: { auto [low, high] = AdrIdy(false); And(low, high); break; }
        case 0x32: { auto [low, high] = AdrIdp(); And(low, high); break; }
        case 0x33: { auto [low, high] = AdrIsy(); And(low, high); break; }
        case 0x34: { auto [low, high] = AdrDpx(); Bit(low, high); break; }
        case 0x35: { auto [low, high] = AdrDpx(); And(low, high); break; }
        case 0x36: { auto [low, high] = AdrDpx(); Rol(low, high); break; }
        case 0x37: { auto [low, high] = AdrIly(); And(low, high); break; }
        case 0x38: { AdrImp(); m_c = true; break; }
        case 0x39: { auto [low, high] = AdrAby(false); And(low, high); break; }
        case 0x3a: { AdrImp(); if (m_mf) m_a = (m_a & 0xff00) | ((m_a - 1) & 0xff); else m_a--; SetZnFlags(m_a, m_mf); break; }
        case 0x3b: { AdrImp(); m_a = m_sp; SetZnFlags(m_a, false); break; }
        case 0x3c: { auto [low, high] = AdrAbx(false); Bit(low, high); break; }
        case 0x3d: { auto [low, high] = AdrAbx(false); And(low, high); break; }
        case 0x3e: { auto [low, high] = AdrAbx(true); Rol(low, high); break; }
        case 0x3f: { auto [low, high] = AdrAlx(); And(low, high); break; }
        case 0x40: { AdrImp(); Idle(); SetFlags(PullByte()); m_pc = PullWord(false); if (!m_e) m_k = PullByte(); break; }
        case 0x41: { auto [low, high] = AdrIdx(); Eor(low, high); break; }
        case 0x42: { ReadOpcode(); break; }
        case 0x43: { auto [low, high] = AdrSr(); Eor(low, high); break; }
        case 0x44: { uint8_t dest = ReadOpcode(); uint8_t src = ReadOpcode(); m_db = dest; Write((static_cast<uint32_t>(dest) << 16) | m_y, Read((static_cast<uint32_t>(src) << 16) | m_x)); m_a--; m_x--; m_y--; if (m_a != 0xffff) { m_pc -= 3; } if (m_xf) { m_x &= 0xff; m_y &= 0xff; } Idle(); CheckInterrupts(); Idle(); break; }
        case 0x45: { auto [low, high] = AdrDp(); Eor(low, high); break; }
        case 0x46: { auto [low, high] = AdrDp(); Lsr(low, high); break; }
        case 0x47: { auto [low, high] = AdrIdl(); Eor(low, high); break; }
        case 0x48: { AdrImp(); if (m_mf) PushByte(m_a); else PushWord(m_a, true); break; }
        case 0x49: { auto [low, high] = AdrImm(false); Eor(low, high); break; }
        case 0x4a: { AdrImp(); m_c = (m_a & 1) != 0; if (m_mf) m_a = (m_a & 0xff00) | ((m_a >> 1) & 0x7f); else m_a >>= 1; SetZnFlags(m_a, m_mf); break; }
        case 0x4b: { AdrImp(); PushByte(m_k); break; }
        case 0x4c: { m_pc = ReadOpcodeWord(true); break; }
        case 0x4d: { auto [low, high] = AdrAbs(); Eor(low, high); break; }
        case 0x4e: { auto [low, high] = AdrAbs(); Lsr(low, high); break; }
        case 0x4f: { auto [low, high] = AdrAbl(); Eor(low, high); break; }
        case 0x50: { DoBranch(!m_v); break; }
        case 0x51: { auto [low, high] = AdrIdy(false); Eor(low, high); break; }
        case 0x52: { auto [low, high] = AdrIdp(); Eor(low, high); break; }
        case 0x53: { auto [low, high] = AdrIsy(); Eor(low, high); break; }
        case 0x54: { uint8_t dest = ReadOpcode(); uint8_t src = ReadOpcode(); m_db = dest; Write((static_cast<uint32_t>(dest) << 16) | m_y, Read((static_cast<uint32_t>(src) << 16) | m_x)); m_a--; m_x++; m_y++; if (m_a != 0xffff) { m_pc -= 3; } if (m_xf) { m_x &= 0xff; m_y &= 0xff; } Idle(); CheckInterrupts(); Idle(); break; }
        case 0x55: { auto [low, high] = AdrDpx(); Eor(low, high); break; }
        case 0x56: { auto [low, high] = AdrDpx(); Lsr(low, high); break; }
        case 0x57: { auto [low, high] = AdrIly(); Eor(low, high); break; }
        case 0x58: { AdrImp(); m_i = false; break; }
        case 0x59: { auto [low, high] = AdrAby(false); Eor(low, high); break; }
        case 0x5a: { AdrImp(); if (m_xf) PushByte(m_y); else PushWord(m_y, true); break; }
        case 0x5b: { AdrImp(); m_dp = m_a; SetZnFlags(m_dp, false); break; }
        case 0x5c: { uint16_t value = ReadOpcodeWord(false); CheckInterrupts(); m_k = ReadOpcode(); m_pc = value; break; }
        case 0x5d: { auto [low, high] = AdrAbx(false); Eor(low, high); break; }
        case 0x5e: { auto [low, high] = AdrAbx(true); Lsr(low, high); break; }
        case 0x5f: { auto [low, high] = AdrAlx(); Eor(low, high); break; }
        case 0x60: { Idle(); Idle(); m_pc = PullWord(false) + 1; CheckInterrupts(); Idle(); break; }
        case 0x61: { auto [low, high] = AdrIdx(); Adc(low, high); break; }
        case 0x62: { uint16_t value = ReadOpcodeWord(false); Idle(); PushWord(m_pc + static_cast<int16_t>(value), true); break; }
        case 0x63: { auto [low, high] = AdrSr(); Adc(low, high); break; }
        case 0x64: { auto [low, high] = AdrDp(); Stz(low, high); break; }
        case 0x65: { auto [low, high] = AdrDp(); Adc(low, high); break; }
        case 0x66: { auto [low, high] = AdrDp(); Ror(low, high); break; }
        case 0x67: { auto [low, high] = AdrIdl(); Adc(low, high); break; }
        case 0x68: { AdrImp(); Idle(); if (m_mf) m_a = (m_a & 0xff00) | PullByte(); else m_a = PullWord(true); SetZnFlags(m_a, m_mf); break; }
        case 0x69: { auto [low, high] = AdrImm(false); Adc(low, high); break; }
        case 0x6a: { AdrImp(); bool carry = (m_a & 1) != 0; if (m_mf) m_a = (m_a & 0xff00) | ((m_a >> 1) & 0x7f) | (m_c << 7); else m_a = (m_a >> 1) | (m_c << 15); m_c = carry; SetZnFlags(m_a, m_mf); break; }
        case 0x6b: { Idle(); Idle(); m_pc = PullWord(false) + 1; CheckInterrupts(); m_k = PullByte(); break; }
        case 0x6c: { uint16_t adr = ReadOpcodeWord(false); uint16_t adr_h = (m_e && (adr & 0xff) == 0xff) ? (adr & 0xff00) : (adr + 1); m_pc = ReadWord(adr, adr_h, true); break; }
        case 0x6d: { auto [low, high] = AdrAbs(); Adc(low, high); break; }
        case 0x6e: { auto [low, high] = AdrAbs(); Ror(low, high); break; }
        case 0x6f: { auto [low, high] = AdrAbl(); Adc(low, high); break; }
        case 0x70: { DoBranch(m_v); break; }
        case 0x71: { auto [low, high] = AdrIdy(false); Adc(low, high); break; }
        case 0x72: { auto [low, high] = AdrIdp(); Adc(low, high); break; }
        case 0x73: { auto [low, high] = AdrIsy(); Adc(low, high); break; }
        case 0x74: { auto [low, high] = AdrDpx(); Stz(low, high); break; }
        case 0x75: { auto [low, high] = AdrDpx(); Adc(low, high); break; }
        case 0x76: { auto [low, high] = AdrDpx(); Ror(low, high); break; }
        case 0x77: { auto [low, high] = AdrIly(); Adc(low, high); break; }
        case 0x78: { AdrImp(); m_i = true; break; }
        case 0x79: { auto [low, high] = AdrAby(false); Adc(low, high); break; }
        case 0x7a: { AdrImp(); Idle(); if (m_xf) m_y = PullByte(); else m_y = PullWord(true); SetZnFlags(m_y, m_xf); break; }
        case 0x7b: { AdrImp(); m_a = m_dp; SetZnFlags(m_a, false); break; }
        case 0x7c: { uint16_t adr = ReadOpcodeWord(false); Idle(); uint32_t base_adr = (static_cast<uint32_t>(m_k) << 16) | adr; m_pc = ReadWord(base_adr + m_x, base_adr + m_x + 1, true); break; }
        case 0x7d: { auto [low, high] = AdrAbx(false); Adc(low, high); break; }
        case 0x7e: { auto [low, high] = AdrAbx(true); Ror(low, high); break; }
        case 0x7f: { auto [low, high] = AdrAlx(); Adc(low, high); break; }
        case 0x80: { DoBranch(true); break; }
        case 0x81: { auto [low, high] = AdrIdx(); Sta(low, high); break; }
        case 0x82: { m_pc += static_cast<int16_t>(ReadOpcodeWord(false)); CheckInterrupts(); Idle(); break; }
        case 0x83: { auto [low, high] = AdrSr(); Sta(low, high); break; }
        case 0x84: { auto [low, high] = AdrDp(); Sty(low, high); break; }
        case 0x85: { auto [low, high] = AdrDp(); Sta(low, high); break; }
        case 0x86: { auto [low, high] = AdrDp(); Stx(low, high); break; }
        case 0x87: { auto [low, high] = AdrIdl(); Sta(low, high); break; }
        case 0x88: { AdrImp(); if (m_xf) m_y = (m_y - 1) & 0xff; else m_y--; SetZnFlags(m_y, m_xf); break; }
        case 0x89: { if (m_mf) { CheckInterrupts(); m_z = (m_a & ReadOpcode()) == 0; } else { m_z = (m_a & ReadOpcodeWord(true)) == 0; } break; }
        case 0x8a: { AdrImp(); if (m_mf) m_a = (m_a & 0xff00) | (m_x & 0xff); else m_a = m_x; SetZnFlags(m_a, m_mf); break; }
        case 0x8b: { AdrImp(); PushByte(m_db); break; }
        case 0x8c: { auto [low, high] = AdrAbs(); Sty(low, high); break; }
        case 0x8d: { auto [low, high] = AdrAbs(); Sta(low, high); break; }
        case 0x8e: { auto [low, high] = AdrAbs(); Stx(low, high); break; }
        case 0x8f: { auto [low, high] = AdrAbl(); Sta(low, high); break; }
        case 0x90: { DoBranch(!m_c); break; }
        case 0x91: { auto [low, high] = AdrIdy(true); Sta(low, high); break; }
        case 0x92: { auto [low, high] = AdrIdp(); Sta(low, high); break; }
        case 0x93: { auto [low, high] = AdrIsy(); Sta(low, high); break; }
        case 0x94: { auto [low, high] = AdrDpx(); Sty(low, high); break; }
        case 0x95: { auto [low, high] = AdrDpx(); Sta(low, high); break; }
        case 0x96: { auto [low, high] = AdrDpy(); Stx(low, high); break; }
        case 0x97: { auto [low, high] = AdrIly(); Sta(low, high); break; }
        case 0x98: { AdrImp(); if (m_mf) m_a = (m_a & 0xff00) | (m_y & 0xff); else m_a = m_y; SetZnFlags(m_a, m_mf); break; }
        case 0x99: { auto [low, high] = AdrAby(true); Sta(low, high); break; }
        case 0x9a: { AdrImp(); m_sp = m_e ? ((m_sp & 0xFF00) | (m_x & 0x00FF)) : m_x; break; }
        case 0x9b: { AdrImp(); if (m_xf) m_y = m_x & 0xff; else m_y = m_x; SetZnFlags(m_y, m_xf); break; }
        case 0x9c: { auto [low, high] = AdrAbs(); Stz(low, high); break; }
        case 0x9d: { auto [low, high] = AdrAbx(true); Sta(low, high); break; }
        case 0x9e: { auto [low, high] = AdrAbx(true); Stz(low, high); break; }
        case 0x9f: { auto [low, high] = AdrAlx(); Sta(low, high); break; }
        case 0xa0: { auto [low, high] = AdrImm(true); Ldy(low, high); break; }
        case 0xa1: { auto [low, high] = AdrIdx(); Lda(low, high); break; }
        case 0xa2: { auto [low, high] = AdrImm(true); Ldx(low, high); break; }
        case 0xa3: { auto [low, high] = AdrSr(); Lda(low, high); break; }
        case 0xa4: { auto [low, high] = AdrDp(); Ldy(low, high); break; }
        case 0xa5: { auto [low, high] = AdrDp(); Lda(low, high); break; }
        case 0xa6: { auto [low, high] = AdrDp(); Ldx(low, high); break; }
        case 0xa7: { auto [low, high] = AdrIdl(); Lda(low, high); break; }
        case 0xa8: { AdrImp(); if (m_xf) m_y = m_a & 0xff; else m_y = m_a; SetZnFlags(m_y, m_xf); break; }
        case 0xa9: { auto [low, high] = AdrImm(false); Lda(low, high); break; }
        case 0xaa: { AdrImp(); if (m_xf) m_x = m_a & 0xff; else m_x = m_a; SetZnFlags(m_x, m_xf); break; }
        case 0xab: { AdrImp(); Idle(); m_db = PullByte(); SetZnFlags(m_db, true); break; }
        case 0xac: { auto [low, high] = AdrAbs(); Ldy(low, high); break; }
        case 0xad: { auto [low, high] = AdrAbs(); Lda(low, high); break; }
        case 0xae: { auto [low, high] = AdrAbs(); Ldx(low, high); break; }
        case 0xaf: { auto [low, high] = AdrAbl(); Lda(low, high); break; }
        case 0xb0: { DoBranch(m_c); break; }
        case 0xb1: { auto [low, high] = AdrIdy(false); Lda(low, high); break; }
        case 0xb2: { auto [low, high] = AdrIdp(); Lda(low, high); break; }
        case 0xb3: { auto [low, high] = AdrIsy(); Lda(low, high); break; }
        case 0xb4: { auto [low, high] = AdrDpx(); Ldy(low, high); break; }
        case 0xb5: { auto [low, high] = AdrDpx(); Lda(low, high); break; }
        case 0xb6: { auto [low, high] = AdrDpy(); Ldx(low, high); break; }
        case 0xb7: { auto [low, high] = AdrIly(); Lda(low, high); break; }
        case 0xb8: { AdrImp(); m_v = false; break; }
        case 0xb9: { auto [low, high] = AdrAby(false); Lda(low, high); break; }
        case 0xba: { AdrImp(); if (m_xf) m_x = m_sp & 0xff; else m_x = m_sp; SetZnFlags(m_x, m_xf); break; }
        case 0xbb: { AdrImp(); if (m_xf) m_x = m_y & 0xff; else m_x = m_y; SetZnFlags(m_x, m_xf); break; }
        case 0xbc: { auto [low, high] = AdrAbx(false); Ldy(low, high); break; }
        case 0xbd: { auto [low, high] = AdrAbx(false); Lda(low, high); break; }
        case 0xbe: { auto [low, high] = AdrAby(false); Ldx(low, high); break; }
        case 0xbf: { auto [low, high] = AdrAlx(); Lda(low, high); break; }
        case 0xc0: { auto [low, high] = AdrImm(true); Cpy(low, high); break; }
        case 0xc1: { auto [low, high] = AdrIdx(); Cmp(low, high); break; }
        case 0xc2: { uint8_t valToClear = ReadOpcode(); CheckInterrupts(); valToClear &= (m_e ? (~0x30) : 0xFF); SetFlags(GetFlags() & ~valToClear); Idle(); break; }
        case 0xc3: { auto [low, high] = AdrSr(); Cmp(low, high); break; }
        case 0xc4: { auto [low, high] = AdrDp(); Cpy(low, high); break; }
        case 0xc5: { auto [low, high] = AdrDp(); Cmp(low, high); break; }
        case 0xc6: { auto [low, high] = AdrDp(); Dec(low, high); break; }
        case 0xc7: { auto [low, high] = AdrIdl(); Cmp(low, high); break; }
        case 0xc8: { AdrImp(); if (m_xf) m_y = (m_y + 1) & 0xff; else m_y++; SetZnFlags(m_y, m_xf); break; }
        case 0xc9: { auto [low, high] = AdrImm(false); Cmp(low, high); break; }
        case 0xca: { AdrImp(); if (m_xf) m_x = (m_x - 1) & 0xff; else m_x--; SetZnFlags(m_x, m_xf); break; }
        case 0xcb: { m_waiting = true; Idle(); Idle(); break; }
        case 0xcc: { auto [low, high] = AdrAbs(); Cpy(low, high); break; }
        case 0xcd: { auto [low, high] = AdrAbs(); Cmp(low, high); break; }
        case 0xce: { auto [low, high] = AdrAbs(); Dec(low, high); break; }
        case 0xcf: { auto [low, high] = AdrAbl(); Cmp(low, high); break; }
        case 0xd0: { DoBranch(!m_z); break; }
        case 0xd1: { auto [low, high] = AdrIdy(false); Cmp(low, high); break; }
        case 0xd2: { auto [low, high] = AdrIdp(); Cmp(low, high); break; }
        case 0xd3: { auto [low, high] = AdrIsy(); Cmp(low, high); break; }
        case 0xd4: { auto [low, high] = AdrDp(); PushWord(ReadWord(low, high, false), true); break; }
        case 0xd5: { auto [low, high] = AdrDpx(); Cmp(low, high); break; }
        case 0xd6: { auto [low, high] = AdrDpx(); Dec(low, high); break; }
        case 0xd7: { auto [low, high] = AdrIly(); Cmp(low, high); break; }
        case 0xd8: { AdrImp(); m_d = false; break; }
        case 0xd9: { auto [low, high] = AdrAby(false); Cmp(low, high); break; }
        case 0xda: { AdrImp(); if (m_xf) PushByte(m_x); else PushWord(m_x, true); break; }
        case 0xdb: { m_stopped = true; Idle(); Idle(); break; }
        case 0xdc: { uint16_t adr = ReadOpcodeWord(false); m_pc = ReadWord(adr, (adr + 1) & 0xffff, false); CheckInterrupts(); m_k = Read((adr + 2) & 0xffff); break; }
        case 0xdd: { auto [low, high] = AdrAbx(false); Cmp(low, high); break; }
        case 0xde: { auto [low, high] = AdrAbx(true); Dec(low, high); break; }
        case 0xdf: { auto [low, high] = AdrAlx(); Cmp(low, high); break; }
        case 0xe0: { auto [low, high] = AdrImm(true); Cpx(low, high); break; }
        case 0xe1: { auto [low, high] = AdrIdx(); Sbc(low, high); break; }
        case 0xe2: { uint8_t val = ReadOpcode(); CheckInterrupts(); val &= (m_e ? (~0x30) : 0xFF); SetFlags(GetFlags() | val); Idle(); break; }
        case 0xe3: { auto [low, high] = AdrSr(); Sbc(low, high); break; }
        case 0xe4: { auto [low, high] = AdrDp(); Cpx(low, high); break; }
        case 0xe5: { auto [low, high] = AdrDp(); Sbc(low, high); break; }
        case 0xe6: { auto [low, high] = AdrDp(); Inc(low, high); break; }
        case 0xe7: { auto [low, high] = AdrIdl(); Sbc(low, high); break; }
        case 0xe8: { AdrImp(); if (m_xf) m_x = (m_x + 1) & 0xff; else m_x++; SetZnFlags(m_x, m_xf); break; }
        case 0xe9: { auto [low, high] = AdrImm(false); Sbc(low, high); break; }
        case 0xea: { AdrImp(); break; }
        case 0xeb: { AdrImp(); uint8_t high = m_a >> 8; m_a = (m_a << 8) | high; SetZnFlags(m_a, true); break; }
        case 0xec: { auto [low, high] = AdrAbs(); Cpx(low, high); break; }
        case 0xed: { auto [low, high] = AdrAbs(); Sbc(low, high); break; }
        case 0xee: { auto [low, high] = AdrAbs(); Inc(low, high); break; }
        case 0xef: { auto [low, high] = AdrAbl(); Sbc(low, high); break; }
        case 0xf0: { DoBranch(m_z); break; }
        case 0xf1: { auto [low, high] = AdrIdy(false); Sbc(low, high); break; }
        case 0xf2: { auto [low, high] = AdrIdp(); Sbc(low, high); break; }
        case 0xf3: { auto [low, high] = AdrIsy(); Sbc(low, high); break; }
        case 0xf4: { PushWord(ReadOpcodeWord(false), true); break; }
        case 0xf5: { auto [low, high] = AdrDpx(); Sbc(low, high); break; }
        case 0xf6: { auto [low, high] = AdrDpx(); Inc(low, high); break; }
        case 0xf7: { auto [low, high] = AdrIly(); Sbc(low, high); break; }
        case 0xf8: { AdrImp(); m_d = true; break; }
        case 0xf9: { auto [low, high] = AdrAby(false); Sbc(low, high); break; }
        case 0xfa: { AdrImp(); Idle(); if (m_xf) m_x = PullByte(); else m_x = PullWord(true); SetZnFlags(m_x, m_xf); break; }
        case 0xfb: { AdrImp(); bool old_e = m_e; std::swap(m_c, m_e); (m_e != old_e) ? ((m_e) ? (m_mf = true, m_xf = true, m_sp = (m_sp & 0x00FF) | 0x0100, m_x &= 0x00FF, m_y &= 0x00FF) : (m_mf = false, m_xf = false)) : false; break; }
        case 0xfc: { uint16_t adr = ReadOpcodeWord(false); PushWord(m_pc - 1, false); Idle(); uint32_t base_adr = (static_cast<uint32_t>(m_k) << 16) | adr; m_pc = ReadWord(base_adr + m_x, base_adr + m_x + 1, true); break; }
        case 0xfd: { auto [low, high] = AdrAbx(false); Sbc(low, high); break; }
        case 0xfe: { auto [low, high] = AdrAbx(true); Inc(low, high); break; }
        case 0xff: { auto [low, high] = AdrAlx(); Sbc(low, high); break; }
        default: { AdrImp(); break; }
    }
}
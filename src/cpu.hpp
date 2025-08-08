#pragma once

#include "LocaleInitializer.hpp"

#include <cstdint>
#include <functional>
#include <memory>

#define CPU_API

struct CpuDebugState
{
    uint16_t a, x, y, sp, pc, dp;
    uint8_t k, db;
    bool c, z, v, n, i, d, xf, mf, e;
};


class CPU_API Cpu
{
public:
    using ReadHandler = std::function<uint8_t(uint32_t address)>;
    using WriteHandler = std::function<void(uint32_t address, uint8_t value)>;
    using IdleHandler = std::function<void(bool isWaiting)>;

    Cpu(ReadHandler readHandler, WriteHandler writeHandler, IdleHandler idleHandler);

    ~Cpu();

    Cpu(const Cpu&) = delete;
    Cpu& operator=(const Cpu&) = delete;
    Cpu(Cpu&&) = delete;
    Cpu& operator=(Cpu&&) = delete;

    void Reset(bool hard);

    void RunOpcode();

    void Nmi();

    void SetIrq(bool state);

    CpuDebugState GetDebugState() const;

private:
    struct PImpl;
    std::unique_ptr<PImpl> m_pimpl;
};
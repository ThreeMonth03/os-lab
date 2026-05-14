#include "kernel/arch/x86_64/pic.hpp"

#include "kernel/arch/x86_64/io.hpp"

namespace
{

constexpr uint16_t kPic1Command = 0x20;
constexpr uint16_t kPic1Data = 0x21;
constexpr uint16_t kPic2Command = 0xa0;
constexpr uint16_t kPic2Data = 0xa1;

constexpr uint8_t kPicEoi = 0x20;
constexpr uint8_t kIcw1Init = 0x10;
constexpr uint8_t kIcw1Icw4 = 0x01;
constexpr uint8_t kIcw4_8086 = 0x01;

void write_mask(uint8_t irq_line, bool masked)
{
    const bool slave = irq_line >= 8;
    const uint16_t port = slave ? kPic2Data : kPic1Data;
    const uint8_t bit = static_cast<uint8_t>(1u << (irq_line & 7u));
    uint8_t value = kernel::arch::x86_64::io::inb(port);

    if (masked)
    {
        value = static_cast<uint8_t>(value | bit);
    }
    else
    {
        value = static_cast<uint8_t>(value & ~bit);
    }

    kernel::arch::x86_64::io::outb(port, value);
}

} // namespace

namespace kernel::arch::x86_64::pic
{

void remap()
{
    const uint8_t master_mask = io::inb(kPic1Data);
    const uint8_t slave_mask = io::inb(kPic2Data);

    io::outb(kPic1Command, kIcw1Init | kIcw1Icw4);
    io::wait();
    io::outb(kPic2Command, kIcw1Init | kIcw1Icw4);
    io::wait();

    io::outb(kPic1Data, vector_base);
    io::wait();
    io::outb(kPic2Data, vector_base + 8);
    io::wait();

    io::outb(kPic1Data, 0x04);
    io::wait();
    io::outb(kPic2Data, 0x02);
    io::wait();

    io::outb(kPic1Data, kIcw4_8086);
    io::wait();
    io::outb(kPic2Data, kIcw4_8086);
    io::wait();

    io::outb(kPic1Data, master_mask);
    io::outb(kPic2Data, slave_mask);
}

void mask_all()
{
    io::outb(kPic1Data, 0xff);
    io::outb(kPic2Data, 0xff);
}

void mask(uint8_t irq_line)
{
    if (irq_line >= irq_count)
    {
        return;
    }

    write_mask(irq_line, true);
}

void unmask(uint8_t irq_line)
{
    if (irq_line >= irq_count)
    {
        return;
    }

    if (irq_line >= 8)
    {
        write_mask(2, false);
    }

    write_mask(irq_line, false);
}

void send_eoi(uint8_t irq_line)
{
    if (irq_line >= 8)
    {
        io::outb(kPic2Command, kPicEoi);
    }

    io::outb(kPic1Command, kPicEoi);
}

} // namespace kernel::arch::x86_64::pic

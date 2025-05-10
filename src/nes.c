/*
; The NES computer struct definition, with the appropriate emulated hardware.
*/

#include "util.h"
#include "nes.h"
#include "cpu.h"
#include "ppu.h"
#include "cartridge.h"

// Reset the NES.
void nes_reset(struct nes* computer)
{
    computer->cycles = 0;
    computer->oam_cycle_count = 0;
    computer->oam_page = 0;
    computer->oam_offset = 0;
    computer->oam_executing_dma = false;
    cpu_reset(computer->cpu);
    ppu_reset(computer->ppu);
}

// Read a byte from a given address.
uint8_t nes_read(struct nes* computer, uint16_t address)
{
    // Attempt to read from the cartridge. Usually $4020-$FFFF.
    uint8_t byte;
    if (cartridge_cpu_read(computer->cartridge, address, &byte))
        return byte;

    // $0000-$1FFF: internal RAM.
    else if (0x0000 <= address && address <= 0x1FFF)
        return computer->ram[address & 0x7FF];

    // $2000-$3FFF: NES PPU registers.
    else if (0x2000 <= address && address <= 0x3FFF)
        return ppu_cpu_read(computer->ppu, address & 0x0007);

    // $4016-$4017: controller input.
    // The returned byte is supposed to have input data lines D0-D4, however
    // only D0 is emulated, so the rest is open bus for now (bits 5-7 are also
    // open bus in the actual hardware).
    else if (0x4016 <= address && address <= 0x4017)
    {
        uint8_t index = address - 0x4016;
        uint8_t byte = (computer->controller_cache[index] & 0x80) >> 7;
        computer->controller_cache[index] <<= 1;
        return byte;
    }

    // Open bus: while not accurate, just return zero.
    return 0;
}

// Write a byte to a given address.
void nes_write(struct nes* computer, uint16_t address, uint8_t byte)
{
    // Attempt to write to the cartridge. Usually $0000-$1FFF.
    if (cartridge_cpu_write(computer->cartridge, address, byte))
        return;

    // $0000-$1FFF: internal RAM.
    else if (0x0000 <= address && address <= 0x1FFF)
        computer->ram[address & 0x7FF] = byte;

    // $2000-$3FFF: NES PPU registers.
    else if (0x2000 <= address && address <= 0x3FFF)
        ppu_cpu_write(computer->ppu, address & 0x0007, byte);

    // $4014: NES OAM direct memory access.
    else if (address == 0x4014)
    {
        computer->oam_page = byte;
        computer->oam_offset = 0x00;
        computer->oam_executing_dma = true;
        computer->idle_cycle = computer->cpu->enumerated_cycles & 1;
    }

    // $4016: set the controller port latch bit (the expansion port is not emulated).
    else if (address == 0x4016)
    {
        computer->controller_port_latch = byte & 0b1;
        if (computer->controller_port_latch)
        {
            computer->controller_cache[0] = computer->controllers[0].value;
            computer->controller_cache[1] = computer->controllers[1].value;
        }
    }

    // Open bus.
}

// Clock the NES.
void nes_clock(struct nes* computer)
{
    // For every 4th cycle, clock the PPU.
    if (computer->cycles % 4 == 0)
        ppu_clock(computer->ppu);

    // For every 12th cycle, clock the CPU.
    if (computer->cycles % 12 == 0)
    {
        // Override CPU clocking with OAM DMA if it is currently taking place.
        if (computer->oam_executing_dma)
        {
            // For each odd non-idle cycle (read/write cycles are combined for ease
            // of emulation), copy from the given CPU page:offset to OAM.
            if (computer->oam_cycle_count <= 512 && computer->oam_cycle_count & 1)
            {
                uint8_t byte = nes_read(computer, (computer->oam_page << 8) | computer->oam_offset);
                computer->ppu->oam_byte_pointer[computer->oam_offset++] = byte;
            }

            // Handle the number of executed CPU cycles. This procedure takes 513 cycles,
            // + 1 if the first CPU cycle at the time of instantiation was odd.
            if (computer->idle_cycle)
                computer->idle_cycle = false;
            else
            {
                computer->oam_cycle_count++;
                if (computer->oam_cycle_count > 512)
                {
                    computer->oam_cycle_count = 0;
                    computer->oam_executing_dma = false;
                }
            }
        }
        else
        {
            ///if (computer->cpu->cycles == 0)
            //    cpu_spew(computer->cpu, computer->cpu->pc, stdout);
            cpu_clock(computer->cpu);
        }
    }
    
    // Change the CPU NMI status depending on the PPU's vblank flag status.
    computer->cpu->nmi = !(computer->ppu->ppustatus.vars.vblank_flag && 
        computer->ppu->ppuctrl.vars.vblank_nmi_enable);

    // Increment the total number of cycles.
    computer->cycles++; 
    if (computer->cycles > (UINT64_MAX - 4)) // % 12 and % 4 should both return 0
        computer->cycles = 0;
}

// Create a new NES computer instance.
struct nes* nes_alloc()
{
    // Allocate a new NES computer instance.
    struct nes* computer = safe_malloc(sizeof(struct nes));

    // Create the NES computer's CPU.
    computer->cpu = cpu_alloc();
    cpu_setnes(computer->cpu, computer);

    // Create the NES computer's PPU.
    computer->ppu = ppu_alloc();
    ppu_setnes(computer->ppu, computer);

    // Return the NES computer.
    return computer;
}

// Free a NES computer instance.
void nes_free(struct nes* computer)
{
    if (computer == NULL)
        return;
    ppu_free(computer->ppu);
    cpu_free(computer->cpu);
    free(computer);
}
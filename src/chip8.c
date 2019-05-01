#include "../include/chip8.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "defs.h"

// clang-format off
const u8 chip8_fontmap[80] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};
// clang-format on

#define PC (emu->pc)
#define MEM (emu->mem)
#define SP (emu->sp)
#define V (emu->v)
#define VF (emu->vf)
#define KEYS (emu->keys)
#define SAS (emu->sas)
#define I (emu->i)
#define FB (emu->fb)

void chip8_init(struct chip8 *emu, const u8 *rom, size_t sz)
{
	memset(V, 0, sizeof V);
	I = 0;
	PC = 0x200;
	memset(SAS, 0, sizeof SAS);
	SP = 0;
	KEYS = 0;

	// Font map goes from 0x000 to 0x050.
	// TODO: Does it actually go from 0x50 to 0xA0?
	memcpy(MEM, chip8_fontmap, sizeof chip8_fontmap);
	// Program ROM goes from 0x200 to end.
	if (sz > CHIP8_MAX_ROM_SIZE)
		sz = CHIP8_MAX_ROM_SIZE;
	memcpy(MEM + 0x200, rom, sz);

	memset(FB, 0, sizeof FB);
}

const char *chip8_interrupt_desc(enum chip8_interrupt e)
{
	switch (e) {
	case CHIP8_OK:
		return "No interrupt occurred..";
	case CHIP8_BAD_INSTRUCTION:
		return "Invalid instruction.";
	case CHIP8_OOB_INSTRUCTION:
		return "Tried to read an instruction out of bounds";
	case CHIP8_SAS_UNDERFLOW:
		return "Tried to return from a subroutine but the subroutine address stack was empty.";
	case CHIP8_SAS_OVERFLOW:
		return "Tried to return from a subroutine but the subroutine address stack was full.";
	case CHIP8_NEED_RAND:
		return "The emulator needs a random number to complete the current cycle.";
	case CHIP8_GFX_OOB:
		return "The sprite drawing instruction tried to read from memory out of bounds.";
	case CHIP8_GFX_WRITE:
		return "The graphics buffer was updated.";
	case CHIP8_BAD_KEY:
		return "Tried to set a key with a code greater than 0xF.";
	case CHIP8_NEED_KEY:
		return "The emulator is waiting for a keypress.";
	case CHIP8_DELAY_TIMER_WRITE:
		return "The delay timer has been written to.";
	case CHIP8_NEED_DELAY_TIMER:
		return "The emulator needs the elapsed time since the last write to the delay timer to complete the current cycle.";
	case CHIP8_SOUND_TIMER_WRITE:
		return "The sound timer has been written to.";
	case CHIP8_BAD_FONT_DIGIT:
		return "Tried to get a font digit greater than 0xF";
	case CHIP8_OOB_BCD:
		return "Tried to write a binary coded decimal out of bounds.";
	case CHIP8_OOB_REGWRITE:
		return "Tried to write the contents of the V registers out of bounds.";
	case CHIP8_OOB_REGREAD:
		return "Tried to read data into the V registers out of bounds.";
	}
	return NULL;
}

enum chip8_interrupt chip8_cycle(struct chip8 *emu)
{
	if (PC == 0xFFF)
		return CHIP8_OOB_INSTRUCTION;

	const u16 ins = MEM[PC] << 8 | MEM[PC + 1];

	switch ((ins & 0xF000) >> 12) {
	case 0x0:
		switch (ins) {
		case 0x00E0: // clear screen.
			memset(FB, 0, sizeof(FB));
			return CHIP8_OK;
		case 0x00EE:
			if (SP == 0)
				return CHIP8_SAS_UNDERFLOW;
			PC = SAS[--SP] + 1;
			return CHIP8_OK;
		}
		break;
	case 0x1: // Jump to address at NNN
		PC = ins & 0x0FFF;
		return CHIP8_OK;
	case 0x2: // Execute subroutine at NNN
		if (SP == 15)
			return CHIP8_SAS_OVERFLOW;
		SAS[SP++] = PC;
		PC = ins & 0x0FFF;
		return CHIP8_OK;
	case 0x3: // Skip next instruction if VX is equal to NN.
		PC += V[(ins & 0x0F00) >> 8] == (ins & 0x00FF) ? 4 : 2;
		return CHIP8_OK;
	case 0x4: // Skip next instruction if VX is not equal to NN.
		PC += V[(ins & 0x0F00) >> 8] != (ins & 0x00FF) ? 4 : 2;
		return CHIP8_OK;
	case 0x5: // Skip next instruction if VX is equal to VY.
		if ((ins & 0x000F) == 0)
			break;
		PC += V[(ins & 0x0F00) >> 8] == V[(ins & 0x00F0) >> 4] ? 4 : 2;
		return CHIP8_OK;
	case 0x6: // Store NN in VX
		V[(ins & 0x0F00) >> 8] = ins & 0x00FF;
		PC += 2;
		return CHIP8_OK;
	case 0x7: // Add NN to VX
		V[(ins & 0x0F00) >> 8] += ins & 0x00FF;
		PC += 2;
		return CHIP8_OK;
	case 0x8: { // Do something to VX with VY
		u8 *const vx = V + ((ins & 0x0F00) >> 8);
		u8 *const vy = V + ((ins & 0x00F0) >> 4);
		switch (ins & 0x000F) {
		case 0x0:
			*vx = *vy;
			break;
		case 0x1:
			*vx |= *vy;
			break;
		case 0x2:
			*vx &= *vy;
			break;
		case 0x3:
			*vx ^= *vy;
			break;
		case 0x4: {
			const u8 x = *vx;
			*vx += *vy;
			VF = *vx < x ? 1 : 0;
			break;
		}
		case 0x5: {
			const u8 x = *vx;
			*vx -= *vy;
			VF = *vx > x ? 1 : 0;
			break;
		}
		case 0x6:
			VF = *vy & 1;
			*vx = *vy >> 1;
			break;
		case 0x7: {
			const u8 x = *vx;
			*vx = *vy - *vx;
			VF = *vx > x ? 1 : 0;
			break;
		}
		case 0xF:
			VF = *vy & 0x80;
			*vx = *vy << 1;
			break;
		default:
			return CHIP8_BAD_INSTRUCTION;
		}
		PC += 2;
		return CHIP8_OK;
	}
	case 0x9: // Skip next instruction if VX and VY are not equal
		if ((ins & 0x000F) != 0)
			break;
		PC += V[(ins & 0x0F00) >> 8] == V[(ins & 0x00F0) >> 4] ? 4 : 2;
		return CHIP8_OK;
	case 0xA: // Store address NNN in register I
		I = ins & 0x0FFF;
		PC += 2;
		return CHIP8_OK;
	case 0xB: // Jump to address NNN + V0
		PC = (ins & 0x0FFF) + V[0];
		return CHIP8_OK;
	case 0xC: // Set VX to a random number
		return CHIP8_NEED_RAND;
	case 0xD: { // Draw sprite at pos VX, VY
		// TODO: handle the gfx_wrapping option.
		const u8 xpos = V[(ins & 0x0F00) >> 8];
		const u8 ypos = V[(ins & 0x00F0) >> 4];
		const u8 nrows = ins & 0x000F;

		VF = 0;
		if (I + nrows - 1 > 0xFFF)
			return CHIP8_GFX_OOB;

		// Loop through the sprite bytes
		for (int y = 0; y < nrows; y++) {
			const u8 byte = MEM[I + y];
			// Loop through each bit in the byte
			for (int x = 0; x < 8; x++) {
				const u8 bit = byte & 0x80 >> x;
				// If the set bit in fb turned off, set vf to 1.
				if (!(FB[(y + ypos) % 32][(x + xpos) % 64] ^= bit))
					VF = 1;
			}
		}
		PC += 2;
		return CHIP8_GFX_WRITE;
	}
	case 0xE: { // Skip next instruction based on keypad state
		const u8 k = V[(ins & 0x0F00) >> 8];
		switch (ins & 0x00FF) {
		case 0x9E:
			if (k > 0xF)
				return CHIP8_BAD_KEY;
			PC += KEYS & 1 << k ? 4 : 2;
			return CHIP8_OK;
		case 0xA1:
			if (k > 0xF)
				return CHIP8_BAD_KEY;
			PC += KEYS & 1 << k ? 2 : 4;
			return CHIP8_OK;
		}
		break;
	}
	case 0xF: {
		const u8 x = (ins & 0x0F00) >> 8;
		u8 *const vx = V + x;
		switch (ins & 0x00FF) {
		case 0x07:
			return CHIP8_NEED_DELAY_TIMER;
		case 0x0A:
			return CHIP8_NEED_KEY;
		case 0x15:
			PC += 2;
			return CHIP8_DELAY_TIMER_WRITE;
		case 0x18:
			PC += 2;
			return CHIP8_SOUND_TIMER_WRITE;
		case 0x1E: // Add VX to I.
			I += *vx;
			PC += 2;
			return CHIP8_OK;
		case 0x29: // Set I to the font digit in VX.
			if (*vx > 0xF)
				return CHIP8_BAD_FONT_DIGIT;
			// Font digits are 5 pixels tall.
			I = *vx * 5;
			PC += 2;
			return CHIP8_OK;
		case 0x33: // Write binary coded decimal (BCD) at I reg.
			if (I + 2 > 0xFFF)
				return CHIP8_OOB_BCD;
			MEM[I] = (*vx / 100) % 10;
			MEM[I + 1] = (*vx / 10) % 10;
			MEM[I + 2] = *vx % 10;
			PC += 2;
			return CHIP8_OK;
		case 0x55: // Write V registers to memory at reg I.
			if (I + x + 1 > 0xFFF)
				return CHIP8_OOB_REGWRITE;
			for (int i = 0; i < x + 1; i++) MEM[I + i] = V[i];
			I += x + 1;
			PC += 2;
			return CHIP8_OK;
		case 0x65: // Read memory at I into V registers
			if (I + x + 1 > 0xFFF)
				return CHIP8_OOB_REGREAD;
			for (int i = 0; i < x + 1; i++) V[i] = MEM[I + i];
			I += x + 1;
			PC += 2;
			return CHIP8_OK;
		}
		break;
	}
	}

	return CHIP8_BAD_INSTRUCTION;
}

void chip8_supply_rand(struct chip8 *emu, u8 r)
{
	V[MEM[PC] & 0x0F] = r & MEM[PC + 1];
	PC += 2;
}

void chip8_supply_key(struct chip8 *emu, u8 k)
{
	assert(k < 16);
	V[MEM[PC] & 0x0F] = k;
}

void chip8_supply_delay_timer(struct chip8 *emu, u8 t)
{
	V[MEM[PC] & 0x0F] = t;
	PC += 2;
}
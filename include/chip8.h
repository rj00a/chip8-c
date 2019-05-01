#pragma once

#include <stdint.h>

// Holds the state of the chip8 emulator
struct chip8 {
	// Set to true to enable the wrapping of sprites around the screen
	bool gfx_wrapping;
	// General purpose 8 bit registers.
	union {
		uint8_t v[16];
		struct {
			uint8_t
				v0, v1, v2, v3,
				v4, v5, v6, v7,
				v8, v9, va, vb,
				vc, vd, ve, vf;
		};
	};
	// The image register. Holds addresses for graphics.
	uint16_t i;
	// Program counter
	// The instruction we are currently on.
	uint16_t pc;
	// Subroutine address stack
	// An address is added to the stack when a subroutine is called.
	uint16_t sas[16];
	// The index into the SAS where the next address will go.
	// SAS is empty if sp is zero.
	uint8_t sp;
	// The keypad
	// Has 16 key states represented by each bit.
	uint16_t keys;
	// Main memory
	uint8_t mem[0x1000];
	// The frame buffer
	// Each byte indicates the state of a pixel.
	// We could pack it 8x tighter but that would be annoying.
	uint8_t fb[64][32];
};

#define CHIP8_MAX_ROM_SIZE 0xE00

// Initializes a chip8 emulator from a ROM.
void chip8_init(struct chip8 *, const uint8_t *rom, size_t sz);

enum chip8_interrupt {
	CHIP8_OK,
	CHIP8_BAD_INSTRUCTION,
	CHIP8_OOB_INSTRUCTION,
	CHIP8_SAS_UNDERFLOW,
	CHIP8_SAS_OVERFLOW,
	CHIP8_NEED_RAND,
	CHIP8_GFX_OOB,
	CHIP8_GFX_WRITE,
	CHIP8_BAD_KEY,
	CHIP8_NEED_KEY,
	CHIP8_DELAY_TIMER_WRITE,
	CHIP8_NEED_DELAY_TIMER,
	CHIP8_SOUND_TIMER_WRITE,
	CHIP8_BAD_FONT_DIGIT,
	CHIP8_OOB_BCD,
	CHIP8_OOB_REGWRITE,
	CHIP8_OOB_REGREAD,
};

// Returns a string description of a chip8_interrupt
const char *chip8_interrupt_desc(enum chip8_interrupt);

// Advances the state of the emulator by one instruction.
enum chip8_interrupt chip8_cycle(struct chip8 *);

// Call this after chip8_cycle returns CHIP8_NEED_RAND.
// 'r' is a random number in the range [0, 255].
void chip8_supply_rand(struct chip8 *, uint8_t r);

// Call this after chip8_cycle returns CHIP8_NEED_KEY.
// 'k' is a value in the range [0, 15] which corresponds to a key on the keypad.
void chip8_supply_key(struct chip8 *, uint8_t k);

// Call this after chip8_cycle returns CHIP8_NEED_DELAY_TIMER
// 't' should be a count (60hz) of the time elapsed since CHIP8_DELAY_TIMER_WRITE was returned.
void chip8_supply_delay_timer(struct chip8 *, uint8_t t);


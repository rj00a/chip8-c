#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Holds the state of the chip8 emulator
struct chip8 {
	// Set to true to enable the wrapping of sprites around the screen
	bool gfx_wrapping;
	// General purpose 8 bit registers.
	uint8_t v[16];
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
	// This buffer is written to when the system writes to the delay timer.
	// Then, cycle returns with CHIP8_DELAY_TIMER_WRITE.
	// When the system needs to read from the delay timer, cycle returns with CHIP8_NEED_DELAY_TIMER
	// Then the frontend calls chip8_supply_delay_timer.
	uint8_t dtimer_buf;
	// This buffer is written to when the system writes to the sound timer.
	// Then, cycle returns with CHIP8_SOUND_TIMER_WRITE.
	uint8_t stimer_buf;
	// Main memory
	uint8_t mem[0x1000];
	// The frame buffer
	// Each byte indicates the state of a pixel.
	// We could pack it 8x tighter but that would be annoying to work with.
	uint8_t fb[64][32];
};

#define CHIP8_MAX_ROM_SIZE 0xE00

// Initializes a chip8 emulator from a ROM.
void chip8_init(struct chip8 *, const uint8_t *rom, size_t sz);

enum chip8_interrupt {
	CHIP8_OK,
	CHIP8_BAD_INSTRUCTION,
	CHIP8_OOB_INSTRUCTION,
	CHIP8_GFX_CLEAR,
	CHIP8_SAS_UNDERFLOW,
	CHIP8_SAS_OVERFLOW,
	CHIP8_NEED_RAND,
	CHIP8_GFX_OOB,
	CHIP8_GFX_DRAW,
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

// Advances the state of the emulator by one instruction.
enum chip8_interrupt chip8_cycle(struct chip8 *);

// Returns a string description of a chip8_interrupt
const char *chip8_interrupt_desc(enum chip8_interrupt);

// Call this after chip8_cycle returns CHIP8_NEED_RAND.
// 'r' is a random number in the range [0, 255].
void chip8_supply_rand(struct chip8 *, uint8_t r);

// Call this after chip8_cycle returns CHIP8_NEED_KEY.
// 'k' is a value in the range [0, 15] which corresponds to a key on the keypad.
void chip8_supply_key(struct chip8 *, uint8_t k);

// Call this after chip8_cycle return CHIP8_NEED_DELAY_TIMER
// 't' is the value of the last write to the delay timer minus the number of ticks that have passed since.
// a single tick is 1/60th of a second
// if the number of ticks that have passed is greater than the initial delay timer value, then 't' should be zero.
void chip8_supply_delay_timer(struct chip8 *, uint8_t t);


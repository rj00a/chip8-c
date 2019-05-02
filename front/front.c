#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "../src/defs.h"
#include "chip8.h"

static u8 rom_buffer[CHIP8_MAX_ROM_SIZE];

static struct chip8 chip8;

/* TODO: These command line options should be available:
	--help
	--width
	--height
	--cycle-delay
	--wrapping-gfx
	--disable-audio
	--disable-message-boxes
	--foreground
	--background
*/

static void report(
	Uint32 flags,
	const char *restrict title,
	const char *restrict format,
	...)
{
	va_list args;
	va_start(args, format);
	char buffer[200];
	vsnprintf(buffer, sizeof buffer, format, args);

	fputs((const char *)&buffer, stderr);
	putc('\n', stderr);

	if (SDL_ShowSimpleMessageBox(flags, title, (const char *)&buffer, NULL)) {
		fprintf(
			stderr, "Error showing simple message box: %s\n", SDL_GetError());
		exit(-1);
	}
}

u8 keypad_from_sdl_scancode(SDL_Scancode k)
{
	switch (k) {
	case SDL_SCANCODE_1:
		return 0;
	case SDL_SCANCODE_2:
		return 1;
	case SDL_SCANCODE_3:
		return 2;
	case SDL_SCANCODE_4:
		return 3;
	case SDL_SCANCODE_Q:
		return 4;
	case SDL_SCANCODE_W:
		return 5;
	case SDL_SCANCODE_E:
		return 6;
	case SDL_SCANCODE_R:
		return 7;
	case SDL_SCANCODE_A:
		return 8;
	case SDL_SCANCODE_S:
		return 9;
	case SDL_SCANCODE_D:
		return 10;
	case SDL_SCANCODE_F:
		return 11;
	case SDL_SCANCODE_Z:
		return 12;
	case SDL_SCANCODE_X:
		return 13;
	case SDL_SCANCODE_C:
		return 14;
	case SDL_SCANCODE_V:
		return 15;
	default:
		return 0xFF;
	}
}

#define RET_ERROR(...) \
	do { \
		report(SDL_MESSAGEBOX_ERROR, __VA_ARGS__); \
		return __LINE__; \
	} while (0)

int front_main(int argc, char *argv[])
{
	if (argc == 1)
		RET_ERROR("Argument error", "Must specify a ROM to read.");

	if (argc > 2)
		RET_ERROR("Argument error", "Only one argument expected.");

	const char *rompath = argv[1];

	FILE *rom = fopen(rompath, "rb");

	if (!rom)
		RET_ERROR("IO Error", "Failed to open ROM file: %s", rompath);

	size_t rom_size = fread(rom_buffer, 1, CHIP8_MAX_ROM_SIZE, rom);

	if (ferror(rom))
		RET_ERROR("IO Error", "Error reading ROM file.");

	if (!feof(rom))
		report(
			SDL_MESSAGEBOX_WARNING,
			"IO Warning",
			"ROM file was truncated because it exceeded the maximum ROM size.");

	fclose(rom);

	// Initialize SDL
	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO))
		RET_ERROR("SDL Error", "Failed to initialize SDL: %s", SDL_GetError());

	SDL_DisplayMode display_mode;
	if (SDL_GetCurrentDisplayMode(0, &display_mode))
		RET_ERROR(
			"SDL Error",
			"Failed to query current display mode: %s",
			SDL_GetError());

	// If the width and height were not provided, make the window 1/4th the
	// screen area.
	int win_width = display_mode.w / 2;
	int win_height = display_mode.h / 2;

	SDL_Window *window = SDL_CreateWindow(
		rompath,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		win_width,
		win_height,
		SDL_WINDOW_RESIZABLE);

	if (!window)
		RET_ERROR("SDL Error", "Failed to create window: %s", SDL_GetError());

	SDL_Renderer *renderer =
		SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	if (!renderer)
		RET_ERROR("SDL Error", "Failed to create renderer: %s", SDL_GetError());

	if (SDL_RenderSetScale(renderer, win_width / 64.f, win_height / 32.f))
		RET_ERROR(
			"SDL Error", "Failed to set render scale: %s", SDL_GetError());

	u32 mulberry32 = time(NULL);
	u32 delay_timer = SDL_GetTicks();
	u32 sound_timer = delay_timer;
	bool need_keypress = false;

	chip8_init(&chip8, rom_buffer, rom_size);
	while (1) {
		// TODO: configurable cycle delay.
		SDL_Delay(17); // 16.666ms == 60hz

		// TODO: decrease sound timer here.

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				return 0;
			case SDL_KEYUP:
			case SDL_KEYDOWN: {
				u8 k = keypad_from_sdl_scancode(event.key.keysym.scancode);
				if (k == 0xFF)
					break; // Irrelevant key
				if (event.type == SDL_KEYDOWN)
					chip8.keys |= 1 << k;
				else
					chip8.keys &= ~(1 << k);

				if (need_keypress) {
					chip8_supply_key(&chip8, k);
					need_keypress = false;
				}
				break;
			}
				// TODO: handle resize events
			}
		}

		if (need_keypress)
			continue;

		// CYCLE
		const enum chip8_interrupt in = chip8_cycle(&chip8);
		switch (in) {
		case CHIP8_OK:
			break;
		case CHIP8_NEED_RAND: {
			// mulberry32 PRNG algorithm
			u32 z = (mulberry32 += 0x6D2B79F5UL);
			z = (z ^ (z >> 15)) * (z | 1UL);
			z ^= z + (z ^ (z >> 7)) * (z | 61UL);
			z = z ^ (z >> 14);
			// Use top 8 bits of z
			chip8_supply_rand(&chip8, z >> 24);
			break;
		}
		case CHIP8_NEED_KEY:
			need_keypress = true;
			break;
		case CHIP8_GFX_UPDATE:
			// Background color
			if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF))
				RET_ERROR(
					"SDL Error",
					"Failed to set draw color: %s",
					SDL_GetError());
			if (SDL_RenderClear(renderer))
				RET_ERROR(
					"SDL Error",
					"Failed to clear renderer: %s",
					SDL_GetError());
			// Foreground color
			if (SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF))
				RET_ERROR(
					"SDL Error",
					"Failed to set draw color: %s",
					SDL_GetError());
			for (int y = 0; y < 32; y++)
				for (int x = 0; x < 64; x++)
					if (chip8.fb[x][y])
						if (SDL_RenderDrawPoint(renderer, x, y))
							RET_ERROR(
								"SDL Error",
								"Failed to draw point at %d, %d: %s",
								x,
								y,
								SDL_GetError());
			SDL_RenderPresent(renderer);
			break;
		case CHIP8_DELAY_TIMER_WRITE:
			delay_timer = SDL_GetTicks();
			break;
		case CHIP8_NEED_DELAY_TIMER: {
			// ticks_since is expected to overflow if more than 255 ticks pass.
			// One tick is 1/60th of a second.
			u8 ticks_since =
				round((SDL_GetTicks() - delay_timer) / (1000.0 / 60));
			chip8_supply_delay_timer(&chip8, ticks_since);
			break;
		}
		case CHIP8_SOUND_TIMER_WRITE:
			// TODO: set sound timer (SDL_AddTimer?)
			break;
		case CHIP8_BAD_INSTRUCTION:
			RET_ERROR(
				"Invalid Instruction",
				"Invalid instruction encountered: 0x%04" PRIX16,
				chip8.mem[chip8.pc]);
		default:
			RET_ERROR("Unrecoverable Interrupt", chip8_interrupt_desc(in));
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

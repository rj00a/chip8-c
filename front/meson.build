chip8front_src = files([
	'main.c'])

# TODO: add fallback for sdl2. Test on windows.

chip8font = executable('chip8', chip8front_src,
	dependencies : [dependency('sdl2'), libchip8])


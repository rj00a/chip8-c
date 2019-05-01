project('chip8', 'c',
	license: 'Zlib',
	default_options: ['c_std=c11']
)

libchip8 = declare_dependency(
	link_with: library('chip8', 'src/chip8.c'),
	include_directories: include_directories('include')
)

#subdir('test')

subdir('front')


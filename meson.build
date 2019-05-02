project('chip8', 'c',
	license: 'Zlib',
	default_options: ['c_std=c11'])

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)

libchip8 = declare_dependency(
	link_with: library('chip8', 'src/chip8.c'),
	dependencies : m_dep,
	include_directories: include_directories('include'))

#subdir('test')

subdir('front')


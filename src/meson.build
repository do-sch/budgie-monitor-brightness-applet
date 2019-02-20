dependencies = [
	dependency('gtk+-3.0', version: '>=3.18'),
	dependency('budgie-1.0', version: '>=2'),
	dependency('glib-2.0', version: '>=2.46.0'),
	dependency('libpeas-1.0', version: '>=1.8.0'),
	dependency('ddcutil', version: '>=0.9.0'),
	dependency('threads')
]

sources = [
	'applet.h',
	'applet.c',
	'plugin.h',
	'plugin.c',
	'displaymanager.h',
	'displaymanager.c',
	'ddcwrapper.h',
	'ddcwrapper.c',
	'internaldisplayhandler.h',
	'internaldisplayhandler.c'
]

shared_library(
	'budgiemonitorbrightnessapplet', sources, 
	dependencies: dependencies,
	install: true,
	install_dir: lib_install_dir
)

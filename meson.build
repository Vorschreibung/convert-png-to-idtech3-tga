project('convert-png-to-idtech3-tga', 'c',
  version: '0.1.0',
  default_options: ['c_std=c99', 'warning_level=2'])

libpng_dep = dependency('libpng', required: true)

executable('convert-png-to-idtech3-tga',
  'src/convert-png-to-idtech3-tga.c',
  dependencies: [libpng_dep],
  install: true)

# When linking statically, we have to use the C++ linker
# to bring in the C++ runtime.
link_language = get_option('default_library') == 'static' ? 'cpp' : 'c'

test('plainc', executable('ctest', 'ctest.c',
                          dependencies: capypdf_dep,
                          link_language: link_language))

test('cppwrapper', executable('cpptest', 'cpptest.cpp', override_options: ['cpp_std=c++20'], dependencies: capypdf_dep))

test('syntax', find_program('syntaxchecks.py'))

test('format', find_program('formatcheck.py'))

if get_option('default_library') == 'shared'
  test('Python tests', find_program('capypdftests.py'), args: ['-v'])
endif

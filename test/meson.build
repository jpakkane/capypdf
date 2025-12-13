test('plainc', executable('ctest', 'ctest.c', dependencies: capypdf_dep))

test('cppwrapper', executable('cpptest', 'cpptest.cpp', override_options: ['cpp_std=c++20'], dependencies: capypdf_dep))

test('syntax', find_program('syntaxchecks.py'))

test('format', find_program('formatcheck.py'))

if get_option('default_library') == 'shared'
  test('Python tests', find_program('capypdftests.py'), args: ['-v'])
endif

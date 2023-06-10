test('plainc', executable('ctest', 'ctest.c', dependencies: capypdf_dep))

test('Python tests',
  find_program('capypdftests.py'),
  args: meson.current_source_dir() / '..')

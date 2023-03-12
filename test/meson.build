test('plainc', executable('ctest', 'ctest.c', dependencies: a4pdf_dep))

test('Python tests',
  find_program('a4pdftests.py'),
  args: meson.current_source_dir() / '..')

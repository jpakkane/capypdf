capydeps = [png_dep, jpeg_dep, lcms_dep, tiff_dep, zlib_dep, freetype_dep]

cpp_args = ['-DBUILDING_CAPYPDF']

if get_option('fuzzing')
  cpp_args += '-DCAPY_FUZZING'
endif

capypdf_lib = shared_library('capypdf',
  'pdfcommon.cpp',
  'generator.cpp',
  'drawcontext.cpp',
  'document.cpp',
  'imagefileops.cpp',
  'utils.cpp',
  'colorconverter.cpp',
  'fontsubsetter.cpp',
  'ft_subsetter.cpp',
  'cffsubsetter.cpp',
  'bitfiddling.cpp',
  'capi.cpp',
  'errorhandling.cpp',
  'pdfwriter.cpp',
  'objectformatter.cpp',
  'commandstreamformatter.cpp',
  'mmapper.cpp',
  include_directories: [pubinc],
  #link_args: ['-static-libstdc++'],
  cpp_args: cpp_args,
  dependencies: capydeps,
  gnu_symbol_visibility: 'inlineshidden',
  version: version,
  soversion: soversion,
  install: true,
  cpp_pch: 'pch/capypch.hpp',
)

capypdf_dep = declare_dependency(include_directories: pubinc,
  link_with: capypdf_lib,
)

capypdf_internal_dep = declare_dependency(include_directories: [pubinc, '.'],
  objects: capypdf_lib.extract_all_objects(recursive: false),
  dependencies: capydeps,
)

if get_option('fuzzing')
    executable('fontfuzzer', 'fontfuzz.cpp',
    dependencies: capypdf_internal_dep)
else
    executable('fontgen',
      'fontgen.cpp',
      dependencies: capypdf_internal_dep,
    )

    executable('cfftest',
      'cfftest.cpp',
      dependencies: capypdf_internal_dep,
    )

    executable('formtest', 'formtest.cpp',
      dependencies: [capypdf_internal_dep]
    )

    executable('fembedtest', 'fembedtest.cpp',
      dependencies: [capypdf_internal_dep]
    )
endif

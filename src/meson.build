a4deps = [fmt_dep, png_dep, jpeg_dep, lcms_dep, tiff_dep, zlib_dep, freetype_dep]

capypdf_lib = shared_library('capypdf',
  'pdfgen.cpp',
  'pdfdrawcontext.cpp',
  'pdfdocument.cpp',
  'imageops.cpp',
  'utils.cpp',
  'pdfcolorconverter.cpp',
  'fontsubsetter.cpp',
  'ft_subsetter.cpp',
  'pdfcapi.cpp',
  'errorhandling.cpp',
  include_directories: [pubinc],
  #link_args: ['-static-libstdc++'],
  cpp_args: ['-DBUILDING_CAPYPDF'],
  dependencies: a4deps,
  gnu_symbol_visibility: 'inlineshidden',
  version: version,
  soversion: soversion,
  install: true,
)

capypdf_dep = declare_dependency(include_directories: pubinc,
  link_with: capypdf_lib,
)

capypdf_internal_dep = declare_dependency(include_directories: pubinc,
  objects: capypdf_lib.extract_all_objects(recursive: false),
  dependencies: a4deps,
)

executable('pdfgen',
  'main.cpp',
  dependencies: capypdf_internal_dep,
)

executable('covergen',
  'cover.cpp',
  dependencies: capypdf_internal_dep,
)

executable('fontgen',
  'fontgen.cpp',
  dependencies: capypdf_internal_dep,
)

executable('gstategen',
  'gstategen.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('pathgen',
  'pathgen.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('patterngen',
  'patterngen.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('labtest', 'labtest.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('imagetest', 'imagetest.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('outlinetest', 'outlinetest.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('formxobj', 'formxobj.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('formtest', 'formtest.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('subtypetest', 'subtype.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('fembedtest', 'fembedtest.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('presentation', 'presentation.cpp',
  dependencies: [capypdf_internal_dep]
)

executable('loremipsum', 'loremipsum.cpp',
  dependencies: [capypdf_internal_dep]
)

if gtk_dep.found()
  executable('pdfviewer',
    'pdfviewer.cpp',
    'pdfparser.cpp',
    dependencies: [zlib_dep, gtk_dep, fmt_dep])
endif

executable('fonttester', 'fonttester.cpp',
  dependencies: [capypdf_internal_dep])

if tiff_dep.found()
  executable('tifftest', 'tifftest.cpp', dependencies: tiff_dep)
endif

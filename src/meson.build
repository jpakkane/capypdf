a4deps = [fmt_dep, png_dep, jpeg_dep, lcms_dep, tiff_dep, zlib_dep, freetype_dep]

a4pdf_lib = shared_library('a4pdf',
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
  cpp_args: ['-DBUILDING_A4PDF'],
  dependencies: a4deps,
  gnu_symbol_visibility: 'inlineshidden',
  version: version,
  soversion: soversion,
  install: true,
)

a4pdf_dep = declare_dependency(include_directories: pubinc,
  link_with: a4pdf_lib,
)

a4pdf_internal_dep = declare_dependency(include_directories: pubinc,
  objects: a4pdf_lib.extract_all_objects(recursive: false),
  dependencies: a4deps,
)

executable('pdfgen',
  'main.cpp',
  dependencies: a4pdf_internal_dep,
)

executable('covergen',
  'cover.cpp',
  dependencies: a4pdf_internal_dep,
)

executable('fontgen',
  'fontgen.cpp',
  dependencies: a4pdf_internal_dep,
)

executable('gstategen',
  'gstategen.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('pathgen',
  'pathgen.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('patterngen',
  'patterngen.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('labtest', 'labtest.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('imagetest', 'imagetest.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('outlinetest', 'outlinetest.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('icctest', 'icctest.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('formxobj', 'formxobj.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('formtest', 'formtest.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('subtypetest', 'subtype.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('fembedtest', 'fembedtest.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('presentation', 'presentation.cpp',
  dependencies: [a4pdf_internal_dep]
)

executable('loremipsum', 'loremipsum.cpp',
  dependencies: [a4pdf_internal_dep]
)

if gtk_dep.found()
  executable('pdfviewer',
    'pdfviewer.cpp',
    'pdfparser.cpp',
    dependencies: [zlib_dep, gtk_dep, fmt_dep])
endif

executable('fonttester', 'fonttester.cpp',
  dependencies: [a4pdf_internal_dep])

if tiff_dep.found()
  executable('tifftest', 'tifftest.cpp', dependencies: tiff_dep)
endif

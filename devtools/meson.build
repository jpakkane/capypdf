gtk_dep = dependency('gtk4', required: false)
hb_dep = dependency('harfbuzz', required: false)

if gtk_dep.found()
  executable('pdfviewer',
    'pdfviewer.cpp',
    'pdfparser.cpp',
    dependencies: [zlib_dep, gtk_dep],
    install: true
  )
endif

if hb_dep.found()
  executable('hbtest',
    'hbtest.cpp',
    dependencies: [capypdf_internal_dep, hb_dep],
    install: true
  )
endif

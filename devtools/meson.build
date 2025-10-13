gtk_dep = dependency('gtk4', required: false)
hb_dep = dependency('harfbuzz', required: false)

if gtk_dep.found()
  executable('pdfviewer',
    'pdfviewer.cpp',
    'pdfparser.cpp',
    dependencies: [zlib_dep, gtk_dep])
endif

if hb_dep.found()
  executable('hbtest',
    'hbtest.cpp',
    dependencies: [capypdf_internal_dep, hb_dep])
endif

executable('im2pdf', 'im2pdf.cpp',
  dependencies: capypdf_dep)

executable('text2pdf', 'text2pdf.cpp',
  dependencies: capypdf_dep)

executable('subsettool', 'subsettool.cpp',
  dependencies: [capypdf_internal_dep])

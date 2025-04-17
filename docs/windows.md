# Building and running CapyPDF on Windows
Mostly notes to self, but these may be helpful to others.

## Dependencies
- Install VS dev tools
- From command prompt (not PS), activate VS dev tools, e.g. `"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"` (x64) 
    - Note: by default, VsDevCmd targets x86 - this doesn't work as it raises errors related to `int32_t`/`uint32_t` conflict
- Install meson (`pip install meson`)
- Install Ninja (`winget install Ninja-build.Ninja`)
- Install GhostScript (for running tests)
- Configure with `meson setup --vsenv builddir`
- Build with `meson compile -C builddir`

## Running CapyPDF programs on Windows
- Windows needs the DLL to be in the path or the same directory as the exe, so either copy the DLL or edit with the absolute path to DLL, i.e:
  `set PATH=C:\path\to\src\;%PATH%`
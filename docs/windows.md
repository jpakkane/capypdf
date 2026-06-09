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

# Installing CapyPDF using Dependency Management Tools

If you are using a dependency management tool,
you can also install CapyPDF using the respective package manager,
instead of compiling it from source code.

## Conan

You can download and install CapyPDF using the [Conan](https://conan.io/) dependency manager:

```
conan install -r conancenter --requires="capypdf/[*]" --build=missing
```

The CapyPDF package in Conan Center is maintained by
[ConanCenterIndex](https://github.com/conan-io/conan-center-index) community.
If the version is out of date or the package does not work,
please create an issue or pull request on the [Conan Center Index repository](https://github.com/conan-io/conan-center-index).

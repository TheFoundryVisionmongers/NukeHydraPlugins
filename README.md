# Nuke to Hydra

This project implements a Hydra scene delegate for Nuke's 3D system, as well as
ops to facilitate rendering 3D scenes using available Hydra render delegates.

## Building

The Nuke Hydra plug-in has the following dependencies:
- Compiler requirements are as for all Nuke NDK plugins.
- Nuke (13.0v3 onwards)
- USD (https://github.com/PixarAnimationStudios/USD/releases/tag/v20.08) (20.08 onwards)
- Boost (https://boost.org) (1.70.0 onwards)
- CMake (https://cmake.org/documentation/) (3.13 onwards)
- [Recommended build tool] Ninja (https://ninja-build.org/) (1.8.2 onwards)
- [Only if building with unit tests] Catch2 (https://github.com/catchorg/Catch2)
  - The unit tests are disabled by default. Enable them by setting the CMake option BUILD\_HDNUKE\_UNITTESTS=ON.


After that, building should be pretty simple:

```
cmake \
-GNinja \
-D CMAKE_INSTALL_PREFIX=<YOUR_INSTALL_PREFIX> \
-D Nuke_ROOT=<YOUR_NUKE_INSTALL_ROOT> \
-D PXR_USD_LOCATION=<YOUR_USD_INSTALL_ROOT> \
<SOURCE_DIR>

ninja install
```

If you need to specify where some of the USD dependencies are located, or if
they are not being found properly, try setting some of these CMake variables as
needed:

- `TBB_ROOT_DIR`
- `BOOST_ROOT`
- `CMAKE_PREFIX_PATH`

## Initial Contributors

Foundry thanks Luma Pictures and Nathan Rusch for their many contributions during the initiation of this project.

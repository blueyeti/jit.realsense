# jit.realsense
RealSense external for Max/MSP.

## Current Instructions to Build in MacOS

### Older librealsense2 and Max SDK

- Branch [`rs_working_older`](https://github.com/smokhov/jit.realsense/tree/rs_working_older)
- Will need to download separately and unzip as `max-sdk` directory
under `jit.realsense/` as the submodule pointer in the original repo was broken:
   - https://github.com/jcelerier/max-sdk/tree/7c42a22a38a5edf3d69c2c1c1b77f6fd5462d174

### Latest librealsense2 and Max SDK

- Branch [`rs2020`](https://github.com/smokhov/jit.realsense/tree/rs2020)
- newest Max SDK and librealsense2 for (2)
  - branched off `rs_working_older` but with submodules pointing to the latest commits in `master` of both librealsense2 and Max SDK.

### Generic steps after pulling one of the above branches

- Clone and pull those branches in (for convenience can use
separate clones for (1) or (2), if using GitHub Desktop or similar,
make sure submodules are pulled, if not do in Terminal: `git submodule update --init --recursive`
- In Terminal in `jit.realsense/`
  ```#!bash
  mkdir build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Debug
  cmake --build .
    (wait for awhile while librealsense2)
  mkdir -p Debug/jit.realsense.mxo/Contents/MacOS/
  cp jit.realsense Debug/jit.realsense.mxo/Contents/MacOS/
  cp ../Info.plist ../PkgInfo Debug/jit.realsense.mxo/Contents/
  cp ../patch/example.maxpat Debug/
  cd Debug
  open example.maxpat
  ```
  - The older branch may need the `librealsense2_Poller_bsd.patch` applied to `Poller_bsd.cpp` in librealsense2 to compile; newer shouldn't need it.
  -  (to restart from scratch, remove that build directory and redo the steps)

### Using Xcode to build/debug jit.realsense project:

- Install Xcode via AppStore
- Install Homebrew package manager.
- Install the following packages via Homebrew:

```
brew install libusb pkg-config
brew install homebrew/core/glfw3
brew install cmake
```
- In jit.realsense directory, run:

```
mkdir build && cd build
sudo xcode-select --reset
cmake .. -DBUILD_EXAMPLES=true -DBUILD_WITH_OPENMP=false -DHWM_OVER_XU=false -G Xcode
open jit.realsense.xcodeproj
```

Before building, please add the following directory path in the build settings of jit.realsense project:

```
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers
```

Header Search Paths: (recursive)
System Header Search Paths: (recursive)


Click the build button - with finger crossed.. (You might need to comment out the line that throw sqlite3 fastmath tag error.)

After Successfully built the project - please check target file jit.realsense/build/Debug/jit.realsense is prensented (File size @21MB)

Then in jit.realsense/build directory, follow the similar steps in CMake build:
```
mkdir -p Debug/jit.realsense.mxo/Contents/MacOS/
cd Debug
cp ../../Info.plist ../../PkgInfo jit.realsense.mxo/Contents/
cp ../../patch/example.maxpat .
cp jit.realsense jit.realsense.mxo/Contents/MacOS/ && open example.maxpat
```

Whenever you make changes to jit.realsense.cpp or other related header or cpp files, rebuild only the jit.realsense project (about few seconds). Then quit Max 8 and run
```
cp jit.realsense jit.realsense.mxo/Contents/MacOS/ && open example.maxpat
```

The changes should be applied in the externals. The response cycle should be acceptable now.


## Original Instructions from upstream

### Building for Windows

#### Required tools: 

* Last tested with Visual Studio 2017
* CMake 3.8+

#### Building

* Open a shell and run : 

```
mkdir build
cd build
cmake path/to/jit.realsense
cmake --build . --config Release
```

This will build the 32-bit version; for building the 64 bit version, run cmake with the following command instead: 

```
cmake -G "Visual Studio 15 2017 Win64" path/to/jit.realsense
```

The `jit.realsense.mxe` and `jit.realsense.mxe64` externals will be in the `Release` folder created during build.

### Building for macOS

#### Required tools:

* Last tested with Xcode 8
* CMake >= 3.8

#### Building

* Open a shell and run : 

```
mkdir build
cd build
cmake path/to/jit.realsense -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Note that the build is optimized for CPUs with at least AVX instructions (basically, any Sandy Bridge i7 or more recent); 
this can be tweaked on CMakeLists.txt:9. The produced external will be a fat library working with both 32 and 64 bit Max.

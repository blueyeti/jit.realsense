# jit.realsense
RealSense external for Max/MSP

## TL;DR Release Building

### Building for Windows

#### Required tools:

* Last tested with Visual Studio 2017
* CMake 3.11+

#### Building:

* Open a shell and run:

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
* CMake >= 3.11

#### Building:

* Open a shell, go to folder where you want to download the repo and run (from robtherich or any current forks):

```
git clone --recursive https://github.com/robtherich/jit.realsense.git
mkdir build
cd build
```

##### if you want to create a release build without opening XCode:

```
cmake path/to/jit.realsense -DCMAKE_BUILD_TYPE=Release
```

for some strange reason you might have error messages. just do it again:
```
cmake path/to/jit.realsense -DCMAKE_BUILD_TYPE=Release
```

```
cmake --build .
```

Note that the build is optimized for CPUs with at least AVX instructions (basically, any Sandy Bridge i7 or more recent);
this can be tweaked in `CMakeLists.txt`. The produced external will be a fat library working with both 32 and 64 bit Max.

##### If you want to build and debug with XCode, run instead

```
cmake -G Xcode ..
```
for some strange reason you might have error messages. just do it again:
```
cmake -G Xcode ..
```

 for some reason the xcode project Debug configuration has optimizations enabled, so if you want to debug you'll have to manually disable the optimizations setting (Optimization Level setting)

## Current Instructions to Build Debug Mode on MacOS

This is a long version for developers.

### Using Xcode to build/debug jit.realsense project:

- Install Xcode via AppStore
- Install Homebrew package manager
- Install the following packages via Homebrew:

```
brew install libusb pkg-config
brew install homebrew/core/glfw3
brew install cmake
```

(glfw3 is needed if RealSense examples are built for debug/preview to see if the camera works standalone)

- Download/Clone `jit.realsense` project:
```
git clone https://github.com/blueyeti/jit.realsense.git
```

- Initialize required git submodules (namely `librealsense` and `maxsdk`)
```
cd jit.realsense
git submodule update --init --recursive
```

- Setup for build in `jit.realsense` folder

```
mkdir build && cd build
sudo xcode-select --reset
cmake .. -DBUILD_EXAMPLES=true -DBUILD_WITH_OPENMP=false -DHWM_OVER_XU=false -G Xcode
open jit.realsense.xcodeproj
```

- In XCode, add System Header Path to Build Settings of `jit.realsense` target (cmake should already have it).

```
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers
```

- Click the build button or press `Command + B`

(You might need to comment out the line that throw sqlite3 fastmath tag error.)

After successfully building the project - check target file jit.realsense/build/Debug/jit.realsense is present (file size is about 21MB)

- Prepare the .mxo package for Max 8 to load:

In `jit.realsense/build` folder
```
mkdir -p Debug/jit.realsense.mxo/Contents/MacOS/
cd Debug
cp ../../Info.plist ../../PkgInfo jit.realsense.mxo/Contents/
cp ../../patch/example.maxpat .
cp jit.realsense jit.realsense.mxo/Contents/MacOS/ && open example.maxpat
```

(Info.plist and PkgInfo should normally be take care of automatically by newer cmake)

Whenever you make changes to `jit.realsense.cpp` or other related header or cpp files, rebuild only the `jit.realsense` project (about few seconds). Then quit Max 8 and run
```
cp jit.realsense jit.realsense.mxo/Contents/MacOS/ && open example.maxpat
```

The changes then should be applied and visibile in the external.

### Generic steps summary

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
  - The older MacOS may need the `librealsense2_Poller_bsd.patch` applied to `Poller_bsd.cpp` in librealsense2 to compile; newer shouldn't need it.
  -  (to restart from scratch, remove that build directory and redo the steps)

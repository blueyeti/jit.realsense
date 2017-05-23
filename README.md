# jit.realsense
RealSense external for Max/MSP

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

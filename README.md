# libsimplecurl++
A simple, yet elegant way to add [cURL](https://curl.haxx.se/libcurl/) support to your C++ code

## Depedencies
libsimplecurl++ depends on two libraries: [libcurl](https://curl.haxx.se/libcurl/) of course, and [libevent2](https://libevent.org/).

The dependency on libevent2 is necessary to provide multi-connection functionality. Currently, libsimplecurl++ runs on a single asynchronous thread that uses the cURL multi-interface to provide a socket-driven event loop. libevent2 helps facilitate this.

## Building
Included with this repository is a CMakeLists.txt for building the shared and static library on it's own, or for integration with a CMake project. This is completely optional, and libsimplecurl++ can be built without it by including the code files and linking to libcurl and libevent2.

To build with cmake, simply execute the following commands:
```
mkdir build && cd build
cmake ..
make
```

This will generate the shared (libsimplecurl++) and static (libsimplecurl++_static) libraries.

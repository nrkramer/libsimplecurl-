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

## Usage
Once you've got a copy of the library you need to include the header in your code to access the CURLHelper class. The class itself is only accessable through the factory method `CURLHelper::get()` that returns a reference to the CURLHelper singleton (which is initialized when the function is called the first time):
```cpp
#include "cURLHelper.hpp"

...
CURLHelper curl = CURLHelper.get();
...
```
Once you have a reference to the singleton, it is easy to immediately begin a request (and store the Connection for later use):
```cpp
CURLHelper::Connection connectionToGoogle = curl.addRequest("https://www.google.com/");
// OR
CURLHelper::Connection connectionToGoogle = CURLHelper.get().addRequest("https://www.google.com/");
```
This request is completed _asynchronously_. Included in the library is a blocking mechanism for convenience:
```cpp
CURLHelper.get().blockForAllTransfers();
```
Retrieval of data is simple:
```cpp
std::string& dataAsString = *connectionToGoogle.data;
char* rawData = dataAsString.data();
```
Additionally, the API supports completion callbacks via an overload of the `addRequest` function:
```cpp
CURLHelper.get().addRequest("https://www.google.com/", [](const std::string& data) {
    std::cout << "Literally Google: " << data << std::endl;
});
```
And that's pretty much all there is to it!

## Future work
- Upload functionality
- Configuration of header data
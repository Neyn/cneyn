# CNeyn
```CNeyn``` is a fast Http library written in C. You can checkout [C++](https://github.com/Neyn/neynxx) and [Python](https://github.com/Neyn/neynpy) interfaces too. Some of the features:

+ Fast
+ Very Easy to Use
+ No External Dependencies

Since the project is new there are some limitations:

+ ```Windows``` platform isn't supported for now.
+ Partially implements [HTTP/1.1](https://tools.ietf.org/html/rfc7230) for now.
+ Uses some new features of ```Linux``` kernel so version 4.5 and above is supported for now.

# Build & Install
You have two options:
+ Building and installing the library system-wide.
+ Adding to your ```CMake``` project as a subdirectory.

You can download the latest release and extract (or you can clone the repository but the latest release is more stable).

## System-Wide Installation
You can do these in the ```cneyn``` directory:

``` shell
mkdir build && cd build
cmake -DCNEYN_BUILD_TESTS=OFF -DCNEYN_INSTALL_LIB=ON  -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
sudo cmake --install .
```

Then you can use it with various build systems. Here is an example of ```CMake```:

``` cmake
find_package(cneyn REQUIRED)
add_executable(myexec main.cpp)
target_link_libraries(myexec cneyn::cneyn)
```

## CMake Subdirectory
You can put the ```cneyn``` directory inside your project's directory and add it as a subdirectory. For example:

``` cmake
add_subdirectory(cneyn)
add_executable(myexec main.c)
target_link_libraries(myexec ${CNEYN_LIBRARIES})
target_include_directories(myexec PUBLIC ${CNEYN_INCLUDE_DIRS})
```

# Usage

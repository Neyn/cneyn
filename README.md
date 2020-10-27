# Table of Contents
- [Table of Contents](#table-of-contents)
- [CNeyn](#cneyn)
- [Build & Install](#build--install)
  - [System-Wide Installation](#system-wide-installation)
  - [CMake Subdirectory](#cmake-subdirectory)
- [Usage](#usage)
  - [Configuring](#configuring)
  - [Handling Requests](#handling-requests)
  - [Creating Server](#creating-server)
  - [Running the Server](#running-the-server)
- [Contributing](#contributing)
- [License](#license)

# CNeyn
```CNeyn``` is a fast Http library written in C. You can checkout [C++](https://github.com/Neyn/neynxx) and [Python](https://github.com/Neyn/neynpy) interfaces too. Some of the features:

+ Fast
+ Very Easy to Use
+ No External Dependencies

Since the project is new there are some limitations:

+ ```Windows``` platform isn't supported for now.
+ Partially implements [HTTP/1.1](https://tools.ietf.org/html/rfc7230) for now.
+ Uses some new features of ```Linux``` kernel so version 4.5 and above kernel is supported for now.

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
You can include the library like this:

``` c
#include <cneyn/cneyn.h>
```

## Configuring
Here are the options you can set:

+ Port: port number of the server.
+ IP Version: it can be ```neyn_address_ipv4``` or ```neyn_address_ipv6```.
+ Timeout: server in milliseconds. set 0 for no timeout.
+ Limit: request size limit in bytes. set 0 for no limit.
+ Threads: number of threads.
+ Address: address string of the server.

Example:

``` c
struct neyn_config config;
config.port = 8081;
config.ipvn = neyn_address_ipv4;
config.timeout = 0;
config.limit = 0;
config.threads = 1;
config.address = "0.0.0.0";
```

## Handling Requests
Handling of input requests is done by passing a function to the server. This function takes ```neyn_request``` and ```neyn_response``` and a user-defined data. Request struct has ```port, address, major, minor, method, path, body, header``` fields. Response struct has ```status, header, body``` fields.

Example:

``` c
void handler(struct neyn_request *request, struct neyn_response *response, void *data)
{
  response->body.len = 5;
  response->body.ptr = "Hello";
  neyn_response_write(response);
}
```

## Creating Server
You need to create a server object and pass the created configuration and handler function to it. Here is how:

``` c
struct neyn_server server;
neyn_server_init(&server);
server.handler = handler;
server.config = config;
```

## Running the Server
You must call ```neyn_server_run``` function and pass the server object to it. If you want the function to be non-blocking you can pass 0 as the last arguement and 1 otherwise. You can stop a non-blocking server by calling ```neyn_server_kill``` on it. 

Example:
``` c
enum neyn_error error = neyn_server_run(&server, 1);
printf("%i\n", error);
```

# Contributing
You can report bugs, ask questions and request features on [issues page](../../issues). Pull requests are not accepted right now.

# License
This library is licensed under BSD 3-Clause permissive license. You can read it [here](LICENSE).
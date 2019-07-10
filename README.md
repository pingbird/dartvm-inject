# dartvm-inject - Injects code into a running DartVM process.

## Requirements

* CMake
* lldb
* Dart SDK of target

## Building

1. Update CMakeLists.txt with compiler and lldb / Dart SDK include paths
2. `cmake . && make`

## Usage

At the moment the only command supported is `spawn` which spawns a dart script as a new isolate in the target process, usage:
```
./dart-inject -p <pid> spawn <dart file>
```
You may need to tell liblldb where to find lldb-server:
```
export LLDB_DEBUGSERVER_PATH=/usr/lib/llvm-6.0/bin/lldb-server
```

See `./dart-inject --help` for more information.

## How it works

`dart-inject` uses liblldb to call `dlopen` on the target process to load `libantman.so` which uses Dart SDK internals to compile and execute a dart file in a new isolate.

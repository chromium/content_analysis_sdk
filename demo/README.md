# Google Chrome Content Analysis Connector Agent SDK Demo

This directory holds the Google Chrome Content Analysis Connector Agent SDK Demo.
It contains an example of how to use the SDK.

<<<<<<< HEAD
Build instructions are available in the main project `README.md`.
=======
## Pre-requisites

The following must be installed on the computer before building the demo:

- [git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git) version 2.33 or later.
- [cmake](https://cmake.org/install/) version 3.23 or later.
- A C++ compiler toolchain for your platform.
- On linux, the `realpath` shell tool, available in the `coreutils` package.
  In Debian-based distributions use `sudo apt intall coreutils`.
  In Red Hat distributions use `sudo yum install coreutils`.

### Google Protocol Buffers

This SDK depends on Google Protocol Buffers version 3.18 or later.  It may be
installed from Google's [download page](https://developers.google.com/protocol-buffers/docs/downloads#release-packages)
for your developement platform.  It may also be installed using a package
manager.

This demo uses the Microsoft [vcpkg](https://github.com/microsoft/vcpkg)
package manager to install protobuf.  vcpkg is available on all supported
platforms.

## Build

First get things ready by installing required dependencies:
```
$SDK_DIR/demo/prepare_build <build-dir>
```
where `<build-dir>` is the path to a directory where the demo will be built.
This directory must not already exist. If building within the source tree, it is
recommended to use a directory named `build` (eg: `/demo/build`). Any output
within the `build/` directory will be ignored by version control.

`prepare_build` performs the following steps:
1. Downloads the vcpkg package manager.
2. Downloads and builds the Google Protocol Buffers library.
3. Generates the headers and sources for the content analysis protobuf message.
4. Creates build files for your specific platform.

To build the demo run the command `cmake --build <build-dir>`.

To build the protocol buffer targets run the command `cmake --build <build-dir> --target protos`
>>>>>>> 570d9c1 (update READMEs and add some comments)

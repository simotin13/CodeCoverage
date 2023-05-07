[日本語のREADMEはこちら](https://github.com/simotin13/CodeCoverage/blob/main/README-ja.md)

# What is this ?
This repository is a code coverage tool that utilizes [Pin](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html), a Dynamic Binary Instrumentation (DBI) engine published by Intel. It operates as a Pin plugin.

# Supported Platforms
- GNU/Linux

# How to use (Quick Start)
First, download this repository.
```
git clone git@github.com:simotin13/CodeCoverage.git
```

Pin tool itself is required to run the code.
You can download the Pin tool from the Intel Pin website.

Scripts for downloading, building, and running examples are included in this repository.
Please start by running `00_setup.sh.`

```
cd CodeCoverage/
./00_setup.sh
```

In `00_setup.sh`, Pin 3.27 is downloaded and extracted to the same directory as this repository. Then, the source code in this repository is built.

To use the latest version of the pin, download it from the site as needed.

## Running the coverage tool
After downloading and building, please run 01_run_example.sh.

In `01_run_example.sh`, a C program included in examples/c_function_call is executed as a coverage measurement target for this coverage tool.

After running, the coverage measurement results are output as an HTML file in the `report` folder.


# Build and Execution Commands
## Build
To build this tool, please execute:

```
make PIN_ROOT=../pin-3.27-98718-gbeaa5d51e-gcc-linux
```

The convention when building Pin tools is to specify the directory path of the PIN tool with PIN_ROOT.

# Execution
To run this tool, execute the following command:

```
../pin-3.27-98718-gbeaa5d51e-gcc-linux/pin -t ./obj-intel64/CodeCoverage.so -- <target_module_path> <target_args...>
```

where <target_module_path> and <target_args...> are the path and any arguments for the target module you want to measure code coverage for.

# Note
This coverage tool uses DWARF debugging information to obtain line number information.
Pin 3.27 supports DWARF4 as debugging information. When building the application for which you want to measure coverage, please build it with the `-g` and `-gdwarf-4` options.

例).
```
gcc -g -gdwarf-4 main.c
```

# File access example for Allied Vision Alvium C cameras

## Usage
The example can be used to read and write user defined data to the camera. For each operation a separate executable is provided.

# Get Sources
```
git clone https://github.com/dlangenkamp-avt/alvium_file_access_example.git
git submodule update --init --recursive
```

## Building
The example can be build using cmake and gcc.
```
mkdir build
cmake -S . -B build
cmake --build build
```

### Reading data
```
file_access_read [-o file_name] alvium-subdev
```
The received data is written to stdout by default. By using the option "-o" the data can also be saved to a file.

### Writing data
```
file_access_write alvium-subdev data-file
```
The write example currently only allows uploading data from a file. If you upload a new file the previously saved data will be completely overridden.
# File access example for Allied Vision Alvium C cameras

### Usage
The example can be used to read and write user defined data to the camera. For each operation a separate executable is provided.

### Get Sources
```
git clone https://github.com/dlangenkamp-avt/alvium_file_access_example.git
git submodule update --init --recursive
```

### Building
The example can be build using cmake and gcc.
```
mkdir build
cmake -S . -B build
cmake --build build
```

### Writing data
```
file_access_write <alvium_subdev_index> data-file
```
The write example currently only allows uploading data from a file. If you upload a new file the previously saved data will be completely overridden.

### Reading data
```
file_access_read [-o file_name] <alvium_subdev_index>
```
The received data is written to stdout by default. By using the option "-o" the data can also be saved to a file.

### Usage Example

First of all check the alvium camera <alvium_subdev_index> using:

```
media-ctl -p
```
This will give you the following output:

```
- entity 51: alvium-csi2 3-003c (1 pad, 1 link)
             type V4L2 subdev subtype Sensor flags 0
             device node name /dev/v4l-subdev6
        pad0: Source
                [fmt:UYVY8_1X16/640x480@1/10 field:none colorspace:srgb xfer:srgb ycbcr:601 quantization:full-range
                 crop.bounds:(0,0)/2064x1544
                 crop:(0,0)/640x480]
                -> "max96717 6-0040":0 [ENABLED,IMMUTABLE]
```

By this log can be seen that:

```
<alvium_subdev_index> = 6
```

Let's create an "hellow world" file to be written later into the camera:
```
echo "hello world!" > test
```

Write this into the camera using:
```
./file_access_write 6 test
```
Some logs:

```
File length: 13
Written: 100% (13/13)
```

Let's read back from the camera using:
```
./file_access_read 6
```
Read cmd give you the following output back:
```
hello world!
```
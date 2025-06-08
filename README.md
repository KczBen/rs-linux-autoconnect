# A shim library to automatically route JACK connections

To get Rocksmith 2014 running on Linux, follow [this guide.](https://github.com/theNizo/linux_rocksmith) If you're using PipeWire, your game likely will not automatically connect the input and output nodes. This library solves that.

## How it works
The library hooks into the `jack_activate()` function to make JACK connections to Rocksmith 2014.

## How to use
Download the latest version from the Releases tab. Place it anywhere on your system, and put `LD_PRELOAD=/path/to/librsshim.so %command%` in the Steam launch arguments for Rocksmith 2014.

The shim must be loaded with `LD_PRELOAD` to have priority over the real `jack_activate` function.

By default, it automatically selects the system-wide default PipeWire audio input and output devices. This selection can be overwritten with environment variables:

`RS_PHYS_INPUT_L` Left port of the input device\
`RS_PHYS_INPUT_R` Right port of the input device\
`RS_PHYS_OUTPUT_L` Left port of the output device\
`RS_PHYS_OUTPUT_R` Right port of the output device

To help you figure out the names of your audio ports, a log file `jack_shim_debug.log` is generated your current working directory. For Rocksmith on Steam, this will be the game directory `steamapps/common/Rocksmith2014`

The shim can be used with any* JACK application, provided it has only two input and two output ports. The shim automatically tries to discover the name of the client it is running on. In case that fails, the following additional environment variables are available:

**The below variables do not need to be set for Rocksmith 2014.**

`RS_GAME_INPUT_L` Left input port of the application\
`RS_GAME_INPUT_R` Right input port of the application\
`RS_GAME_OUTPUT_L` Left output port of the application\
`RS_GAME_OUTPUT_R` Right output port of the application

*Because this uses `LD_PRELOAD`, there are three criteria for this to work:
* The application must not statically link `libjack`
* The application must not qualify the `libjack` import path
* The application must call `jack_activate()`

For the above reasons, this will *not* work with REAPER, Carla, and many more JACK applications.

## Building

It is recommended to download the prebuilt libraries from the Releases tab

### Build dependencies (Debina, Ubuntu)
```
sudo apt install libjack-jackd2-dev:i386
```
### Build
```bash
mkdir build && cd build
cmake ..
make
```
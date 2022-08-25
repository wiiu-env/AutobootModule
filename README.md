[![CI-Release](https://github.com/wiiu-env/AutobootModule/actions/workflows/ci.yml/badge.svg)](https://github.com/wiiu-env/AutobootModule/actions/workflows/ci.yml)
# AutobootModule
This is a bootmenu targetted to be loaded with the [EnvironmentLoader](https://github.com/wiiu-env/EnvironmentLoader). It allows you to boot into the Wii U Menu, Homebrew Channel, vWii System Menu or vWii Homebrew Channel.

## Usage
Place the `99_autoboot` in the `[ENVIRONMENT]/modules/setup` folder and run the EnvironmentLoader. 
- Requires the [HBLInstallerWrapper](https://github.com/wiiu-env/HBLInstallerWrapper) in the `[ENVIRONMENT]/modules/setup` folder.
- Requires the `homebrew_launcher.elf` in `sd:/wiiu/apps/homebrew_launcher/homebrew_launcher.elf`. 

Hold START (+) on the Gamepad while launching this Environment to force open the Autoboot menu.

Press Y on the autoboot menu to set autobooting to this titles. To revert it, force open the menu by holding START (+) while launching the environment.

## Features
- Boot into Wii U Menu, Homebrew Channel, vWii System Menu or vWii Homebrew Channel.
- Full support of Quick Boot Menu of the Gamepad when coldbooting
- Set a autoboot title

## Buildflags

### Logging
Building via `make` only logs errors (via OSReport). To enable logging via the [LoggingModule](https://github.com/wiiu-env/LoggingModule) set `DEBUG` to `1` or `VERBOSE`.

`make` Logs errors only (via OSReport).  
`make DEBUG=1` Enables information and error logging via [LoggingModule](https://github.com/wiiu-env/LoggingModule).  
`make DEBUG=VERBOSE` Enables verbose information and error logging via [LoggingModule](https://github.com/wiiu-env/LoggingModule).

If the [LoggingModule](https://github.com/wiiu-env/LoggingModule) is not present, it'll fallback to UDP (Port 4405) and [CafeOS](https://github.com/wiiu-env/USBSerialLoggingModule) logging.

## Building
For building you just need [wut](https://github.com/devkitPro/wut/) installed, then use the `make` command.

## Building using the Dockerfile

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```
# Build docker image (only needed once)
docker build . -t autobootmodule-builder

# make 
docker run -it --rm -v ${PWD}:/project autobootmodule-builder make

# make clean
docker run -it --rm -v ${PWD}:/project autobootmodule-builder make clean
```

## Format the code via docker

`docker run --rm -v ${PWD}:/src wiiuenv/clang-format:13.0.0-2 -r ./source -i`

## Credits
- GaryOderNichts
- Maschell
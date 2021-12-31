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

## Credits
- GaryOderNichts
- Maschell
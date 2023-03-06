# Microchip PolarFire SoC Zephyr OS Support on Icicle Kit.
This repository provides Microchip PolarFire SoC support for the Zephyr RTOS project with:
- Sample Zephyr OS applications for the Microchip PolarFire SoC Icicle Kit.

- A Dockerfile to build a docker image capable of building a Zephyr OS application.

- SoftConsole Debug launch configurations for debugging a Zephyr OS application on a Microchip Polarfire SoC Icicle Kit.



# Building Zephyr OS Applications


This section describes the procedure to build a Zephyr OS application for Microchip PolarFire SoC.

### Supported Build Hosts
This document assumes you are running on a modern Linux system. The process documented here was tested using Ubuntu 20.04/18.04 LTS.
### Install Zephyr SDK and Build System on Host PC:

To build Zephyr OS applications natively on a host PC, please follow the installation instructions for the Zephyr SDK and build system found in the [Zephyr OS Install Guide](https://docs.zephyrproject.org/latest/getting_started/index.html)


## Build Instructions:
The following commands checkout the polarfire-soc-zephyr-applications in a new directory:
```
$ git clone https://github.com/polarfire-soc/polarfire-soc-zephyr-applications.git
```
The next step initializes the polarfire-soc-zephyr-applications directory to be used with zephyr's build system, west.
```
$ cd polarfire-soc-applications/zephyr
$ west init -l
$ cd ../
$ west update
$ west zephyr-export
$ source zephyr/zephyr-env.sh
```

To build a zephyr application, use the command:
```
$ west build -p -b mpfs_icicle <application> -G "Eclipse CDT4 - Unix Makefiles"
```

Where `mpfs_icicle` is the supported board for the PolarFire SoC Icicle Kit, `<application>` is the Zephyr OS application.

The table below lists the current applications available for PolarFire Soc Icicle Kit:

| `application` |
| --- |
| `mpfs-applications/blinky` |
| `mpfs-applications/smp-ddr-four-cpu` |
| `mpfs-applications/smp-ddr-two-cpu` |
| `mpfs-applications/smp-lim-two-cpu` |
| `mpfs-applications/synchronization` |


### Build Zephyr OS Applications using Docker:
As an alternative to installing the Zephyr SDK and build system on the Host PC, a Dockerfile is provided that will build a Docker image that has all dependencies met to build Zephyr OS applications. To build a Zephyr OS Application using Docker, [follow the instructions here](container-services/README.md)

## Debug Zephyr OS Application using Microchip SoftConsole IDE:
[Follow the instructions here](softconsole-launch-configs/README.md) to start a debug session using Microchips SoftConsole IDE


## Additional Reading:
[Zephyr User Manual](https://docs.zephyrproject.org/latest/)

[Zephyr Project - Github](https://github.com/zephyrproject-rtos/zephyr) 

[Polarfire SoC Documentation](https://github.com/polarfire-soc/polarfire-soc-documentation)    



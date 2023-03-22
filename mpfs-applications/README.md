# PolarFire SoC Zephyr Applications

### Blinky
The Blinky sample blinks an LED forever using the GPIO api

### Button
A simple button demo showcasing the use of GPIO input with interrupts.
The sample prints a message to the console each time a button is pressed.

### Thread Synchronization Support
A simple application that demonstrates basic sanity of the kernel.
Two threads (A and B) take turns printing a greeting message to the console,
and use sleep requests and semaphores to control the rate at which messages
are generated. This demonstrates that kernel scheduling, communication,
and timing are operating correctly.

To build this application and generate an eclipse project configuration file which can be used in Microchip SoftConsole:
```
$ west build -p -b mpfs_icicle mpfs-applications/synchronization -G "Eclipse CDT4 - Unix Makefiles"
```

### Symmetric Multi Processing Support 

This sample applications for SMP calculate Pi independently in many threads, and
demonstrates the benefit of multiple execution units (CPU cores)
when compute-intensive tasks can be run in parallel, with
no cross-dependencies or shared resources.

By changing the value of CONFIG_MP_NUM_CPUS on SMP systems, you
can see that using more cores takes almost linearly less time
to complete the computational task.

You can also edit the sample source code to change the
number of digits calculated (``DIGITS_NUM``), and the
number of threads to use (``THREADS_NUM``).

The applications are provided with Devicetree overlays demonstrating the
application running from:
- DDR with four harts
- DDR with two harts
- Scratchpad with two harts

To build a SMP application, for example targeting DDR with four harts and generate an eclipse project configuration file which can be used in Microchip SoftConsole:
```
$ west build -p -b mpfs_icicle mpfs-applications/smp-ddr-four-cpu/pi - G "Eclipse CDT4 - Unix Makefiles"
```

The `smp-scratchpad-two-cpu` application comes with a Hart Software Services (HSS) payload-generator configuration file, `config.yaml`, which can be used to create a formatted payload image for the Hart Software Service zero-stage bootloader on PolarFire SoC. More information on the HSS payload-generator can be found [here](https://github.com/polarfire-soc/hart-software-services/tree/master/tools/hss-payload-generator).

### Zephyr OS Application Build Directory
The output of the build process can be found in:
```
$ polarfire-soc-zephyr-applications/build/zephyr/ 
```
Here you will find the application ELF binary, zephyr.elf, etc.
### Debugging a Zephyr OS Application using Microchip SoftConsole IDE
[Follow the instructions here](../softconsole-launch-configs/README.md) to start a debug session using Microchips SoftConsole IDE.
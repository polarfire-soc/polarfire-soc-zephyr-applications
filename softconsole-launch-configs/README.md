## Importing the Zephyr application into Microchip SoftConsole IDE
In a SoftConsole workspace, in the top left corner click *File >> import.. >> Existing Projects into Workspace* and click *next*.

Click Browse to navigate to your polarfire-soc-zephyr-applications directory and open it.
The application that has been built should show up in the box. Make sure to uncheck the *copy projects into workspace* option.


## Importing SoftConsole Debug Launch Configuration
A SoftConsole debug configuration is required to load a zephyr application onto hardware using a combination of openOCD and GDB. 

In the SoftConosle workspace, in the top left corner click *File >> import.. >> Run/Debug >> Launch Configurations* and click next. Navigate to your polarfire-soc-zephyr-applications/softconsole-launch-configs directory and click open. Choose the launch config for the Zephyr application that you have built.


The mpfs-applications output to uart 0 of the Polarfire SoC Icicle Kit. Connect your Polarfire SoC Icicle Kit to your PC using microUSB-USB cable to access serial console. Using the `screen`, command in another console, enter:
```
$ sudo screen -L /dev/serial/by-id/usb-Silicon_Labs_CP2108_Quad_USB_to_UART_Bridge_Controller_*-if00-port0 115200
``` 
Power on the Icicle Kit and press `Debug` in SoftConsole to launch the debug seesion. The debugger will stop at the break point '__start', press continue in SoftConsole, which will load the Zephyr application on to the Icicle kit.
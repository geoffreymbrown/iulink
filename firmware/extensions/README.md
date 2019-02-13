It is possible to add a CLI to the existing code through a virtual com
port, although an stm32 with more ram will be required.  We have used this
capability on earlier "base boards".  In this directory, we provide modified

usbcfg.[ch] files providing the additional functionality when used with the
ChibiOS usbserial driver and cli.   Other than creating a cli thread
and initializing the usbserial driver, no signifiant changes are required.




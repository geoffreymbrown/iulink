# iulink
This project provides an stlink "clone" that takes < 13K flash on
an stm32f042.  It does not support tracing (swo), but it does play nicely
with openocd.  We developed this so that we could include stlink support
in a baseboard for programming/configuring sub-gram data loggers that
we are developing for collecting actvity data of songbirds.

In our application we needed to include battery charging hardware and did not
want to ask our biology partners to use a platform cobbled together from
off-the-shelf stlink hardware + adapter boards + power supplies.


An example board schematic is provided to help understand the code.  There
are a few things to note about the board design that may not fit your needs

 1)  This board was designed to talk to a device with a npn transistor
     in the reset line.  Thus, the firmware reverses the sense of reset
     from regular stlink boards

 2)  This board includes devices to electrically isolate the attached "tag"
     when powered off.  This may or may not be necessary in your application


 3)  The simplest case hardware would be:  stm32f042, connectors, voltage
     regulator, passives, and a boot0 button.   In our application we
     use dfu-util to flash the stm32f042.

A few notes about the firmware

 1) The firmware uses ChibiOS which is available under various licensing
     options.  If you plan to use this firmware in a commercial product
     you will need to obtain an appropriate ChibiOS license.


 2) The firmware includes header information from various sources (marked)
    The position of FSF **seems** to be that a GPL header can be used without
    forcing the whole to be GPL'd.  I don't no the answer to this

 3) In my ChibiOS projects, I use FMPP to generate board .h/.c files.  This
    repo includes the generated files (board/), but if you port this to another
    context, you may want to use the boardcfg/ files

 4) The firmware uses an STMicroelectronics VID/PID.  You should obtain
    your own VID/PID if you plan to use this code in anything other than
    a hobby/research environment.  We **do not** have permission to use
    this VID/PID

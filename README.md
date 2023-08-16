
# LIS2DH Zephyr Example with Anymotion Detection

## Overview

This is a modified sample application from the ZephyrOS provided example of the LIS2DH sensor so that it can be configured to either the following:
* Periodically reads accelerometer data from the
LIS2DH sensor (or the compatible LS2DH12, LIS3DH, and LSM303DLHC
sensors), and displays the sensor data on the console.
* Read the accelerometer data when it is configured to trigger on "Anymotion"
  
## Anymotion Configuration
`CONFIG_LIS2DH_ODR_RUNTIME` Must be disabled to use the anymotion trigger for this example and one of `CONFIG_LIS2DH_ODR_X` must be set in the prj.conf file. Otherwise the periodic interrupts will be used.

Threshold and duration is set in the source file for this exmaple, it can be adjusted prior to compiling.

Note that the LIS2DH configured for Anymotion trigger will continuously trigger the host device for 1/ODR seconds. The trigger routine in this example disables itself upon firing and then re-enables itself for 1/ODR seconds to prevent continous interruption upon trigger. 
## References

For more information about the LIS2DH motion sensor see
http://www.st.com/en/mems-and-sensors/lis2dh.html.

## Building and Running

The LIS2DH2 or compatible sensors are available on a variety of boards
and shields supported by Zephyr, including:

* `actinius_icarus`
* `thingy52_nrf52832`
* `stm32f3_disco_board`
* `x-nucleo-iks01a2`

See the board documentation for detailed instructions on how to flash
and get access to the console where acceleration data is displayed.

## Building on actinius_icarus


`actinius_icarus` includes an ST LIS2DH12 accelerometer which
supports the LIS2DH interface.

.. zephyr-app-commands::
   :zephyr-app: samples/sensor/lis2dh
   :board: actinius_icarus
   :goals: build flash
   :compact:

## Building on nucleo_l476rg with IKS01A2 shield

The `x-nucleo-iks01a2` includes an LSM303AGR accelerometer which
supports the LIS2DH interface.  This shield may also be used on other
boards with Arduino headers.

.. zephyr-app-commands::
   :zephyr-app: samples/sensor/lis2dh
   :board: nucleo_l476rg
   :goals: build flash
   :shield: x_nucleo_iks01a2
   :compact:

## Sample Output

    Polling at 0.5 Hz
    #1 @ 12 ms: x -5.387328 , y 5.578368 , z -5.463744
    #2 @ 2017 ms: x -5.310912 , y 5.654784 , z -5.501952
    #3 @ 4022 ms: x -5.349120 , y 5.692992 , z -5.463744

   <repeats endlessly every 2 seconds>

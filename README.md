 Project Summary
---------------
This project combines 3 examples of SimpleLink Example of cc1350, which are pinShutdown_CC1350_LAUNCHXL_tirtos_ccs
i2ctmp007_CC1350_LAUNCHXL_tirtos_ccs
rfEchoRx_CC1350_LAUNCHXL_tirtos_ccs

On the cc1310 Launchpad, we write programs for 2 tasks and register them in RTOS to be controlled by the hardware interrupts. We substitute the Real Time Clock with buttons on the Launchpad to simulate the hardware interrupts. Once button1 is pressed, task 1 of receiving RF packet will be running, the green LED lights up, indicating it is waiting for a packet. After it receives a packet, it will transmit the packet it receives and the red LED toggles to indicate that transmission; once button 0 is pressed, task 2 of reading the data from light sensor MAX44009 will be running, and red LED toggles to indicate the reading process of 20 lux data. During the running time of any of these 2 tasks, the hardware interrupt from the 2 buttons in the system is blocked. After tasks finish, the system goes back to standby mode.

Main program: main_tirtos.c
Task 1 program: rfEchoRx.c
Task 2 program: i2ctmp007.c

Peripherals Exercised
---------------------
During the Process of Task 1:
* `Board_PIN_LED0` - Blinking indicates a successful reception and 
  re-transmission of a packet (echo)
* `Board_PIN_LED1` - Indicates an error in either reception of a packet or 
  in its re-transmission
  
  During the Process of Task 2:
 * `Board_PIN_LED0` - Blinking indicates a successful Reading operation from the light sensor Max44009 via i2c.
 
Resources & Jumper Settings
---------------------------
> If you're using an IDE (such as CCS or IAR), please refer to Board.html in 
your project directory for resources used and board-specific jumper settings. 
Otherwise, you can find Board.html in the directory 
&lt;SDK_INSTALL_DIR&gt;/source/ti/boards/&lt;BOARD&gt;.

Details on rfEchoRx.c
-------------
The user will require two launchpads, one running rfEchoTx (`Board_1`), 
another running rfEchoRx (`Board_2`). Run Board_2 first, followed by 
Board_1. Board_1 is set to transmit a packet every second while Board_2 is 
set to receive the packet and then turnaround and transmit it after a delay of
100ms. Board_PIN_LED1 on Board_1 will toggle when it's able to successfully 
transmits a packet, and when it receives its echo. Board_PIN_LED2 on Board_2 
will toggle when it receives a packet, and once again when it's able to 
re-transmit it (see [figure 1]).

![perfect_echo_ref][figure 1]

If there is an issue in receiving a packet then Board_PIN_LED1 on Board_2 is 
toggled while Board_PIN_LED2 is cleared, to indicate an error condition

![echo_error_ref][figure 2]

Application Design Details
--------------------------
This examples consists of a single task and the exported SmartRF Studio radio
settings.

The default frequency is 868.0 MHz (433.92 MHz for the 
CC1350-LAUNCHXL-433 board). In order to change frequency, modify the
smartrf_settings.c file. This can be done using the code export feature in
Smart RF Studio, or directly in the file.

When the task is executed it:

1. Configures the radio for Proprietary mode
2. Gets access to the radio via the RF drivers RF_open
3. Sets up the radio using CMD_PROP_RADIO_DIV_SETUP command
4. Set the output power to 14 dBm (requires that CCFG_FORCE_VDDR_HH = 1 in ccfg.c)
5. Sets the frequency using CMD_FS command
6. Run the CMD_PROP_RX command and wait for a packet to be transmitted. The 
   receive command is chained with a transmit command, CMD_PROP_TX, which runs
   once a packet is received
7. When a packet is successfully received Board_PIN_LED2 is toggled, the radio
   switches over to the transmit mode and schedules the packet for transmission
   100 ms in the future
8. If there is an issue either with the receive or transmit, an error, both 
   LEDs are set
9. The devices repeat steps 6-8 forever.

Note for IAR users: When using the CC1310DK, the TI XDS110v3 USB Emulator must
be selected. For the CC1310_LAUNCHXL, select TI XDS110 Emulator. In both cases,
select the cJTAG interface.

[figure 1]:rfEcho_PerfectEcho.png "Perfect Echo"
[figure 2]:rfEcho_ErrorTxRx.png "Echo Error"
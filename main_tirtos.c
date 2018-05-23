/*
 * This program is modified from pinShutdown_CC1350_LAUNCHXL_tirtos_ccs in Simplelink Examples of cc1350.
 *
 * This is the main program of our object, which will trigger the listening phase by pressing button 1 (DIO14 on the development
 * board of cc1350) and trigger the reading phase by pressing button 0 (DIO13).
 *
 * The LEDs on develop board are initialized to be off, when button 0 is pressed, the i2c transfer begins and the
 * red LED toggles for 20 times to read the light data and store it in an array variable of lux[20]. When button 1 is
 * pressed, the green LED is on to indicates that it is waiting for a packet. Once the packet is received, the green LED
 * will be off and the red LED toggles to indicate a successful receiving and toggles again to show transmitting a packet.
 * After that it will go back to sleep mode.
 *
 * When buttons are pressed, the BIOS system will release a semaphore (listenSem or readSem), corresponding to the specific thread
 * it calls, and the thread that is waiting for the semaphore will catch it so as to move on to the following steps.
 * To avoid overlapping of listening state and reading data state, during the cycle of each thread, the Hardware interrupt is
 * disabled by PIN_setConfig in buttonCb. After finishing one cycle, the thread (listenThread or readThread) will call PIN_setConfig
 * to enable the Hardware Interrupt on each other, so then main program catches the semaphore and enable hardware interrupts again.
 *
 * Author: Ruoxuan Xu
 * Date: May 10, 2018
 *
 */


////////////////////////////////////////////////////////////////////////////
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <unistd.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Swi.h>
#include "Board.h"
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_prcm.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)


Task_Struct listenTask, readTask;
static uint8_t listenTaskStack[1024], readTaskStack[1024];

Semaphore_Struct listenSem, readSem;

/* Clock used for debounce logic */
Clock_Struct buttonClock;
Clock_Handle hButtonClock;

PIN_Handle hButtons;
PIN_State buttonState;

PIN_Config ButtonTable[] = {
    Board_PIN_BUTTON1   | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    Board_PIN_BUTTON0   | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE                                 /* Terminate list */
};


PIN_Id activeButtonPinId;

/*!*****************************************************************************
 *  @brief      Button clock callback
 *
 *  Called when the debounce periode is over. Stopping the clock, toggling
 *  the device mode based on activeButtonPinId:
 *
 *              Board_PIN_BUTTON1 will put the device in listening mode.
 *
 *  Reenabling the interrupts and resetting the activeButtonPinId.
 *
 *  @param      arg  argument (PIN_Handle) connected to the callback
 *
 ******************************************************************************/
static void buttonClockCb(UArg arg) {
    PIN_Handle buttonHandle = (PIN_State *) arg;

    /* Stop the button clock */
    Clock_stop(hButtonClock);


    /* Check that there is active button for debounce logic*/
    if (activeButtonPinId != PIN_TERMINATE) {
        /* Debounce logic, only toggle if the button is still pushed (low) */
        if (!PIN_getInputValue(activeButtonPinId)) {
            /* Toggle LED based on the button pressed */
            switch (activeButtonPinId) {
            case Board_PIN_BUTTON1:
                Semaphore_post(Semaphore_handle(&listenSem));
                break;
            case Board_PIN_BUTTON0:
                Semaphore_post(Semaphore_handle(&readSem));
                break;
            default:
                /* Do nothing */
                break;
            }
        }
    }

//    /* wait for the triggered phase accomplished and release the finishSem */
//    Semaphore_pend(Semaphore_handle(&finishSem), BIOS_WAIT_FOREVER);


    /* Re-enable interrupts to detect button release. */
    PIN_setConfig(buttonHandle, PIN_BM_IRQ, activeButtonPinId | PIN_IRQ_NEGEDGE);

    /* Set activeButtonPinId to none... */
    activeButtonPinId = PIN_TERMINATE;
}

/*!*****************************************************************************
 *  @brief      Button callback
 *
 *  Initiates the debounce period by disabling interrupts, setting a timeout
 *  for the button clock callback and starting the button clock.
 *  Sets the activeButtonPinId.
 *
 *  @param      handle PIN_Handle connected to the callback
 *
 *  @param      pinId  PIN_Id of the DIO triggering the callback
 *
 *  @return     none
 ******************************************************************************/
static void buttonCb(PIN_Handle handle, PIN_Id pinId) {
    /* Set current pinId to active */
    activeButtonPinId = pinId;

    /* Disable all button interrupts (not only active PinID) during debounces */
    PIN_setConfig(handle, PIN_BM_IRQ, Board_PIN_BUTTON1 | PIN_IRQ_DIS);
    PIN_setConfig(handle, PIN_BM_IRQ, Board_PIN_BUTTON0 | PIN_IRQ_DIS);

    /* Set 50 ms from now as the timeout of hButtonClock instance and start this instance */
    Clock_setTimeout(hButtonClock, (50 * (1000 / Clock_tickPeriod)));
    Clock_start(hButtonClock);

    /* After 50 ms, this instance is closed and a callback function hButtonClockCb is called.*/
}

///////////////////////////////////////////////////////////////////////////////

/*
 *  ======== main_tirtos.c ========
 */
#include <stdint.h>

/* POSIX Header files */
#include <pthread.h>

/* RTOS header files */
#include <ti/sysbios/BIOS.h>

/* Example/Board Header files */
#include "Board.h"

extern void *listenThread(void *arg0);
extern void *readThread(void *arg0);
/* Stack size in bytes */
#define THREADSTACKSIZE    1024

/*
 *  ======== main ========
 */
int main(void)
{
    Power_init();
    PIN_init(BoardGpioInitTable);
    Board_initGeneral();

    hButtons = PIN_open(&buttonState, ButtonTable);  // let every registers in the table configured as the table said
    PIN_registerIntCb(hButtons, buttonCb);  // when the button becomes at the same state as in the 4th column of the ButtonTable
    // hardware interrupt happened and buttonCb is called.

    Clock_Params clockParams;
    Clock_Params_init(&clockParams);
    clockParams.arg = (UArg)hButtons;  //   hButtons are trigger of the clock ????????
    Clock_construct(&buttonClock, buttonClockCb, 0, &clockParams);
    hButtonClock = Clock_handle(&buttonClock);

    /* The listening task initialization*/

    Task_Params listenTaskParams;
    Task_Params_init(&listenTaskParams);
    listenTaskParams.stack = listenTaskStack;
    listenTaskParams.priority = 1;
    listenTaskParams.stackSize = sizeof(listenTaskStack);
    Task_construct(&listenTask, (Task_FuncPtr)listenThread, &listenTaskParams, NULL);

    Semaphore_Params semParams;
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&listenSem, 0, &semParams);



    /* The Reading light sensor task initialization*/

    Task_Params readTaskParams;
    Task_Params_init(&readTaskParams);
    readTaskParams.stack = readTaskStack;
    readTaskParams.priority = 2;
    readTaskParams.stackSize = sizeof(readTaskStack);
    Task_construct(&readTask, (Task_FuncPtr)readThread, &readTaskParams, NULL);

    Semaphore_Params semParam2;
    Semaphore_Params_init(&semParam2);
    semParam2.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&readSem, 0, &semParam2);

    BIOS_start();

}

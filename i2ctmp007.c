/*
 * This program is used to initialize I2C driver, for the master (cc1350) to read the data from the light sensor ( MAX44009)
 * and for the cc1350 to display the data it reads to PC using UART, which was assembled in the library display.h.
 *
 * The slave address of MAX44009:
 * |A0      SLAVE ADDRESS FOR Writing     SLAVE ADDRESS FOR Reading  |
 * |GND            10010100                     10010101             |
 * |VCC            10010110                     10010111             |
 *
 * As the last bit of address is to indicate R/W, we set MAX_ADDR as 8-bit number "10010100" with the write digit set.
 * If A0 is connected to VCC, change it to "10010110". The #define is implemented in Board.h
 *
 * We connect A0 to ground and set the slave address to be 0x4a (only contains 7 bit regardless of the last bit)
 *
 * For MAX44009, we read a byte of data, the register pointer must at first be set through a write operation
 * Send the slave address with the R/W set to 0, followed by the address of the register that needs to be read.
 * After a Repeated START condition, send the slave address with the R/W bit set to 1, to initiate a read operation
 *
 * The register address of MAX44009
 * luxHigh: 0x03
 * luxLow:  0x04
 *
 * The following code includes Display.h for sake of debugging. In debugging, connect the launchpad cc1350 to PC with CCS
 * software, and clicked "view" on top of CCS Edit mode to open the serial port. click the right blue icon on the terminal,
 * choose terminal as "serial terminal" and choose the serial port as "" (which may vary from different devices), then the
 * display can be seen through the terminal output.
 *
 * In actual use of this code, comment out all 'display' statements and shorten the sleep time.
 *
 * Author : Ruoxuan Xu
 * Date: May 10, 2018
 */

/*
 *    ======== i2cMAX44009.c ========
 *
 */
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>
#include <ti/display/Display.h>

/* Example/Board Header files */
#include "Board.h"

#define TASKSTACKSIZE       640


#define MAX_Lux_High    0x03  /* Register Address*/
#define MAX_Lux_Low     0x04  /* Register Address following */

static Display_Handle display;
static uint16_t lux[20];


/* Specific header files and external variables to integrate with the main program */
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/BIOS.h>
extern Semaphore_Struct readSem;
extern PIN_Handle hButtons;




/*
 *  ======== mainThread ========
 */
void *readThread(void *arg0)
{
    unsigned int    i;
    uint16_t        lightLevel;
    uint8_t         txBuffer[1];
    uint8_t         rxBuffer[2];
    I2C_Handle      i2c;
    I2C_Params      i2cParams;
    I2C_Transaction i2cTransaction;

    /* Call driver init functions */
    Display_init();
    GPIO_init();
    I2C_init();

//    /* Configure the LED pin */
//    GPIO_setConfig(Board_GPIO_LED0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Open the HOST display for output */
    display = Display_open(Display_Type_UART, NULL);
    if (display == NULL) {
        while (1);
    }


    Display_printf(display, 0, 0, "Starting the MAX44009 example\n");

    /* Create I2C for usage */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_100kHz;
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
        Display_printf(display, 0, 0, "Error Initializing I2C\n");
        while (1);
    }
    else {
        Display_printf(display, 0, 0, "I2C Initialized!\n");
    }



    /* Point to the T ambient register and read its 2 bytes */
    txBuffer[0] = MAX_Lux_High;
    i2cTransaction.slaveAddress = Board_MAX_ADDR; // Board_MAX_ADDR is identified in Board.h
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 2;

/* only for checking if i2c is fuctioning */
//    if (I2C_transfer(i2c, &i2cTransaction)){
//        Display_printf(display, 0, 0, "Config read!\n");
//        Display_printf(display, 0, 0, "Conf %u:  0x%x 0x%x\n", rxBuffer[0], rxBuffer[1]);
//    } else {
//        Display_printf(display, 0, 0, "Config read fail!\n");
//    }


    while (1) {

        Semaphore_pend(Semaphore_handle(&readSem), BIOS_WAIT_FOREVER);

        /* Take 20 samples and print them out onto the console */
        for (i = 0; i < 20; i++) {
            if (I2C_transfer(i2c, &i2cTransaction)) {
                /* Extract LIGHT LEVEL from the received data; see MAX44009 datasheet */
                int exponent = (rxBuffer[0] & 0xf0) >> 4;
                int mant = (rxBuffer[0] & 0x0f) << 4 | rxBuffer[1];
                lightLevel = (float)(((0x00000001 << exponent) * (float)mant) * 0.045);
                lux[i] = lightLevel;

               Display_printf(display, 0, 0, "Sample %u: %d (C)\n", i, lux[i]);

    /* A different computing method:  https://github.com/herrfz/openwsn-fw/blob/master/bsp/chips/max44009/max44009.c   */

    //            exponent = (( max44009_data[0] >> 4 )  & 0x0F);
    //            mantissa = (( max44009_data[0] & 0x0F ) << 4) | ((max44009_data[1] & 0x0F));
    //
    //            result = ( (uint16_t) exponent << 8 ) | ( (uint16_t) mantissa << 0);
    //
    //            return result;

            }
            else {
                Display_printf(display, 0, 0, "I2C Bus fault\n");
            }

            /* Sleep for 1 second */
            GPIO_write(Board_GPIO_LED0, Board_GPIO_LED_ON);
            sleep(1.0);
            GPIO_write(Board_GPIO_LED0, Board_GPIO_LED_OFF);
            sleep(1.0);
        }

        /* Re-enable button 1 interrupts to detect listen triggers. */
        PIN_setConfig(hButtons, PIN_BM_IRQ, Board_PIN_BUTTON1 | PIN_IRQ_NEGEDGE);

    }

    /* Deinitialized I2C */
    I2C_close(i2c);
//    GPIO_write(Board_GPIO_LED0, Board_GPIO_LED_OFF);
    Display_printf(display, 0, 0, "I2C closed!\n");

    return (NULL);
}

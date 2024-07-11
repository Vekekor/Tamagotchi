/*
 * pasigotchi.c
 *
 *  Created on: Nov 8, 2023
 *      Author: linuxlite
 */
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/i2c/I2CCC26XX.h>

/* Board Header files */
#include "Board.h"
#include "buzzer.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"


/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char rlhuTaskStack[STACKSIZE];


static PIN_Handle ledHandle;
static PIN_State ledState;

static PIN_Handle ledHandle2;
static PIN_State ledState2;

static PIN_Handle hBuzzer;
static PIN_State sBuzzer;

static PIN_Handle buttonHandle;
static PIN_State buttonState;


PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

PIN_Config ledConfig2[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

PIN_Config ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};


static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};
// NAPPIEN käyttöön otto

enum activity{RUOKA = 0, LIIKE, HOIVA, WAITING};
enum activity aktiviteetti = WAITING;

enum sleepstate{AWAKE = 0, ASLEEP};
enum sleepstate unitila = AWAKE;

Int nalka = 10;
Int uni = 10;
Int liike = 10;
Int hoiva = 10;



void buttonFxn(PIN_Handle handle, PIN_Id pinId){
    char merkkijono[30];
    sprintf(merkkijono, "pasin tila: %d, %d, %d, %d\n", nalka, liike, hoiva, uni);
    System_printf(merkkijono);
    System_flush();
}


void sensorTaskFxn(UArg arg0, UArg arg1){

    I2C_Handle i2cMPU;
    I2C_Params i2cMPUParams;

    I2C_Handle i2c;
    I2C_Params i2cParams;


    float ax = 0;
    float ay = 0;
    float az = 0;
    float gx = 0;
    float gy = 0;
    float gz = 0;
    double light = 0;
    Int counter = 0;


    I2C_Params_init(&i2cMPUParams);
        i2cMPUParams.bitRate = I2C_400kHz;
        i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;


        i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
          if (i2cMPU == NULL) {
             System_abort("Error Initializing I2C\n");
          }
          else{
              System_printf("I2C initialized\n");
          }

          Task_sleep(100000 / Clock_tickPeriod);

          mpu9250_setup(&i2cMPU);
          I2C_close(i2cMPU);

          I2C_Params_init(&i2cParams);
          i2cParams.bitRate = I2C_400kHz;

          i2c = I2C_open(Board_I2C_TMP, &i2cParams);
          if(i2c == NULL){
              System_abort("Error Initializing I2C\n");
          }

          Task_sleep(100000 / Clock_tickPeriod);

          opt3001_setup(&i2c);
          I2C_close(i2c);




          while(1){

              counter++;


              i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
              mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

              if(gx < -240){
                  aktiviteetti = RUOKA;
              }
              else if(gy < -240){
                  aktiviteetti = LIIKE;
              }
              else if(gz < -240){
                  aktiviteetti = HOIVA;
              }

              I2C_close(i2cMPU);

              if(counter >= 20){
              i2c = I2C_open(Board_I2C_TMP, &i2cParams);
              light = opt3001_get_data(&i2c);
              if(light < 10 && unitila == AWAKE){
                  unitila = ASLEEP;
                  uint_t pinValue = PIN_getOutputValue(Board_LED0);
                  pinValue = !pinValue;
                  PIN_setOutputValue(ledHandle, Board_LED0, pinValue);
                  System_printf("pasi meni nukkumaan\n");
                  System_flush();
              }
              else if(light >= 10 && unitila == ASLEEP){
                  unitila = AWAKE;
                  uint_t pinValue = PIN_getOutputValue(Board_LED0);
                  pinValue = !pinValue;
                  PIN_setOutputValue(ledHandle, Board_LED0, pinValue);
                  System_printf("pasi heräsi\n");
                  System_flush();
              }

              I2C_close(i2c);
              counter = 0;
              }




              Task_sleep(100000/Clock_tickPeriod);



          }

}

void rlhuTaskFxn(UArg arg0, UArg arg1){


    Int depletioncounter = 0;
    Int sleepcounter = 0;


    while(1){
        if(unitila == AWAKE){

            depletioncounter++;

            if(depletioncounter >= 100){
                nalka--;
                if(nalka < 0){
                    nalka = 0;
                }
                liike--;
                if(liike < 0){
                    liike = 0;
                }
                hoiva--;
                if(hoiva < 0){
                    hoiva = 0;
                }
                uni--;
                if(uni < 0){
                    uni = 0;
                }
                depletioncounter = 0;
            }



            switch(aktiviteetti){
            case RUOKA:
                nalka = nalka + 3;
                if(nalka > 10){
                    nalka = 10;
                }
                buzzerOpen(hBuzzer);
                buzzerSetFrequency(2500);
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerClose();
                System_printf("pasi ruokittiin\n");
                System_flush();
                break;
            case LIIKE:
                liike = liike + 3;
                if(liike > 10){
                    liike = 10;
                }
                buzzerOpen(hBuzzer);
                buzzerSetFrequency(2500);
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerClose();
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerOpen(hBuzzer);
                buzzerSetFrequency(2500);
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerClose();
                System_printf("pasi liikkui\n");
                System_flush();
                break;
            case HOIVA:
                hoiva = hoiva + 3;
                if(hoiva > 10){
                    hoiva = 10;
                }
                buzzerOpen(hBuzzer);
                buzzerSetFrequency(2500);
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerClose();
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerOpen(hBuzzer);
                buzzerSetFrequency(2500);
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerClose();
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerOpen(hBuzzer);
                buzzerSetFrequency(2500);
                Task_sleep(50000 / Clock_tickPeriod);
                buzzerClose();
                System_printf("pasia hoivattiin\n");
                System_flush();
                break;
            default:
                break;
            }




            aktiviteetti = WAITING;
        }
        else if(unitila == ASLEEP){
            sleepcounter++;
            if(sleepcounter >= 30){
                uni++;
                if(uni > 10){
                    uni = 10;
                }
                sleepcounter = 0;
            }
            if(aktiviteetti != WAITING){
                System_printf("shh pasi nukkuu\n");
                System_flush();
                aktiviteetti = WAITING;
            }
        }

        Task_sleep(100000 / Clock_tickPeriod);
    }

}

Int main(void) {



    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;

    Task_Handle rlhuTaskHandle;
    Task_Params rlhuTaskParams;



    Board_initGeneral();
    Board_initI2C();
    Board_initUART();

    buttonHandle = PIN_open(&buttonState, buttonConfig);
       if(!buttonHandle) {
          System_abort("Error initializing button pins\n");
       }

    ledHandle = PIN_open(&ledState, ledConfig);
    if(!ledHandle){
        System_abort("Error initializing LED pin\n");
    }

    hBuzzer = PIN_open(&sBuzzer, cBuzzer);
     if (hBuzzer == NULL) {
       System_abort("Pin open failed!");
     }

     if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
           System_abort("Error registering button callback function");
        }


    Task_Params_init(&sensorTaskParams);
        sensorTaskParams.stackSize = STACKSIZE;
        sensorTaskParams.stack = &sensorTaskStack;
        sensorTaskParams.priority=2;
        sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
        if (sensorTaskHandle == NULL) {
            System_abort("Task create failed!");
        }

    Task_Params_init(&rlhuTaskParams);
    rlhuTaskParams.stackSize = STACKSIZE;
    rlhuTaskParams.stack = &rlhuTaskStack;
    rlhuTaskParams.priority = 2;
    rlhuTaskHandle = Task_create(rlhuTaskFxn, &rlhuTaskParams, NULL);
    if(rlhuTaskHandle == NULL){
        System_abort("Task create failed");
    }



        BIOS_start();


    return(0);
}






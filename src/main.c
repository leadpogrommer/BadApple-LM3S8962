
#include <stdio.h>
#include "FreeRTOSConfig.h"
#include <projdefs.h>
#include "portmacro.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "hw_memmap.h"
#include "hw_types.h"
#include "hw_sysctl.h"
#include "sysctl.h"
#include "gpio.h"
#include "rit128x96x4.h"
#include "pwm.h"

#include "music.h"
#include "lcd_message.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define WEB_STACK_SIZE            ( configMINIMAL_STACK_SIZE * 3 )
#define OLED_QUEUE_SIZE                    ( 3 )

#define CHARACTER_HEIGHT                ( 9 )
#define FONT_FULL_SCALE                        ( 15 )
#define SSI_FREQUENCY                        ( 3500000UL )


extern void vuIP_Task(void *pvParameters);
_Noreturn static void vOLEDTask(void *pvParameters);
static void prvSetupHardware(void);

void vApplicationIdleHook(void) __attribute__((naked));

QueueHandle_t xOLEDQueue;

void set_frequency(float frq){
    if(frq < 0){
        // mute
        PWMOutputState(PWM_BASE, PWM_OUT_1_BIT, false);
    }else{
        // play tone
        float cpu_clock = 50000000.0f;
        float pwm_clock = cpu_clock / 64.0f;


        unsigned long period = (unsigned long)(pwm_clock / frq);
        PWMGenPeriodSet(PWM_BASE, PWM_GEN_0, period);
        PWMPulseWidthSet(PWM_BASE, PWM_OUT_1, period/4);
        PWMOutputState(PWM_BASE, PWM_OUT_1_BIT, true);
    }
}



TaskHandle_t music_task_handle;
_Noreturn void vMusicTask(void *pvParameters){
//    ulTaskNotifyTake()
    xTaskNotifyWait(~0, 0, NULL, 9999999);
    // assume 1 tick = 1 ms
    TickType_t start_tick = xTaskGetTickCount();
    int next_event = 0;

    while (1){
        TickType_t next_wakeup = start_tick + song[next_event].timestamp;
        vTaskDelay(next_wakeup - xTaskGetTickCount());
        set_frequency(song[next_event].frq);
        next_event++;
    }
}

void initPWM(){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);

    GPIODirModeSet(GPIO_PORTG_BASE, GPIO_PIN_1, GPIO_DIR_MODE_HW);
    GPIOPadConfigSet(GPIO_PORTG_BASE, GPIO_PIN_1, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM);

    SysCtlPWMClockSet(SYSCTL_PWMDIV_64);
    PWMGenConfigure(PWM_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PWMGenEnable(PWM_BASE, PWM_GEN_0);
}


uint8_t ucMACAddress[ 6 ] = { 0x70,0x8A,0x91,0x61,0x45,0xBA };

/* Define the network addressing.  These parameters will be used if either
ipconfigUDE_DHCP is 0 or if ipconfigUSE_DHCP is 1 but DHCP auto configuration
failed. */
static const uint8_t ucIPAddress[ 4 ] = { 192, 168, 2, 231 };
static const uint8_t ucNetMask[ 4 ] = { 255, 255, 255, 0 };
static const uint8_t ucGatewayAddress[ 4 ] = { 192, 168, 2, 1 };

static const uint8_t ucDNSServerAddress[ 4 ] = { 208, 67, 222, 222 };



struct freertos_sockaddr xClient, xBindAddress;
socklen_t xSize = sizeof( xClient );
static const char* hello_str = "Hello: ";
static uint8_t framebuffer[128*96*4/8];
_Noreturn void vEchoTask(void *pvParameters){

    vTaskDelay(1000);
    Socket_t socket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);

    static const TickType_t xReceiveTimeOut = portMAX_DELAY;

    FreeRTOS_setsockopt( socket,
                         0,
                         FREERTOS_SO_RCVTIMEO,
                         &xReceiveTimeOut,
                         sizeof( xReceiveTimeOut ) );

    xBindAddress.sin_port = FreeRTOS_htons(1337);
    FreeRTOS_bind(socket, &xBindAddress, sizeof(xBindAddress));

    FreeRTOS_listen(socket, 20);

    Socket_t client = FreeRTOS_accept(socket, &xClient, &xSize);

    FreeRTOS_setsockopt( client,
                         0,
                         FREERTOS_SO_RCVTIMEO,
                         &xReceiveTimeOut,
                         sizeof( xReceiveTimeOut ) );

    FreeRTOS_setsockopt( client,
                         0,
                         FREERTOS_SO_SNDTIMEO,
                         &xReceiveTimeOut,
                         sizeof( xReceiveTimeOut ) );


    FreeRTOS_send(client, (const void *) hello_str, sizeof(hello_str), 0);
    xTaskNotifyGive(music_task_handle);
    while(1){

        size_t to_receive = sizeof(framebuffer);
        size_t received = 0;

        while (received != to_receive){
            BaseType_t  recv_len = FreeRTOS_recv(client, framebuffer + received, to_receive - received, 0);
            received += recv_len;
            if(recv_len == -pdFREERTOS_ERRNO_ENOMEM){
                GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 1);
            }
        }


        xOLEDMessage xOLEDMessage = {framebuffer};
        xQueueSend(xOLEDQueue, &xOLEDMessage, 0);
    }
}

int main(void) {
    prvSetupHardware();
    initPWM();

    FreeRTOS_IPInit( ucIPAddress,
                     ucNetMask,
                     ucGatewayAddress,
                     ucDNSServerAddress,
                     ucMACAddress );


    xOLEDQueue = xQueueCreate(OLED_QUEUE_SIZE, sizeof(xOLEDMessage));

//    xTaskCreate(vuIP_Task, "uIP", WEB_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vMusicTask, "Music", WEB_STACK_SIZE, NULL, 1, &music_task_handle);
    xTaskCreate(vOLEDTask, "OLED", configMINIMAL_STACK_SIZE + 50, NULL, 3, NULL);
    xTaskCreate(vEchoTask, "client", configMINIMAL_STACK_SIZE + 50, NULL, 3, NULL);


    /* Start the scheduler. */
    vTaskStartScheduler();

    /* Will only get here if there was insufficient memory to create the idle
    task. */
    for (;;);
    return 0;
}

/*-----------------------------------------------------------*/

void prvSetupHardware(void) {
    /* If running on Rev A2 silicon, turn the LDO voltage up to 2.75V.  This is
    a workaround to allow the PLL to operate reliably. */
    if (DEVICE_IS_REVA2) {
        SysCtlLDOSet(SYSCTL_LDO_2_75V);
    }

    /* Set the clocking to run from the PLL at 50 MHz */
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);

    /* 	Enable Port F for Ethernet LEDs
        LED0        Bit 3   Output
        LED1        Bit 2   Output */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIODirModeSet(GPIO_PORTF_BASE, (GPIO_PIN_2 | GPIO_PIN_3), GPIO_DIR_MODE_HW);
    GPIOPadConfigSet(GPIO_PORTF_BASE, (GPIO_PIN_2 | GPIO_PIN_3), GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);

    // led
    GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_DIR_MODE_OUT);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_0, 0);
}



_Noreturn void vOLEDTask(void *pvParameters) {
    xOLEDMessage xMessage;

    RIT128x96x4Init(SSI_FREQUENCY);
    RIT128x96x4StringDraw("BadPlayer TCP", 0, 0, FONT_FULL_SCALE);
    RIT128x96x4StringDraw("by leadpogrommer", 0, CHARACTER_HEIGHT, FONT_FULL_SCALE);
    RIT128x96x4StringDraw("Awaiting connection", 0, 96 - CHARACTER_HEIGHT, FONT_FULL_SCALE);

    for (;;) {
        xQueueReceive(xOLEDQueue, &xMessage, portMAX_DELAY);

        RIT128x96x4ImageDraw((unsigned char *)(xMessage.pcMessage), 0, 0, 128, 96);
    }
}

void vApplicationTickHook(void) {
}


_Noreturn void vApplicationStackOverflowHook( TaskHandle_t xTask,  char * pcTaskName) {
    for (;;);
}



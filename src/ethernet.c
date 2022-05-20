//#include "portmacro.h"
#include "FreeRTOS.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"
#include "hw_ethernet.h"
#include "portmacro.h"
#include "FreeRTOS_IP.h"
#include "NetworkBufferManagement.h"
#include "interrupt.h"
#include "ethernet.h"
#include "semphr.h"
#include "FreeRTOS_IP_Private.h"
#include "debug.h"

xTaskHandle emac_task;
static void prvEMACDeferredInterruptHandlerTask( void *pvParameters );

extern uint8_t ucMACAddress[ 6 ];

BaseType_t xNetworkInterfaceInitialise(void )
{
    unsigned long ulTemp;
    portBASE_TYPE xReturn;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
    SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);


    // crete mac task
    xTaskCreate(prvEMACDeferredInterruptHandlerTask, "emac", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &emac_task);
    vTaskDelay(10);

    /* Ensure all interrupts are disabled. */
    EthernetIntDisable(ETH_BASE, (ETH_INT_PHY | ETH_INT_MDIO | ETH_INT_RXER | ETH_INT_RXOF | ETH_INT_TX | ETH_INT_TXER |
                                  ETH_INT_RX));

    /* Clear any interrupts that were already pending. */
    ulTemp = EthernetIntStatus(ETH_BASE, pdFALSE);
    EthernetIntClear(ETH_BASE, ulTemp);

    /* Initialise the MAC and connect. */
    EthernetInit(ETH_BASE);


    EthernetMACAddrSet(ETH_BASE, ucMACAddress);

    EthernetConfigSet(ETH_BASE, (ETH_CFG_TX_DPLXEN | ETH_CFG_TX_CRCEN | ETH_CFG_TX_PADEN));
    EthernetEnable(ETH_BASE);


    /* We are only interested in Rx interrupts. */
    IntPrioritySet(INT_ETH, configKERNEL_INTERRUPT_PRIORITY);
    IntEnable(INT_ETH);
    EthernetIntEnable(ETH_BASE, ETH_INT_RX);

    return pdTRUE;
}

NetworkBufferDescriptor_t *
my_EthernetPacketGetNonBlocking(unsigned long ulBase);

BaseType_t xNetworkInterfaceOutput( NetworkBufferDescriptor_t * const pxDescriptor,
                                    BaseType_t xReleaseAfterSend )
{
    /* Simple network interfaces (as opposed to more efficient zero copy network
    interfaces) just use Ethernet peripheral driver library functions to copy
    data from the FreeRTOS+TCP buffer into the peripheral driver's own buffer.
    This example assumes SendData() is a peripheral driver library function that
    takes a pointer to the start of the data to be sent and the length of the
    data to be sent as two separate parameters.  The start of the data is located
    by pxDescriptor->pucEthernetBuffer.  The length of the data is located
    by pxDescriptor->xDataLength. */
//    SendData( pxDescriptor->pucBuffer, pxDescriptor->xDataLength );

    while (EthernetPacketPut(ETH_BASE, pxDescriptor->pucEthernetBuffer, pxDescriptor->xDataLength) < 0){

    }

    /* Call the standard trace macro to log the send event. */
    iptraceNETWORK_INTERFACE_TRANSMIT();

    if( xReleaseAfterSend != pdFALSE )
    {
        /* It is assumed SendData() copies the data out of the FreeRTOS+TCP Ethernet
        buffer.  The Ethernet buffer is therefore no longer needed, and must be
        freed for re-use. */
        vReleaseNetworkBufferAndDescriptor( pxDescriptor );
    }

    return pdTRUE;
}

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
                                             uint16_t usSourcePort,
                                             uint32_t ulDestinationAddress,
                                             uint16_t usDestinationPort ){
    // TOD: security
    return (ulSourceAddress ^ ulDestinationAddress ^ usSourcePort ^ usDestinationPort);
}

BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber ){
    static uint32_t seed = 0;
    const uint32_t m = (1<<31);
    const uint32_t a = 1103515245;
    const uint32_t c = 12345;

    seed = (a * seed + c) % m;
    *pulNumber = seed;

    return pdTRUE;
}

void vEMAC_ISR(void) {
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    unsigned long ulTemp;

    /* Clear the interrupt. */
    ulTemp = EthernetIntStatus(ETH_BASE, pdFALSE);
    EthernetIntClear(ETH_BASE, ulTemp);

    /* Was it an Rx interrupt? */
    if (ulTemp & ETH_INT_RX) {
        vTaskNotifyGiveFromISR( emac_task, &xHigherPriorityTaskWoken );
        EthernetIntDisable(ETH_BASE, ETH_INT_RX);
    }

    /* Switch to the uIP task. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void prvEMACDeferredInterruptHandlerTask( void *pvParameters )
{
    NetworkBufferDescriptor_t *pxBufferDescriptor;
    size_t xBytesReceived;
/* Used to indicate that xSendEventStructToIPTask() is being called because
of an Ethernet receive event. */
    IPStackEvent_t xRxEvent;

    for( ;; )
    {
        /* Wait for the Ethernet MAC interrupt to indicate that another packet
        has been received.  The task notification is used in a similar way to a
        counting semaphore to count Rx events, but is a lot more efficient than
        a semaphore. */
        ulTaskNotifyTake( pdFALSE, portMAX_DELAY );

        /* See how much data was received.  Here it is assumed ReceiveSize() is
        a peripheral driver function that returns the number of bytes in the
        received Ethernet frame. */


        while( (pxBufferDescriptor = my_EthernetPacketGetNonBlocking(ETH_BASE)) != 0 )
        {


                if( eConsiderFrameForProcessing( pxBufferDescriptor->pucEthernetBuffer )
                    == eProcessBuffer )
                {
                    /* The event about to be sent to the TCP/IP is an Rx event. */
                    xRxEvent.eEventType = eNetworkRxEvent;

                    /* pvData is used to point to the network buffer descriptor that
                    now references the received data. */
                    xRxEvent.pvData = ( void * ) pxBufferDescriptor;

                    /* Send the data to the TCP/IP stack. */
                    if( xSendEventStructToIPTask( &xRxEvent, 0 ) == pdFALSE )
                    {
                        /* The buffer could not be sent to the IP task so the buffer
                        must be released. */
                        vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );

                        /* Make a call to the standard trace macro to log the
                        occurrence. */
                        iptraceETHERNET_RX_EVENT_LOST();
                    }
                    else
                    {
                        /* The message was successfully sent to the TCP/IP stack.
                        Call the standard trace macro to log the occurrence. */
                        iptraceNETWORK_INTERFACE_RECEIVE();
                    }
                }
                else
                {
                    /* The Ethernet frame can be dropped, but the Ethernet buffer
                    must be released. */
                    vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );
                }
        }
        EthernetIntEnable(ETH_BASE, ETH_INT_RX);
    }
}

NetworkBufferDescriptor_t *
my_EthernetPacketGetInternal(unsigned long ulBase)
{
    unsigned long ulTemp;
    long lFrameLen, lTempLen;
    long i = 0;

    NetworkBufferDescriptor_t *buffer_descriptor;

    //
    // Read WORD 0 (see format above) from the FIFO, set the receive
    // Frame Length and store the first two bytes of the destination
    // address in the receive buffer.
    //
    ulTemp = HWREG(ulBase + MAC_O_DATA);
    lFrameLen = (long)(ulTemp & 0xFFFF);

    long lBufLen = lFrameLen - 6;
    buffer_descriptor = pxGetNetworkBufferWithDescriptor( lBufLen, 999999 );

//    if(buffer_descriptor == 0) return 1;
    unsigned char *pucBuf = buffer_descriptor->pucEthernetBuffer;

    pucBuf[i++] = (unsigned char) ((ulTemp >> 16) & 0xff);
    pucBuf[i++] = (unsigned char) ((ulTemp >> 24) & 0xff);

    //
    // Read all but the last WORD into the receive buffer.
    //
    lTempLen = (lBufLen < (lFrameLen - 6)) ? lBufLen : (lFrameLen - 6);
    while(i <= (lTempLen - 4))
    {
        *(unsigned long *)&pucBuf[i] = HWREG(ulBase + MAC_O_DATA);
        i += 4;
    }

    //
    // Read the last 1, 2, or 3 BYTES into the buffer
    //
    if(i < lTempLen)
    {
        ulTemp = HWREG(ulBase + MAC_O_DATA);
        if(i == lTempLen - 3)
        {
            pucBuf[i++] = ((ulTemp >>  0) & 0xff);
            pucBuf[i++] = ((ulTemp >>  8) & 0xff);
            pucBuf[i++] = ((ulTemp >> 16) & 0xff);
            i += 1;
        }
        else if(i == lTempLen - 2)
        {
            pucBuf[i++] = ((ulTemp >>  0) & 0xff);
            pucBuf[i++] = ((ulTemp >>  8) & 0xff);
            i += 2;
        }
        else if(i == lTempLen - 1)
        {
            pucBuf[i++] = ((ulTemp >>  0) & 0xff);
            i += 3;
        }
    }

    //
    // Read any remaining WORDS (that did not fit into the buffer).
    //
    while(i < (lFrameLen - 2))
    {
        ulTemp = HWREG(ulBase + MAC_O_DATA);
        i += 4;
    }

    //
    // If frame was larger than the buffer, return the "negative" frame length
    //
    lFrameLen -= 6;
//    if(lFrameLen > lBufLen)
//    {
//        return(-lFrameLen);
//    }

    //
    // Return the Frame Length
    //
    buffer_descriptor->xDataLength = lBufLen;
    return buffer_descriptor;
}

NetworkBufferDescriptor_t *
my_EthernetPacketGetNonBlocking(unsigned long ulBase)
{
    //
    // Check the arguments.
    //
    ASSERT(ulBase == ETH_BASE);
    ASSERT(pucBuf != 0);
    ASSERT(lBufLen > 0);

    //
    // Check to see if any packets are available.
    //
    if((HWREG(ulBase + MAC_O_NP) & MAC_NP_NPR_M) == 0)
    {
        return(0);
    }

    //
    // Read the packet, and return.
    //
    return(my_EthernetPacketGetInternal(ulBase));
}

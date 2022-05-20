#ifndef FREERTOS_IP_CONFIG_H
#define FREERTOS_IP_CONFIG_H


#define ipconfigUSE_TCP_WIN       0

//#define ipconfigTCP_TX_BUFFER_LENGTH   ( 2 * ipconfigTCP_MSS )
//#define ipconfigTCP_RX_BUFFER_LENGTH   ( 2 * ipconfigTCP_MSS )
//
//
//#define ipconfigNETWORK_MTU   576
//#define ipconfigTCP_MSS       522


#define ipconfigNETWORK_MTU     1526
#define ipconfigTCP_MSS         1460
#define ipconfigTCP_TX_BUFFER_LENGTH  ( 2 * ipconfigTCP_MSS )
#define ipconfigTCP_RX_BUFFER_LENGTH  ( 4 * ipconfigTCP_MSS )


#define ipconfigUSE_DHCP      0
#define ipconfigUSE_DNS       0

#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS  10
#define ipconfigEVENT_QUEUE_LENGTH    ( ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS + 5 )

#define ipconfigIP_TASK_PRIORITY 2
#define ipconfigIP_TASK_STACK_SIZE_WORDS ( configMINIMAL_STACK_SIZE * 3 )

#endif
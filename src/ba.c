#include "ba.h"

#include "uip.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lcd_message.h"

static int handle_connection(struct ba_state *s);

void ba_init(void){
    uip_listen(HTONS(1337));
}

void ba_appcall(void){
    struct ba_state *s = &(uip_conn->appstate);

    if(uip_connected()) {
        PSOCK_INIT(&s->p, s->buff, sizeof(s->buff));
    }

    handle_connection(s);

}

extern QueueHandle_t xOLEDQueue;
extern TaskHandle_t music_task_handle;

static int handle_connection(struct ba_state *s){

    PSOCK_BEGIN(&s->p);
                PSOCK_SEND_STR(&s->p, "Hello: ");
                xTaskNotifyGive(music_task_handle);
                while(1){
                    PSOCK_READBUF(&s->p);
                    xOLEDMessage xOLEDMessage = {s->buff};
                    xQueueSend(xOLEDQueue, &xOLEDMessage, 0);
                }
                PSOCK_CLOSE(&s->p);


    PSOCK_END(&s->p);

}
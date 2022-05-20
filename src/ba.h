#pragma once

#include "uipopt.h"
#include "psock.h"

typedef struct ba_state {
    struct psock p;
    char buff[128*96/2];
}uip_tcp_appstate_t;

void ba_appcall(void);
void ba_init(void);

#ifndef UIP_APPCALL
#define UIP_APPCALL ba_appcall
#endif
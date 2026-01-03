#ifndef _NET_RNDIS_H
#define _NET_RNDIS_H

#include <stdint.h>

// RNDIS Message Types
#define RNDIS_MSG_INIT          0x00000002
#define RNDIS_MSG_INIT_CMPLT    0x80000002
#define RNDIS_MSG_SET           0x00000005
#define RNDIS_MSG_SET_CMPLT     0x80000005
#define OID_GEN_CURRENT_PACKET_FILTER 0x0001010E

// RNDIS Structures
struct rndis_init_msg { 
    uint32_t Type; 
    uint32_t Len; 
    uint32_t ReqId; 
    uint32_t Maj; 
    uint32_t Min; 
    uint32_t MaxXfer; 
};

struct rndis_set_msg { 
    uint32_t Type; 
    uint32_t Len; 
    uint32_t ReqId; 
    uint32_t Oid; 
    uint32_t InfLen; 
    uint32_t InfOff; 
    uint32_t Res; 
    uint32_t Filter; 
};

struct rndis_generic_response {
    uint32_t Type;
    uint32_t Len;
    uint32_t ReqId;
    uint32_t Status;
};

// Fonctions RNDIS
int rndis_init_device(uint8_t usb_addr);
int rndis_is_hardware_mode(void);
void rndis_set_hardware_mode(int mode);

#endif
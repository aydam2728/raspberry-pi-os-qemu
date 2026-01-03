#include "net/rndis.h"
#include "peripherals/usb.h"
#include "utils.h"
#include "printf.h"
#include <stddef.h>

extern void *memset(void *s, int c, size_t n);

static int hardware_mode = 0;
static uint8_t rndis_response_buffer[256] __attribute__((aligned(64)));

// GET_ENCAPSULATED_RESPONSE
static int rndis_get_response(uint8_t usb_addr, uint32_t expected_type) {
    struct usb_setup_packet setup __attribute__((aligned(64)));
    
    int max_attempts = hardware_mode ? 3 : 1;
    
    for (int retry = 0; retry < max_attempts; retry++) {
        if (retry > 0) {
            delay(100000);
        }
        
        memset(rndis_response_buffer, 0, 256);
        invalidate_dcache_range(rndis_response_buffer, 256);
        
        setup.bmRequestType = 0xA1;
        setup.bRequest = 0x01;
        setup.wValue = 0;
        setup.wIndex = 0;
        setup.wLength = 256;
        
        int result = usb_control_transfer(usb_addr, &setup, rndis_response_buffer, 256);
        if (result < 0) {
            continue;
        }
        
        invalidate_dcache_range(rndis_response_buffer, 256);
        
        struct rndis_generic_response *resp = (struct rndis_generic_response *)rndis_response_buffer;
        
        if (resp->Type == expected_type && resp->Status == 0) {
            printf("RNDIS: Response OK (Type=0x%x)\r\n", resp->Type);
            return 0;
        }
        
        if (resp->Type == 0 || resp->Len == 0) {
            continue;
        }
    }
    
    return -1;
}

// SEND_ENCAPSULATED_COMMAND
static int rndis_send_control(uint8_t usb_addr, uint32_t type, uint32_t *data, uint32_t len, uint32_t expected_response) {
    static uint8_t ctrl_buf[128] __attribute__((aligned(64)));
    static struct usb_setup_packet setup __attribute__((aligned(64)));
    
    uint8_t *src = (uint8_t*)data;
    for(int i=0; i<len; i++) ctrl_buf[i] = src[i];
    
    setup.bmRequestType = 0x21;
    setup.bRequest = 0x00;
    setup.wValue = 0; 
    setup.wIndex = 0;
    setup.wLength = len;

    int result = usb_control_transfer(usb_addr, &setup, ctrl_buf, len);
    if (result < 0) {
        return -1;
    }
    
    delay(hardware_mode ? 50000 : 200000);
    
    if (rndis_get_response(usb_addr, expected_response) < 0) {
        return -1;
    }
    
    return 0;
}

int rndis_init_device(uint8_t usb_addr) {
    printf("RNDIS: Attempting full initialization...\r\n");
    
    hardware_mode = 1;  // Essayer en mode hardware
    delay(300000);

    // RNDIS INIT
    struct rndis_init_msg init = { 
        .Type = RNDIS_MSG_INIT, .Len = sizeof(init), .ReqId = 1,
        .Maj = 1, .Min = 0, .MaxXfer = 0x4000
    };
    
    if (rndis_send_control(usb_addr, RNDIS_MSG_INIT, (uint32_t*)&init, sizeof(init), RNDIS_MSG_INIT_CMPLT) < 0) {
        printf("RNDIS: Init FAILED (probably QEMU)\r\n");
        hardware_mode = 0;
        return -1;
    }
    
    printf("RNDIS: Init OK!\r\n");
    delay(100000);
    
    // SET PACKET FILTER
    struct rndis_set_msg set = { 
        .Type = RNDIS_MSG_SET, .Len = sizeof(struct rndis_set_msg), .ReqId = 2,
        .Oid = OID_GEN_CURRENT_PACKET_FILTER, .InfLen = 4, .InfOff = 20,
        .Res = 0, .Filter = 0x0F
    };
    
    if (rndis_send_control(usb_addr, RNDIS_MSG_SET, (uint32_t*)&set, sizeof(set), RNDIS_MSG_SET_CMPLT) < 0) {
        printf("RNDIS: Set Filter FAILED\r\n");
        hardware_mode = 0;
        return -1;
    }
    
    printf("RNDIS: Packet filter set!\r\n");
    return 0;
}

int rndis_is_hardware_mode(void) {
    return hardware_mode;
}

void rndis_set_hardware_mode(int mode) {
    hardware_mode = mode;
}
#include "utils.h"
#include "printf.h"
#include "timer.h"
#include "entry.h"
#include "peripherals/irq.h"

extern void handle_usb_irq(void);
extern void handle_timer_irq(void);

const char *entry_error_messages[] = {
    "SYNC_INVALID_EL1t", "IRQ_INVALID_EL1t", "FIQ_INVALID_EL1t", "ERROR_INVALID_EL1T",
    "SYNC_INVALID_EL1h", "IRQ_INVALID_EL1h", "FIQ_INVALID_EL1h", "ERROR_INVALID_EL1h",
    "SYNC_INVALID_EL0_64", "IRQ_INVALID_EL0_64", "FIQ_INVALID_EL0_64", "ERROR_INVALID_EL0_64",
    "SYNC_INVALID_EL0_32", "IRQ_INVALID_EL0_32", "FIQ_INVALID_EL0_32", "ERROR_INVALID_EL0_32"
};

void enable_interrupt_controller()
{
    printf("[IRQ] Configuring interrupt controller...\r\n");
    
    // Activer Timer ET USB 
    put32(ENABLE_IRQS_1, SYSTEM_TIMER_IRQ_1 | USB_IRQ_MASK);
    
    printf("[IRQ] Timer IRQ enabled (mask: 0x%x)\r\n", SYSTEM_TIMER_IRQ_1);
    printf("[IRQ] USB IRQ enabled (mask: 0x%x)\r\n", USB_IRQ_MASK);
    
    // Vérification
    unsigned int enabled = get32(ENABLE_IRQS_1);
    printf("[IRQ] ENABLE_IRQS_1 = 0x%x (expected: 0x%x)\r\n", 
           enabled, SYSTEM_TIMER_IRQ_1 | USB_IRQ_MASK);
}

void show_invalid_entry_message(int type, unsigned long esr, unsigned long address)
{
    printf("%s, ESR: %x, address: %x\r\n", entry_error_messages[type], esr, address);
}

void handle_irq(void)
{
    unsigned int irq_pending_1 = get32(IRQ_PENDING_1);

    // --- GESTION USB (PRIORITAIRE) ---
    if (irq_pending_1 & USB_IRQ_MASK) {
        handle_usb_irq();
    }

    // --- GESTION TIMER ---
    if (irq_pending_1 & SYSTEM_TIMER_IRQ_1) {
        handle_timer_irq();
    }
}
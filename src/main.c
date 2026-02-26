#include "swos_asm.h"

__attribute__((used, externally_visible)) volatile uint16_t *_panicReasonCode = &panicReasonCode;

int main(void)
{
    _startShim();
    return 0;
}
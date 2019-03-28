#ifndef __KEY_STATES_H__
#define __KEY_STATES_H__

// Includes:

    #include "fsl_common.h"
    #include "slot.h"
    #include "module.h"

// Macros:

    #define ACTIVE(A) (((A)->current & KeyState_Sw))
    #define INACTIVE(A) (!((A)->current & KeyState_Sw))
    #define ACTIVATED_NOW(A) (((A)->current & KeyState_Sw) && !((A)->previous & KeyState_Sw) )
    #define ACTIVATED_EARLIER(A) (((A)->current & KeyState_Sw) && ((A)->previous & KeyState_Sw) )
    #define DEACTIVATED_NOW(A) (!((A)->current & KeyState_Sw) && ((A)->previous & KeyState_Sw) )
    #define DEACTIVATED_EARLIER(A) (!((A)->current & KeyState_Sw) && !((A)->previous & KeyState_Sw) )
    #define KEYSTATE(A, B, C) (((A) ? KeyState_Hw : 0 ) | ((B) ? KeyState_HwDebounced : 0) | ((C) ? KeyState_Sw : 0))

// Typedefs:

    typedef enum {
        KeyState_Hw = 1,
        KeyState_HwDebounced = 2,
        KeyState_Sw = 4
    } key_state_mask_t;

    typedef struct {
        uint8_t timestamp;
        uint8_t previous;
        uint8_t current;
        bool debouncing : 1;
    } key_state_t;

// Variables:

    extern key_state_t KeyStates[SLOT_COUNT][MAX_KEY_COUNT_PER_MODULE];

#endif

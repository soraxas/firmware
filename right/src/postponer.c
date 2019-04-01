#include "postponer.h"
#include "usb_report_updater.h"
#include "macros.h"

postponer_buffer_record_type_t buffer[POSTPONER_BUFFER_SIZE];
uint8_t buffer_size = 0;
uint8_t buffer_position = 0;

void Postponer_ConsumePending(int count, bool suppress) {
    if(buffer_size == 0) {
        return;
    }
    if( suppress ) {
        for(int i = 0; i < count; i++) {
            buffer[(buffer_position + i) % POSTPONER_BUFFER_SIZE].key->suppressed = true;
        }
    }
    buffer_position = (buffer_position + count) % POSTPONER_BUFFER_SIZE;
    buffer_size = count > buffer_size ? 0 : buffer_size - count;

}


void Postponer_RunPostponed(void) {
    if(buffer_size == 0) {
        return;
    }

    if(DEACTIVATED_EARLIER(buffer[buffer_position].key)) {
        ActivateKey(buffer[buffer_position].key, true);
        Postponer_ConsumePending(1, false);
    } else if(ACTIVATED_EARLIER(buffer[buffer_position].key) && !buffer[buffer_position].key->debouncing) {
        /*if the user taps a key twice and holds, we need to "end" the first tap even if the
         * key is physically being held
         */
        buffer[buffer_position].key->current &= ~KeyState_Sw;
    }

    //try prevent loss of keys if some mechanism postpones more keys than we can properly let through
    while(buffer_size > POSTPONER_MAX_FILL) {
        ActivateKey(buffer[buffer_position].key, true);
        Postponer_ConsumePending(1, false);
    }
}

void Postponer_TrackKey(key_state_t *keyState) {
    uint8_t pos = (buffer_position + buffer_size) % POSTPONER_BUFFER_SIZE;
    buffer[pos].key = keyState;
    buffer_size = buffer_size < POSTPONER_BUFFER_SIZE ? buffer_size + 1 : buffer_size;
}

uint8_t Postponer_PendingCount() {
    return buffer_size;
}

bool Postponer_IsPendingReleased() {
    if(buffer_size == 0) {
        return false;
    }
    uint8_t pos = (buffer_position) % POSTPONER_BUFFER_SIZE;
    return (buffer[pos].key->current & KeyState_HwDebounced) == 0 ;
}

uint16_t Postponer_PendingId(int idx) {
    if(idx >= buffer_size) {
        return 0;
    }
    uint8_t pos = (buffer_position + idx) % POSTPONER_BUFFER_SIZE;
    uint32_t ptr1 = (uint32_t)(key_state_t*)buffer[pos].key;
    uint32_t ptr2 = (uint32_t)(key_state_t*)&(KeyStates[0][0]);
    uint32_t res = (ptr1 - ptr2) / sizeof(key_state_t);
    return res;
}

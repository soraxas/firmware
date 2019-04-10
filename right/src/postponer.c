#include "postponer.h"
#include "usb_report_updater.h"
#include "macros.h"

postponer_buffer_record_type_t buffer[POSTPONER_BUFFER_SIZE];
uint8_t buffer_size = 0;
uint8_t buffer_position = 0;
uint8_t cycles_until_activation = 0;

void Postponer_ConsumePending(int count, bool suppress) {
    if(buffer_size == 0) {
        return;
    }
    if( suppress ) {
        for(int i = 0; i < count; i++) {
            uint8_t pos = (buffer_position + i) % POSTPONER_BUFFER_SIZE;
            if(!Postponer_IsKeyReleased(buffer[pos].key)) {
                buffer[pos].key->suppressed = true;
            }
        }
    }
    buffer_position = (buffer_position + count) % POSTPONER_BUFFER_SIZE;
    buffer_size = count > buffer_size ? 0 : buffer_size - count;

}

bool Postponer_IsActive(void) {
    return PostponeKeys || Postponer_PendingCount() > 0 || cycles_until_activation > 0;
}

void Postponer_RunPostponed(void) {
    cycles_until_activation -= cycles_until_activation > 0 ? 1 : 0;

    if(buffer_size == 0) {
        return;
    }

    if(cycles_until_activation == 0) {
        if(ACTIVATED_EARLIER(buffer[buffer_position].key) || DEACTIVATED_EARLIER(buffer[buffer_position].key)) {
            buffer[buffer_position].key->current &= ~KeyState_Sw;
            buffer[buffer_position].key->current |= (buffer[buffer_position].active ? KeyState_Sw : 0);
            Postponer_ConsumePending(1, false);
            cycles_until_activation = CYCLES_PER_ACTIVATION;
        }
    }

    //try prevent loss of keys if some mechanism postpones more keys than we can properly let through
    while(buffer_size > POSTPONER_MAX_FILL) {
        buffer[buffer_position].key->current &= ~KeyState_Sw;
        buffer[buffer_position].key->current |= buffer[buffer_position].active & KeyState_Sw;
        Postponer_ConsumePending(1, false);
    }
}

void Postponer_TrackKey(key_state_t *keyState, bool active) {
    uint8_t pos = (buffer_position + buffer_size) % POSTPONER_BUFFER_SIZE;
    buffer[pos].key = keyState;
    buffer[pos].active = active;
    buffer_size = buffer_size < POSTPONER_BUFFER_SIZE ? buffer_size + 1 : buffer_size;
    cycles_until_activation = CYCLES_PER_ACTIVATION;
}

uint8_t Postponer_PendingCount() {
    uint8_t cnt = 0;
    for ( int i = 0; i < buffer_size; i++ ) {
        uint8_t pos = (buffer_position + i) % POSTPONER_BUFFER_SIZE;
        if(buffer[pos].active) {
            cnt++;
        }
    }
    return cnt;
}

bool Postponer_IsKeyReleased(key_state_t* key) {
    if(key == NULL) {
        return false;
    }
    for ( int i = 0; i < buffer_size; i++ ) {
        uint8_t pos = (buffer_position + i) % POSTPONER_BUFFER_SIZE;
        if(buffer[pos].key == key && !buffer[pos].active) {
            return true;
        }
    }
    return false;
}

key_state_t* getPending(uint8_t n) {
    for ( int i = 0; i < buffer_size; i++ ) {
        uint8_t pos = (buffer_position + i) % POSTPONER_BUFFER_SIZE;
        if(buffer[pos].active) {
            if(n == 0) {
                return buffer[pos].key;
            } else {
                n--;
            }
        }
    }
    return NULL;
}

bool Postponer_IsPendingReleased() {
    return Postponer_IsKeyReleased(getPending(0));
}

uint16_t Postponer_PendingId(int idx) {
    key_state_t* key = getPending(idx);
    if(key == NULL) {
        return 0;
    }
    uint32_t ptr1 = (uint32_t)(key_state_t*)key;
    uint32_t ptr2 = (uint32_t)(key_state_t*)&(KeyStates[0][0]);
    uint32_t res = (ptr1 - ptr2) / sizeof(key_state_t);
    return res;
}

#include "postponer.h"
#include "usb_report_updater.h"
#include "macros.h"

postponer_buffer_record_type_t buffer[POSTPONER_BUFFER_SIZE];
uint8_t buffer_size = 0;
uint8_t buffer_position = 0;
uint8_t cycles_until_activation = 0;
key_state_t* Postponer_NextEventKey;

#define POS(idx) ((buffer_position + (idx)) % POSTPONER_BUFFER_SIZE)

void consume_event(uint8_t count) {
    buffer_position = POS(count);
    buffer_size = count > buffer_size ? 0 : buffer_size - count;
    Postponer_NextEventKey = buffer_size == 0 ? NULL : buffer[buffer_position].key;
}

void consume_one_keypress(bool suppress) {
    uint8_t shifting_by = 0;
    key_state_t* key = NULL;
    bool release_found = false;
    for(int i = 0; i < buffer_size; i++) {
        buffer[POS(i-shifting_by)] = buffer[POS(i)];
        if(release_found) {
            continue;
        }
        if(buffer[POS(i)].active && key == NULL) {
            shifting_by++;
            key = buffer[POS(i)].key;
        } else if (!buffer[POS(i)].active && buffer[POS(i)].key == key) {
            shifting_by++;
            release_found = true;
        }
    }
    if(key != NULL && !release_found && suppress) {
        key->suppressed = true;
    }
    buffer_size -= shifting_by;
    Postponer_NextEventKey = buffer_size == 0 ? NULL : buffer[buffer_position].key;
}

void Postponer_ConsumePending(int count, bool suppress) {
    for(int i = 0; i < count; i++) {
        consume_one_keypress(suppress);
    }
}

bool Postponer_IsActive(void) {
    return PostponeKeys || buffer_size > 0 || cycles_until_activation > 0;
}

bool Postponer_Overflowing(void) {
    if(buffer_size > POSTPONER_MAX_FILL) {
        Macros_ReportErrorNum("Postponer overflowing. Queue length is", POSTPONER_MAX_FILL);
        return true;
    }
    return false;
}

void Postponer_FinishCycle(void) {
    cycles_until_activation -= cycles_until_activation > 0 ? 1 : 0;
}

void Postponer_Reset(void) {
    PostponeKeys = false;
    cycles_until_activation = 0;
    buffer_size = 0;
}

bool Postponer_RunKey(key_state_t* key, bool active) {
    if(key == buffer[buffer_position].key) {
        if(cycles_until_activation == 0 || buffer_size > POSTPONER_MAX_FILL) {
            bool res = buffer[buffer_position].active;
            consume_event(1);
            cycles_until_activation = CYCLES_PER_ACTIVATION;
            return res;
        }
    }
    return active;
}

void Postponer_RunPostponed(void) {
    cycles_until_activation -= cycles_until_activation > 0 ? 1 : 0;

    if(buffer_size == 0) {
        return;
    }

    if(cycles_until_activation == 0 || buffer_size > POSTPONER_MAX_FILL) {
        if(ACTIVATED_EARLIER(buffer[buffer_position].key) || DEACTIVATED_EARLIER(buffer[buffer_position].key)) {
            buffer[buffer_position].key->current &= ~KeyState_Sw;
            buffer[buffer_position].key->current |= (buffer[buffer_position].active ? KeyState_Sw : 0);
            consume_event(1);
            cycles_until_activation = CYCLES_PER_ACTIVATION;
        }
    }
}

void Postponer_TrackKey(key_state_t *keyState, bool active) {
    uint8_t pos = POS(buffer_size);
    buffer[pos].key = keyState;
    buffer[pos].active = active;
    buffer_size = buffer_size < POSTPONER_BUFFER_SIZE ? buffer_size + 1 : buffer_size;
    cycles_until_activation = CYCLES_PER_ACTIVATION;
    Postponer_NextEventKey = buffer_size == 1 ? buffer[buffer_position].key : Postponer_NextEventKey;
}

uint8_t Postponer_PendingCount() {
    uint8_t cnt = 0;
    for ( int i = 0; i < buffer_size; i++ ) {
        if(buffer[POS(i)].active) {
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
        if(buffer[POS(i)].key == key && !buffer[POS(i)].active) {
            return true;
        }
    }
    return false;
}

uint8_t getPendingIdx(uint8_t n) {
    for ( int i = 0; i < buffer_size; i++ ) {
        if(buffer[POS(i)].active) {
            if(n == 0) {
                return i;
            } else {
                n--;
            }
        }
    }
    return 255;
}

key_state_t* getPending(uint8_t n) {
    uint8_t idx = getPendingIdx(n);
    if(idx == 255) {
        return NULL;
    } else {
        return buffer[POS(idx)].key;
    }
}

bool Postponer_IsPendingReleased() {
    return Postponer_IsKeyReleased(getPending(0));
}

uint16_t Postponer_KeyId(key_state_t* key) {
    if(key == NULL) {
        return 0;
    }
    uint32_t ptr1 = (uint32_t)(key_state_t*)key;
    uint32_t ptr2 = (uint32_t)(key_state_t*)&(KeyStates[0][0]);
    uint32_t res = (ptr1 - ptr2) / sizeof(key_state_t);
    return res;
}

uint16_t Postponer_PendingId(int idx) {
    return Postponer_KeyId(getPending(idx));
}

void Postponer_PrintContent() {
    postponer_buffer_record_type_t* first = &buffer[POS(0)];
    postponer_buffer_record_type_t* last = &buffer[POS(buffer_size-1)];
    Macros_SetStatusString("keyid/active", NULL);
    Macros_SetStatusString("\n", NULL);
    for(int i = 0; i < POSTPONER_BUFFER_SIZE; i++) {
        postponer_buffer_record_type_t* ptr = &buffer[i];
        Macros_SetStatusNum(Postponer_KeyId(ptr->key));
        Macros_SetStatusString("/", NULL);
        Macros_SetStatusNum(ptr->active);
        if(ptr == first) {
            Macros_SetStatusString(" <first", NULL);
        }
        if(ptr == last) {
            Macros_SetStatusString(" <last", NULL);
        }
        Macros_SetStatusString("\n", NULL);
    }
}

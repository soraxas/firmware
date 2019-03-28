#include "postponer.h"
#include "usb_report_updater.h"

postponer_buffer_record_type_t buffer[POSTPONER_BUFFER_SIZE];
uint8_t buffer_size = 0;
uint8_t buffer_position = 0;

void Postponer_RunPostponed(void) {
    if(buffer_size == 0) {
        return;
    }

    if(DEACTIVATED_EARLIER(buffer[buffer_position].key)) {
        ActivateKey(buffer[buffer_position].key, true);
        buffer_position = (buffer_position + 1) % POSTPONER_BUFFER_SIZE;
        buffer_size--;
    } else if(ACTIVATED_EARLIER(buffer[buffer_position].key) && !buffer[buffer_position].key->debouncing) {
        /*if the user taps a key twice and holds, we need to "end" the first tap even if the
         * key is physically being held
         */
        buffer[buffer_position].key->current &= ~KeyState_Sw;
    }

    //try prevent loss of keys if some mechanism postpones more keys than we can properly let through
    while(buffer_size > POSTPONER_MAX_FILL) {
        ActivateKey(buffer[buffer_position].key, true);
        buffer_position = (buffer_position + 1) % POSTPONER_BUFFER_SIZE;
        buffer_size--;
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

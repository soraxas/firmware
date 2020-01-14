#include "postponer.h"
#include "usb_report_updater.h"
#include "macros.h"
#include "timer.h"
#include "utils.h"

postponer_buffer_record_type_t buffer[POSTPONER_BUFFER_SIZE];
uint8_t buffer_size = 0;
uint8_t buffer_position = 0;

uint8_t cycles_until_activation = 0;
key_state_t* Postponer_NextEventKey;
uint32_t last_press_time;

#define POS(idx) ((buffer_position + (idx)) % POSTPONER_BUFFER_SIZE)

//##############################
//### Implementation Helpers ###
//##############################

static uint8_t getPendingKeypressIdx(uint8_t n)
{
    for ( int i = 0; i < buffer_size; i++ ) {
        if (buffer[POS(i)].active) {
            if (n == 0) {
                return i;
            } else {
                n--;
            }
        }
    }
    return 255;
}

static key_state_t* getPendingKeypress(uint8_t n)
{
    uint8_t idx = getPendingKeypressIdx(n);
    if (idx == 255) {
        return NULL;
    } else {
        return buffer[POS(idx)].key;
    }
}

static void consumeEvent(uint8_t count)
{
    buffer_position = POS(count);
    buffer_size = count > buffer_size ? 0 : buffer_size - count;
    Postponer_NextEventKey = buffer_size == 0 ? NULL : buffer[buffer_position].key;
}

static void postponeNCycles(uint8_t n) {
    cycles_until_activation = MAX(n + 1, cycles_until_activation);
}

//######################
//### Core Functions ###
//######################

bool PostponerCore_IsActive(void) {
    return buffer_size > 0 || cycles_until_activation > 0;
}

void PostponerCore_PostponeNCycles(uint8_t n) {
    postponeNCycles(n);
}

void PostponerCore_TrackKey(key_state_t *keyState, bool active)
{
    uint8_t pos = POS(buffer_size);
    buffer[pos].key = keyState;
    buffer[pos].active = active;
    buffer_size = buffer_size < POSTPONER_BUFFER_SIZE ? buffer_size + 1 : buffer_size;
    postponeNCycles(POSTPONER_MIN_CYCLES_PER_ACTIVATION);
    Postponer_NextEventKey = buffer_size == 1 ? buffer[buffer_position].key : Postponer_NextEventKey;
    last_press_time = active ? CurrentTime : last_press_time;
}

bool PostponerCore_RunKey(key_state_t* key, bool active)
{
    if (key == buffer[buffer_position].key) {
        if (cycles_until_activation == 0 || buffer_size > POSTPONER_BUFFER_MAX_FILL) {
            bool res = buffer[buffer_position].active;
            consumeEvent(1);
            postponeNCycles(POSTPONER_MIN_CYCLES_PER_ACTIVATION);
            return res;
        }
    }
    return active;
}

//TODO: remove either this or RunKey
void PostponerCore_RunPostponed(void)
{
    cycles_until_activation -= cycles_until_activation > 0 ? 1 : 0;

    if (buffer_size == 0) {
        return;
    }

    if (cycles_until_activation == 0 || buffer_size > POSTPONER_BUFFER_MAX_FILL) {
        if (ACTIVATED_EARLIER(buffer[buffer_position].key) || DEACTIVATED_EARLIER(buffer[buffer_position].key)) {
            buffer[buffer_position].key->current &= ~KeyState_Sw;
            buffer[buffer_position].key->current |= (buffer[buffer_position].active ? KeyState_Sw : 0);
            consumeEvent(1);
            postponeNCycles(POSTPONER_MIN_CYCLES_PER_ACTIVATION);
        }
    }
}

void PostponerCore_FinishCycle(void)
{
    cycles_until_activation -= cycles_until_activation > 0 ? 1 : 0;
}

//#######################
//### Query Functions ###
//#######################


uint8_t PostponerQuery_PendingKeypressCount()
{
    uint8_t cnt = 0;
    for ( int i = 0; i < buffer_size; i++ ) {
        if (buffer[POS(i)].active) {
            cnt++;
        }
    }
    return cnt;
}

bool PostponerQuery_IsKeyReleased(key_state_t* key)
{
    if (key == NULL) {
        return false;
    }
    for ( int i = 0; i < buffer_size; i++ ) {
        if (buffer[POS(i)].key == key && !buffer[POS(i)].active) {
            return true;
        }
    }
    return false;
}

//##########################
//### Extended Functions ###
//##########################

static void consumeOneKeypress(bool suppress)
{
    uint8_t shifting_by = 0;
    key_state_t* removedKeypress = NULL;
    bool release_found = false;
    for (int i = 0; i < buffer_size; i++) {
        buffer[POS(i-shifting_by)] = buffer[POS(i)];
        if (release_found) {
            continue;
        }
        if (buffer[POS(i)].active && removedKeypress == NULL) {
            shifting_by++;
            removedKeypress = buffer[POS(i)].key;
        } else if (!buffer[POS(i)].active && buffer[POS(i)].key == removedKeypress) {
            shifting_by++;
            release_found = true;
        }
    }
    if (removedKeypress != NULL && !release_found && suppress) {
        removedKeypress->suppressed = true;
    }
    buffer_size -= shifting_by;
    Postponer_NextEventKey = buffer_size == 0 ? NULL : buffer[buffer_position].key;
}

void PostponerExtended_ResetPostponer(void)
{
    cycles_until_activation = 0;
    buffer_size = 0;
}

uint16_t PostponerExtended_PendingId(uint16_t idx)
{
    return Utils_KeyStateToKeyId(getPendingKeypress(idx));
}

uint32_t PostponerExtended_LastPressTime()
{
    return last_press_time;
}

void PostponerExtended_ConsumePendingKeypresses(int count, bool suppress)
{
    for (int i = 0; i < count; i++) {
        consumeOneKeypress(suppress);
    }
}

bool PostponerExtended_IsPendingKeyReleased(uint8_t idx)
{
    return PostponerQuery_IsKeyReleased(getPendingKeypress(idx));
}

void PostponerExtended_PrintContent()
{
    postponer_buffer_record_type_t* first = &buffer[POS(0)];
    postponer_buffer_record_type_t* last = &buffer[POS(buffer_size-1)];
    Macros_SetStatusString("keyid/active, size = ", NULL);
    Macros_SetStatusNum(buffer_size);
    Macros_SetStatusString("\n", NULL);
    for (int i = 0; i < POSTPONER_BUFFER_SIZE; i++) {
        postponer_buffer_record_type_t* ptr = &buffer[i];
        Macros_SetStatusNum(Utils_KeyStateToKeyId(ptr->key));
        Macros_SetStatusString("/", NULL);
        Macros_SetStatusNum(ptr->active);
        if (ptr == first) {
            Macros_SetStatusString(" <first", NULL);
        }
        if (ptr == last) {
            Macros_SetStatusString(" <last", NULL);
        }
        Macros_SetStatusString("\n", NULL);
    }
}

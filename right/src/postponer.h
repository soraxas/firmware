#ifndef SRC_POSTPONER_H_
#define SRC_POSTPONER_H_

// Includes:

    #include "key_states.h"

// Macros:

    #define POSTPONER_BUFFER_SIZE 30
    #define POSTPONER_MAX_FILL (POSTPONER_BUFFER_SIZE-5)

/**
 * CYCLES_PER_ACTIVATION/CYCLES_SINCE_MACRO_ENGINE_ACTIVE ensures that:
 * - key is not activated earlier than the number of cycles after last postponeKeyes modifier is active
 * - that key remains active for at least that number of cycles (split composite keystrokes take 2 cycles to complete)
 */
    #define CYCLES_PER_ACTIVATION 2
    #define CYCLES_SINCE_MACRO_ENGINE_ACTIVE 4

// Typedefs:
    typedef struct {
        key_state_t * key;
        bool active;
    } postponer_buffer_record_type_t;

// Variables:

    extern key_state_t* Postponer_NextEventKey;

// Functions:

    void Postponer_RunPostponed(void);
    void Postponer_TrackKey(key_state_t *keyState, bool active);
    uint8_t Postponer_PendingCount();
    bool Postponer_IsActive(void);
    bool Postponer_Overflowing(void);
    bool Postponer_IsPendingReleased();
    bool Postponer_IsKeyReleased(key_state_t* key);
    uint16_t Postponer_PendingId(uint16_t idx);
    uint16_t Postponer_KeyId(key_state_t* key);
    void Postponer_DecodeId(uint16_t keyid, uint8_t* outSlotId, uint8_t* outSlotIdx);
    key_state_t* Postponer_KeyState(uint16_t keyid);
    void Postponer_ConsumePending(int count, bool suppress);
    void Postponer_PrintContent();
    bool Postponer_RunKey(key_state_t* key, bool active);
    void Postponer_FinishCycle(void);
    void Postponer_Reset(void);
    uint32_t Postponer_LastPressTime(void);

#endif /* SRC_POSTPONER_H_ */

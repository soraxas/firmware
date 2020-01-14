#ifndef SRC_POSTPONER_H_
#define SRC_POSTPONER_H_

// Includes:

    #include "key_states.h"

// Macros:

    #define POSTPONER_BUFFER_SIZE 32
    #define POSTPONER_BUFFER_MAX_FILL (POSTPONER_BUFFER_SIZE-5)

    // CYCLES_PER_ACTIVATION ensures that:
    // - key is not activated earlier than the number of cycles after last postponeKeyes modifier is active
    // - that key remains active for at least that number of cycles (split composite keystrokes take 2 cycles to complete)
    #define POSTPONER_MIN_CYCLES_PER_ACTIVATION 2

// Typedefs:

    typedef struct {
        key_state_t * key;
        bool active;
    } postponer_buffer_record_type_t;

// Variables:

    extern key_state_t* Postponer_NextEventKey;

// Functions (Core hooks):

    bool PostponerCore_IsActive(void);
    void PostponerCore_PostponeNCycles(uint8_t n);
    void PostponerCore_TrackKey(key_state_t *keyState, bool active);
    bool PostponerCore_RunKey(key_state_t* key, bool active);
    void PostponerCore_RunPostponed(void);
    void PostponerCore_FinishCycle(void);

// Functions (Basic Query APIs):

    uint8_t PostponerQuery_PendingKeypressCount();
    bool PostponerQuery_IsKeyReleased(key_state_t* key);

// Functions (Query APIs extended):
    uint16_t PostponerExtended_PendingId(uint16_t idx);
    uint32_t PostponerExtended_LastPressTime(void);
    bool PostponerExtended_IsPendingKeyReleased(uint8_t idx);
    void PostponerExtended_ConsumePendingKeypresses(int count, bool suppress);
    void PostponerExtended_ResetPostponer(void);

    void PostponerExtended_PrintContent();

#endif /* SRC_POSTPONER_H_ */

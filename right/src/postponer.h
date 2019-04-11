#ifndef SRC_POSTPONER_H_
#define SRC_POSTPONER_H_

// Includes:

    #include "key_states.h"

// Macros:

    #define POSTPONER_BUFFER_SIZE 30
    #define POSTPONER_MAX_FILL 25
    #define CYCLES_PER_ACTIVATION 2

// Typedefs:
    typedef struct {
        key_state_t * key;
        bool active;
    } postponer_buffer_record_type_t;

// Functions:
    void Postponer_RunPostponed(void);
    void Postponer_TrackKey(key_state_t *keyState, bool active);
    uint8_t Postponer_PendingCount();
    bool Postponer_IsActive(void);
    bool Postponer_Overflowing(void);
    bool Postponer_IsPendingReleased();
    bool Postponer_IsKeyReleased(key_state_t* key);
    uint16_t Postponer_PendingId(int idx);
    void Postponer_ConsumePending(int count, bool suppress);

#endif /* SRC_POSTPONER_H_ */

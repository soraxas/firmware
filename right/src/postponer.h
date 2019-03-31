#ifndef SRC_POSTPONER_H_
#define SRC_POSTPONER_H_

// Includes:

    #include "key_states.h"

// Macros:

    #define POSTPONER_BUFFER_SIZE 20
    #define POSTPONER_MAX_FILL 15

// Typedefs:
    typedef struct {
        key_state_t * key;
    } postponer_buffer_record_type_t;

// Functions:
    void Postponer_RunPostponed(void);
    void Postponer_TrackKey(key_state_t *keyState);
    uint8_t Postponer_PendingCount();
    bool Postponer_PendingReleased();

#endif /* SRC_POSTPONER_H_ */

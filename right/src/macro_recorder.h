#ifndef __MACRO_RECORDER_H__
#define __MACRO_RECORDER_H__


// Includes:

    #include <stdint.h>
    #include <stdbool.h>
    #include "key_action.h"
    #include "usb_device_config.h"
    #include "key_states.h"

// Macros:

    #define MAX_RUNTIME_MACROS 32
    #define REPORT_BUFFER_MAX_LENGTH 2048
    #define REPORT_BUFFER_MIN_GAP 512

// Typedefs:

    typedef enum {
        BasicKeyboardEmpty,
        BasicKeyboardSimple,
        BasicKeyboard
    } macro_report_type_t;

    typedef struct {
        uint8_t id;
        uint16_t offset;
        uint16_t length;
    } runtime_macro_header;

// Variables:

    extern bool RuntimeMacroPlaying;

// Functions:

    void RecordBasicReport(usb_basic_keyboard_report_t *report);

    bool PlayRuntimeMacroSmart(uint8_t id, usb_basic_keyboard_report_t *report);
    void RecordRuntimeMacroSmart(uint8_t id);

#endif

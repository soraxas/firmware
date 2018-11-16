#ifndef __MACRO_RECORDER_H__
#define __MACRO_RECORDER_H__

// Includes:

    #include <stdint.h>
    #include <stdbool.h>
    #include "key_action.h"
    #include "usb_device_config.h"
    #include "key_states.h"

// Typedefs:

    typedef enum {
        EmptyReport,
        BasicKeyboard
    } macro_report_type_t;

    typedef struct {
        uint8_t offset;
        uint8_t length;
    } runtime_macro_header;

// Variables:

    extern bool RuntimeMacroPlaying;

// Functions:

    void RecordReport(usb_basic_keyboard_report_t *report, uint8_t size);

    bool PlayRuntimeMacroSmart();
    void RecordRuntimeMacroSmart();

#endif

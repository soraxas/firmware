#ifndef __MACROS_H__
#define __MACROS_H__

// Includes:

    #include <stdint.h>
    #include <stdbool.h>
    #include "key_action.h"
    #include "usb_device_config.h"
    #include "key_states.h"

// Macros:

    #define MAX_MACRO_NUM 255
    #define STATUS_BUFFER_MAX_LENGTH 1024
    #define LAYER_STACK_SIZE 5
    #define MACRO_STATE_POOL_SIZE 5

// Typedefs:

    typedef struct {
        uint16_t firstMacroActionOffset;
        uint16_t macroActionsCount;
    } macro_reference_t;

    typedef enum {
        MacroSubAction_Tap,
        MacroSubAction_Press,
        MacroSubAction_Release,
    } macro_sub_action_t;

    typedef enum {
        MacroActionType_Key,
        MacroActionType_MouseButton,
        MacroActionType_MoveMouse,
        MacroActionType_ScrollMouse,
        MacroActionType_Delay,
        MacroActionType_Text,
    } macro_action_type_t;

    typedef struct {
        union {
            struct {
                macro_sub_action_t action;
                keystroke_type_t type;
                uint16_t scancode;
                uint8_t modifierMask;
            } ATTR_PACKED key;
            struct {
                macro_sub_action_t action;
                uint8_t mouseButtonsMask;
            } ATTR_PACKED mouseButton;
            struct {
                int16_t x;
                int16_t y;
            } ATTR_PACKED moveMouse;
            struct {
                int16_t x;
                int16_t y;
            } ATTR_PACKED scrollMouse;
            struct {
                uint16_t delay;
            } ATTR_PACKED delay;
            struct {
                const char *text;
                uint16_t textLen;
            } ATTR_PACKED text;
        };
        macro_action_type_t type;
    } ATTR_PACKED macro_action_t;

    typedef struct {
        bool macroInterrupted;
        bool macroBroken;
        bool macroPlaying;

        uint8_t currentMacroIndex;
        uint16_t currentMacroActionIndex;
        macro_action_t currentMacroAction;
        key_state_t *currentMacroKey;
        uint8_t previousMacroIndex;
        uint32_t previousMacroEndTime;

        bool keyActionPressStarted;
        bool mouseButtonPressStarted;
        bool mouseMoveInMotion;
        bool mouseScrollInMotion;
        uint16_t dispatchTextIndex;
        uint8_t dispatchReportIndex;

        bool delayActive;
        uint32_t delayStart;

        uint16_t bufferOffset;
    } macro_state_t;

// Variables:

    extern macro_reference_t AllMacros[MAX_MACRO_NUM];
    extern uint8_t AllMacrosCount;
    extern bool MacroPlaying;
    extern usb_mouse_report_t MacroMouseReport;
    extern usb_basic_keyboard_report_t MacroBasicKeyboardReport;
    extern usb_media_keyboard_report_t MacroMediaKeyboardReport;
    extern usb_system_keyboard_report_t MacroSystemKeyboardReport;

// Functions:

    void Macros_StartMacro(uint8_t index, key_state_t *keyState);
    void Macros_ContinueMacro(void);
    void Macros_SignalInterrupt(void);
    void Macros_ResetReportClaims(void);
    void Macros_ReportError(const char* err, const char* arg, const char *argEnd);

#endif

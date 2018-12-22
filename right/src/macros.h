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
    #define LAYER_STACK_SIZE 10
    #define MACRO_STATE_POOL_SIZE 5

    #define ALTMASK (HID_KEYBOARD_MODIFIER_LEFTALT | HID_KEYBOARD_MODIFIER_RIGHTALT)
    #define CTRLMASK (HID_KEYBOARD_MODIFIER_LEFTCTRL | HID_KEYBOARD_MODIFIER_RIGHTCTRL)
    #define SHIFTMASK (HID_KEYBOARD_MODIFIER_LEFTSHIFT | HID_KEYBOARD_MODIFIER_RIGHTSHIFT)
    #define GUIMASK (HID_KEYBOARD_MODIFIER_LEFTGUI | HID_KEYBOARD_MODIFIER_RIGHTGUI)

// Typedefs:

    typedef struct {
        uint16_t firstMacroActionOffset;
        uint16_t macroActionsCount;
    } macro_reference_t;

    typedef struct {
        uint8_t layer;
        bool removed;
    } layerStackRecord;

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
        uint32_t currentMacroStartTime;

        uint8_t keyActionPressPhase;
        bool mouseButtonPressStarted;
        bool mouseMoveInMotion;
        bool mouseScrollInMotion;
        uint16_t dispatchTextIndex;
        uint8_t dispatchReportIndex;

        bool currentConditionPassed;

        bool delayActive;
        uint32_t delayStart;

        bool holdActive;
        uint8_t holdLayerIdx;

        uint16_t bufferOffset;

        bool reportsUsed;
        usb_mouse_report_t macroMouseReport;
        usb_basic_keyboard_report_t macroBasicKeyboardReport;
        usb_media_keyboard_report_t macroMediaKeyboardReport;
        usb_system_keyboard_report_t macroSystemKeyboardReport;
    } macro_state_t;

// Variables:

    extern macro_reference_t AllMacros[MAX_MACRO_NUM];
    extern uint8_t AllMacrosCount;
    extern macro_state_t MacroState[MACRO_STATE_POOL_SIZE];
    extern bool MacroPlaying;

// Functions:

    void Macros_StartMacro(uint8_t index, key_state_t *keyState);
    void Macros_ContinueMacro(void);
    void Macros_SignalInterrupt(void);
    bool Macros_ClaimReports(void);
    void Macros_ReportError(const char* err, const char* arg, const char *argEnd);
    void Macros_ReportErrorNum(const char* err, uint32_t num);

#endif

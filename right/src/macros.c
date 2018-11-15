#include "macros.h"
#include "config_parser/parse_macro.h"
#include "config_parser/config_globals.h"
#include "timer.h"
#include "keymap.h"

macro_reference_t AllMacros[MAX_MACRO_NUM];
uint8_t AllMacrosCount;
bool MacroPlaying = false;
usb_mouse_report_t MacroMouseReport;
usb_basic_keyboard_report_t MacroBasicKeyboardReport;
usb_media_keyboard_report_t MacroMediaKeyboardReport;
usb_system_keyboard_report_t MacroSystemKeyboardReport;

static uint8_t currentMacroIndex;
static uint16_t currentMacroActionIndex;
static macro_action_t currentMacroAction;
static key_state_t *currentMacroKey;
static uint8_t previousMacroIndex;
static uint32_t previousMacroEndTime;
static bool wantBreak = false;

#define ERROR_STATUS_BUFFER_LENGTH 256
static char errorStatusBuffer[ERROR_STATUS_BUFFER_LENGTH];
static uint8_t errorStatusLen;

uint8_t characterToScancode(char character)
{
    switch (character) {
        case 'A' ... 'Z':
        case 'a' ... 'z':
            return HID_KEYBOARD_SC_A - 1 + (character & 0x1F);
        case '1' ... '9':
            return HID_KEYBOARD_SC_1_AND_EXCLAMATION - 1 + (character & 0x0F);
        case ')':
        case '0':
            return HID_KEYBOARD_SC_0_AND_CLOSING_PARENTHESIS;
        case '!':
            return HID_KEYBOARD_SC_1_AND_EXCLAMATION;
        case '@':
            return HID_KEYBOARD_SC_2_AND_AT;
        case '#':
            return HID_KEYBOARD_SC_3_AND_HASHMARK;
        case '$':
            return HID_KEYBOARD_SC_4_AND_DOLLAR;
        case '%':
            return HID_KEYBOARD_SC_5_AND_PERCENTAGE;
        case '^':
            return HID_KEYBOARD_SC_6_AND_CARET;
        case '&':
            return HID_KEYBOARD_SC_7_AND_AMPERSAND;
        case '*':
            return HID_KEYBOARD_SC_8_AND_ASTERISK;
        case '(':
            return HID_KEYBOARD_SC_9_AND_OPENING_PARENTHESIS;
        case '`':
        case '~':
            return HID_KEYBOARD_SC_GRAVE_ACCENT_AND_TILDE;
        case '[':
        case '{':
            return HID_KEYBOARD_SC_OPENING_BRACKET_AND_OPENING_BRACE;
        case ']':
        case '}':
            return HID_KEYBOARD_SC_CLOSING_BRACKET_AND_CLOSING_BRACE;
        case ';':
        case ':':
            return HID_KEYBOARD_SC_SEMICOLON_AND_COLON;
        case '\'':
        case '\"':
            return HID_KEYBOARD_SC_APOSTROPHE_AND_QUOTE;
        case '+':
        case '=':
            return HID_KEYBOARD_SC_EQUAL_AND_PLUS;
        case '\\':
        case '|':
            return HID_KEYBOARD_SC_BACKSLASH_AND_PIPE;
        case '.':
        case '>':
            return HID_KEYBOARD_SC_DOT_AND_GREATER_THAN_SIGN;
        case ',':
        case '<':
            return HID_KEYBOARD_SC_COMMA_AND_LESS_THAN_SIGN;
        case '/':
        case '\?':
            return HID_KEYBOARD_SC_SLASH_AND_QUESTION_MARK;
        case '-':
        case '_':
            return HID_KEYBOARD_SC_MINUS_AND_UNDERSCORE;
        case '\n':
            return HID_KEYBOARD_SC_ENTER;
        case ' ':
            return HID_KEYBOARD_SC_SPACE;
    }
    return 0;
}

bool characterToShift(char character)
{
    switch (character) {
        case 'A' ... 'Z':
        case ')':
        case '!':
        case '@':
        case '#':
        case '$':
        case '%':
        case '^':
        case '&':
        case '*':
        case '(':
        case '~':
        case '{':
        case '}':
        case ':':
        case '\"':
        case '+':
        case '|':
        case '>':
        case '<':
        case '\?':
        case '_':
            return true;
    }
    return false;
}

void addBasicScancode(uint8_t scancode)
{
    if (!scancode) {
        return;
    }
    for (uint8_t i = 0; i < USB_BASIC_KEYBOARD_MAX_KEYS; i++) {
        if (MacroBasicKeyboardReport.scancodes[i] == scancode) {
            return;
        }
    }
    for (uint8_t i = 0; i < USB_BASIC_KEYBOARD_MAX_KEYS; i++) {
        if (!MacroBasicKeyboardReport.scancodes[i]) {
            MacroBasicKeyboardReport.scancodes[i] = scancode;
            break;
        }
    }
}

void deleteBasicScancode(uint8_t scancode)
{
    if (!scancode) {
        return;
    }
    for (uint8_t i = 0; i < USB_BASIC_KEYBOARD_MAX_KEYS; i++) {
        if (MacroBasicKeyboardReport.scancodes[i] == scancode) {
            MacroBasicKeyboardReport.scancodes[i] = 0;
            return;
        }
    }
}

void addModifiers(uint8_t modifiers)
{
    MacroBasicKeyboardReport.modifiers |= modifiers;
}

void deleteModifiers(uint8_t modifiers)
{
    MacroBasicKeyboardReport.modifiers &= ~modifiers;
}

void addMediaScancode(uint16_t scancode)
{
    if (!scancode) {
        return;
    }
    for (uint8_t i = 0; i < USB_MEDIA_KEYBOARD_MAX_KEYS; i++) {
        if (MacroMediaKeyboardReport.scancodes[i] == scancode) {
            return;
        }
    }
    for (uint8_t i = 0; i < USB_MEDIA_KEYBOARD_MAX_KEYS; i++) {
        if (!MacroMediaKeyboardReport.scancodes[i]) {
            MacroMediaKeyboardReport.scancodes[i] = scancode;
            break;
        }
    }
}

void deleteMediaScancode(uint16_t scancode)
{
    if (!scancode) {
        return;
    }
    for (uint8_t i = 0; i < USB_MEDIA_KEYBOARD_MAX_KEYS; i++) {
        if (MacroMediaKeyboardReport.scancodes[i] == scancode) {
            MacroMediaKeyboardReport.scancodes[i] = 0;
            return;
        }
    }
}

void addSystemScancode(uint8_t scancode)
{
    if (!scancode) {
        return;
    }
    for (uint8_t i = 0; i < USB_SYSTEM_KEYBOARD_MAX_KEYS; i++) {
        if (MacroSystemKeyboardReport.scancodes[i] == scancode) {
            return;
        }
    }
    for (uint8_t i = 0; i < USB_SYSTEM_KEYBOARD_MAX_KEYS; i++) {
        if (!MacroSystemKeyboardReport.scancodes[i]) {
            MacroSystemKeyboardReport.scancodes[i] = scancode;
            break;
        }
    }
}

void deleteSystemScancode(uint8_t scancode)
{
    if (!scancode) {
        return;
    }
    for (uint8_t i = 0; i < USB_SYSTEM_KEYBOARD_MAX_KEYS; i++) {
        if (MacroSystemKeyboardReport.scancodes[i] == scancode) {
            MacroSystemKeyboardReport.scancodes[i] = 0;
            return;
        }
    }
}

void addScancode(uint16_t scancode, macro_sub_action_t type)
{
    switch (type) {
        case KeystrokeType_Basic:
            addBasicScancode(scancode);
            break;
        case KeystrokeType_Media:
            addMediaScancode(scancode);
            break;
        case KeystrokeType_System:
            addSystemScancode(scancode);
            break;
    }
}

void deleteScancode(uint16_t scancode, macro_sub_action_t type)
{
    switch (type) {
        case KeystrokeType_Basic:
            deleteBasicScancode(scancode);
            break;
        case KeystrokeType_Media:
            deleteMediaScancode(scancode);
            break;
        case KeystrokeType_System:
            deleteSystemScancode(scancode);
            break;
    }
}

bool processKeyAction(void)
{
    static bool pressStarted;

    switch (currentMacroAction.key.action) {
        case MacroSubAction_Tap:
            if (!pressStarted) {
                pressStarted = true;
                addModifiers(currentMacroAction.key.modifierMask);
                addScancode(currentMacroAction.key.scancode, currentMacroAction.key.type);
                return true;
            }
            pressStarted = false;
            deleteModifiers(currentMacroAction.key.modifierMask);
            deleteScancode(currentMacroAction.key.scancode, currentMacroAction.key.type);
            break;
        case MacroSubAction_Release:
            deleteModifiers(currentMacroAction.key.modifierMask);
            deleteScancode(currentMacroAction.key.scancode, currentMacroAction.key.type);
            break;
        case MacroSubAction_Press:
            addModifiers(currentMacroAction.key.modifierMask);
            addScancode(currentMacroAction.key.scancode, currentMacroAction.key.type);
            break;
    }
    return false;
}

bool processDelayAction(void)
{
    static bool inDelay;
    static uint32_t delayStart;

    if (inDelay) {
        if (Timer_GetElapsedTime(&delayStart) >= currentMacroAction.delay.delay) {
            inDelay = false;
        }
    } else {
        delayStart = CurrentTime;
        inDelay = true;
    }
    return inDelay;
}

bool processMouseButtonAction(void)
{
    static bool pressStarted;

    switch (currentMacroAction.key.action) {
        case MacroSubAction_Tap:
            if (!pressStarted) {
                pressStarted = true;
                MacroMouseReport.buttons |= currentMacroAction.mouseButton.mouseButtonsMask;
                return true;
            }
            pressStarted = false;
            MacroMouseReport.buttons &= ~currentMacroAction.mouseButton.mouseButtonsMask;
            break;
        case MacroSubAction_Release:
            MacroMouseReport.buttons &= ~currentMacroAction.mouseButton.mouseButtonsMask;
            break;
        case MacroSubAction_Press:
            MacroMouseReport.buttons |= currentMacroAction.mouseButton.mouseButtonsMask;
            break;
    }
    return false;
}

bool processMoveMouseAction(void)
{
    static bool inMotion;

    if (inMotion) {
        MacroMouseReport.x = 0;
        MacroMouseReport.y = 0;
        inMotion = false;
    } else {
        MacroMouseReport.x = currentMacroAction.moveMouse.x;
        MacroMouseReport.y = currentMacroAction.moveMouse.y;
        inMotion = true;
    }
    return inMotion;
}

bool processScrollMouseAction(void)
{
    static bool inMotion;

    if (inMotion) {
        MacroMouseReport.wheelX = 0;
        MacroMouseReport.wheelY = 0;
        inMotion = false;
    } else {
        MacroMouseReport.wheelX = currentMacroAction.scrollMouse.x;
        MacroMouseReport.wheelY = currentMacroAction.scrollMouse.y;
        inMotion = true;
    }
    return inMotion;
}

bool dispatchText(const char* text, uint16_t textLen)
{
    static uint16_t textIndex;
    static uint8_t reportIndex = USB_BASIC_KEYBOARD_MAX_KEYS;
    char character;
    uint8_t scancode;

    if (textIndex == textLen) {
        textIndex = 0;
        reportIndex = USB_BASIC_KEYBOARD_MAX_KEYS;
        memset(&MacroBasicKeyboardReport, 0, sizeof MacroBasicKeyboardReport);
        return false;
    }
    if (reportIndex == USB_BASIC_KEYBOARD_MAX_KEYS) {
        reportIndex = 0;
        memset(&MacroBasicKeyboardReport, 0, sizeof MacroBasicKeyboardReport);
        return true;
    }
    character = text[textIndex];
    scancode = characterToScancode(character);
    for (uint8_t i = 0; i < reportIndex; i++) {
        if (MacroBasicKeyboardReport.scancodes[i] == scancode) {
            reportIndex = USB_BASIC_KEYBOARD_MAX_KEYS;
            return true;
        }
    }
    MacroBasicKeyboardReport.scancodes[reportIndex++] = scancode;
    MacroBasicKeyboardReport.modifiers = characterToShift(character) ? HID_KEYBOARD_MODIFIER_LEFTSHIFT : 0;
    ++textIndex;
    return true;
}

bool processTextAction(void)
{
    return dispatchText(currentMacroAction.text.text, currentMacroAction.text.textLen);
}

//textEnd is allowed to be null if text is null-terminated
void reportErrorStatusString(const char* text, const char *textEnd)
{
    while(*text && errorStatusLen < ERROR_STATUS_BUFFER_LENGTH && (text < textEnd || textEnd == NULL)) {
        errorStatusBuffer[errorStatusLen] = *text;
        text++;
        errorStatusLen++;
    }
}

void reportErrorStatusBool(bool b)
{
    reportErrorStatusString(b ? "1" : "0", NULL);
}

void reportErrorStatusNum(uint8_t n)
{
    char buff[2];
    buff[0] = ' ';
    buff[1] = 0;
    reportErrorStatusString(buff, NULL);
    for(uint8_t div = 100; div > 0; div /= 10) {
        buff[0] = (char)(((uint8_t)(n/div)) + '0');
        n = n%div;
        reportErrorStatusString(buff, NULL);
    }
}

//Beware, currentMacroAction.text.text is *not* null-terminated!
bool tokenMatches(const char *a, const char *aEnd, const char *b)
{
    while(a < aEnd && *b) {
        if(*a <= 32 || a == aEnd || *b <= 32) {
            return (*a <= 32 || a == aEnd) && *b <= 32;
        }
        if(*a++ != *b++){
            return false;
        }
    }
    return (*a <= 32 || a == aEnd) && *b <= 32;
}

uint8_t tokLen(const char *a, const char *aEnd)
{
    uint8_t l = 0;
    while(*a > 32 && a < aEnd) {
        l++;
        a++;
    }
    return l;
}

const char* nextTok(const char* cmd, const char *cmdEnd)
{
    while(*cmd > 32 && cmd < cmdEnd)    {
        cmd++;
    }
    while(*cmd <= 32 && cmd < cmdEnd) {
        cmd++;
    }
    return cmd;
}

bool processSwitchKeymapCommand(const char* arg1, const char* arg1End)
{
    static uint8_t lastKeymapIdx = 0;    int tmpKeymapIdx = CurrentKeymapIndex;
    if(tokenMatches(arg1, arg1End, "last")) {
        SwitchKeymapById(lastKeymapIdx);
    }
    else {
        SwitchKeymapByAbbreviation(tokLen(arg1, arg1End), arg1);
    }
    lastKeymapIdx = tmpKeymapIdx;
    return false;
}

bool processSwitchLayerCommand(const char* arg1, const char* arg1End)
{
#define LAYER_STACK_SIZE 5
    static uint8_t layerIdxStack[LAYER_STACK_SIZE];
    static uint8_t layerIdxStackTop = 0;
    static uint8_t layerIdxStackSize = 0;
    static uint8_t lastLayerIdx = 0;
    uint8_t tmpLayerIdx = ToggledLayer;
    if(tokenMatches(arg1, arg1End, "previous")) {
        reportErrorStatusString("activating previous\n", NULL);
        if(layerIdxStackSize > 0) {
            layerIdxStackTop = (layerIdxStackTop + LAYER_STACK_SIZE - 1) % LAYER_STACK_SIZE;
            layerIdxStackSize--;
        }
        else {
            layerIdxStack[layerIdxStackTop] = LayerId_Base;
        }
        ToggleLayer(layerIdxStack[layerIdxStackTop]);
    }
    else {
        layerIdxStackTop = (layerIdxStackTop + 1) % LAYER_STACK_SIZE;
        if(tokenMatches(arg1, arg1End, "fn")) {
            reportErrorStatusString("activating fn\n", NULL);
            layerIdxStack[layerIdxStackTop] = LayerId_Fn;
        }
        else if(tokenMatches(arg1, arg1End, "mouse")) {
            layerIdxStack[layerIdxStackTop] = LayerId_Mouse;
        }
        else if(tokenMatches(arg1, arg1End, "mod")) {
            layerIdxStack[layerIdxStackTop] = LayerId_Mod;
        }
        else if(tokenMatches(arg1, arg1End, "base")) {
            layerIdxStack[layerIdxStackTop] = LayerId_Base;
        }
        else if(tokenMatches(arg1, arg1End, "last")) {
            layerIdxStack[layerIdxStackTop] = lastLayerIdx;
        }
        ToggleLayer(layerIdxStack[layerIdxStackTop]);
        layerIdxStackSize = layerIdxStackSize < LAYER_STACK_SIZE - 1 ? layerIdxStackSize+1 : layerIdxStackSize;
    }
    lastLayerIdx = tmpLayerIdx;
    return false;
}

bool processDelayUntilReleaseCommand()
{
    static bool inDelay;
    static uint32_t delayStart;

    if (inDelay) {
        if (Timer_GetElapsedTime(&delayStart) >= 50 && !currentMacroKey->current) {
            inDelay = false;
        }
    } else {
        delayStart = CurrentTime;
        inDelay = true;
    }
    return inDelay;
}

bool processIfDoubletapCommand(bool negate)
{
    if (Timer_GetElapsedTime(&previousMacroEndTime) <= 250 && currentMacroIndex == previousMacroIndex) {
        return true != negate;
    }
    return false != negate;
}

bool processBreakCommand()
{
    wantBreak = true;
    return false;
}

bool processErrorStatusCommand()
{
    bool res = dispatchText(errorStatusBuffer, errorStatusLen);
    if(!res) {
        errorStatusLen = 0;
    }
    return res;
}

bool processReportErrorCommand(const char* arg, const char *argEnd)
{
    reportErrorStatusString(arg, argEnd);
    return false;
}

bool processCommandAction(void)
{
    const char* cmd = currentMacroAction.text.text+1;
    const char* cmdEnd = currentMacroAction.text.text + currentMacroAction.text.textLen;
    while(*cmd) {
        const char* arg1 = nextTok(cmd, cmdEnd);
        if(tokenMatches(cmd, cmdEnd, "break")) {
            return processBreakCommand();
        }
        else if(tokenMatches(cmd, cmdEnd, "switchKeymap")) {
            return processSwitchKeymapCommand(arg1, cmdEnd);
        }
        else if(tokenMatches(cmd, cmdEnd, "switchLayer")) {
            reportErrorStatusString("switching layer with arg '", NULL);
            reportErrorStatusString(arg1, cmdEnd);
            reportErrorStatusString("' + ", NULL);
            reportErrorStatusString(cmdEnd, cmdEnd+1);
            return processSwitchLayerCommand(arg1, cmdEnd);
        }
        else if(tokenMatches(cmd, cmdEnd, "delayUntilRelease")) {
            return processDelayUntilReleaseCommand();
        }
        else if(tokenMatches(cmd, cmdEnd, "errorStatus")) {
            return processErrorStatusCommand();
        }
        else if(tokenMatches(cmd, cmdEnd, "reportError")) {
            return processReportErrorCommand(arg1, cmdEnd);
        }
        else if(tokenMatches(cmd, cmdEnd, "ifDoubletap")) {
            if(!processIfDoubletapCommand(false)) {
                return false;
            }
        }
        else if(tokenMatches(cmd, cmdEnd, "ifNotDoubletap")) {
            if(!processIfDoubletapCommand(true)) {
                return false;
            }
        }
        cmd = arg1;
    }
    return false;
}

bool processTextOrCommandAction(void)
{
    if(currentMacroAction.text.text[0] == '$') {
        return processCommandAction();
    }
    else {
        return processTextAction();
    }
}

bool processCurrentMacroAction(void)
{
    switch (currentMacroAction.type) {
        case MacroActionType_Delay:
            return processDelayAction();
        case MacroActionType_Key:
            return processKeyAction();
        case MacroActionType_MouseButton:
            return processMouseButtonAction();
        case MacroActionType_MoveMouse:
            return processMoveMouseAction();
        case MacroActionType_ScrollMouse:
            return processScrollMouseAction();
        case MacroActionType_Text:
            return processTextOrCommandAction();
    }
    return false;
}

void Macros_StartMacro(uint8_t index, key_state_t *keyState)
{
    if(MacroPlaying) {
        return;
    }
    MacroPlaying = true;
    currentMacroIndex = index;
    currentMacroActionIndex = 0;
    currentMacroKey = keyState;
    ValidatedUserConfigBuffer.offset = AllMacros[index].firstMacroActionOffset;
    ParseMacroAction(&ValidatedUserConfigBuffer, &currentMacroAction);
    memset(&MacroMouseReport, 0, sizeof MacroMouseReport);
    memset(&MacroBasicKeyboardReport, 0, sizeof MacroBasicKeyboardReport);
    memset(&MacroMediaKeyboardReport, 0, sizeof MacroMediaKeyboardReport);
    memset(&MacroSystemKeyboardReport, 0, sizeof MacroSystemKeyboardReport);
}

void Macros_ContinueMacro(void)
{
    if (processCurrentMacroAction() && !wantBreak) {
        return;
    }
    if (++currentMacroActionIndex == AllMacros[currentMacroIndex].macroActionsCount || wantBreak) {
        MacroPlaying = false;
        wantBreak = false;
        previousMacroIndex = currentMacroIndex;
        previousMacroEndTime = CurrentTime;
        return;
    }
    ParseMacroAction(&ValidatedUserConfigBuffer, &currentMacroAction);
}

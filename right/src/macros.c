#include "macros.h"
#include "config_parser/parse_macro.h"
#include "config_parser/config_globals.h"
#include "timer.h"
#include "keymap.h"
#include "usb_report_updater.h"
#include "led_display.h"
#include "macro_recorder.h"

macro_reference_t AllMacros[MAX_MACRO_NUM];
uint8_t AllMacrosCount;
bool MacroPlaying = false;
bool ReportsClaimed = false;
usb_mouse_report_t MacroMouseReport;
usb_basic_keyboard_report_t MacroBasicKeyboardReport;
usb_media_keyboard_report_t MacroMediaKeyboardReport;
usb_system_keyboard_report_t MacroSystemKeyboardReport;

static char statusBuffer[STATUS_BUFFER_MAX_LENGTH];
static uint16_t statusBufferLen;


static uint8_t lastKeymapIdx;
static uint8_t layerIdxStack[LAYER_STACK_SIZE];
static uint8_t layerIdxStackTop;
static uint8_t layerIdxStackSize;
static uint8_t lastLayerIdx;

static macro_state_t macroState[MACRO_STATE_POOL_SIZE];
static macro_state_t *s = macroState;

bool Macros_ClaimReports() {
    if(!ReportsClaimed) {
        ReportsClaimed = true;
        return true;
    }
    return false;
}

void Macros_ResetReportClaims() {
    ReportsClaimed = false;
}

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

void Macros_SignalInterrupt()
{
    for(uint8_t i = 0; i < MACRO_STATE_POOL_SIZE; i++) {
        if(macroState[i].macroPlaying) {
            macroState[i].macroInterrupted = true;
        }
    }
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

    if(!Macros_ClaimReports()) {
        return true;
    }

    switch (s->currentMacroAction.key.action) {
        case MacroSubAction_Tap:
            if (!s->keyActionPressStarted) {
                s->keyActionPressStarted = true;
                addModifiers(s->currentMacroAction.key.modifierMask);
                addScancode(s->currentMacroAction.key.scancode, s->currentMacroAction.key.type);
                return true;
            }
            s->keyActionPressStarted = false;
            deleteModifiers(s->currentMacroAction.key.modifierMask);
            deleteScancode(s->currentMacroAction.key.scancode, s->currentMacroAction.key.type);
            break;
        case MacroSubAction_Release:
            deleteModifiers(s->currentMacroAction.key.modifierMask);
            deleteScancode(s->currentMacroAction.key.scancode, s->currentMacroAction.key.type);
            break;
        case MacroSubAction_Press:
            addModifiers(s->currentMacroAction.key.modifierMask);
            addScancode(s->currentMacroAction.key.scancode, s->currentMacroAction.key.type);
            break;
    }
    return false;
}

bool processDelay(uint32_t time)
{
    if (s->delayActive) {
        if (Timer_GetElapsedTime(&s->delayStart) >= time) {
            s->delayActive = false;
        }
    } else {
        s->delayStart = CurrentTime;
        s->delayActive = true;
    }
    return s->delayActive;
}

bool processDelayAction() {
    return processDelay(s->currentMacroAction.delay.delay);
}

bool processMouseButtonAction(void)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    switch (s->currentMacroAction.key.action) {
        case MacroSubAction_Tap:
            if (!s->mouseButtonPressStarted) {
                s->mouseButtonPressStarted = true;
                MacroMouseReport.buttons |= s->currentMacroAction.mouseButton.mouseButtonsMask;
                return true;
            }
            s->mouseButtonPressStarted = false;
            MacroMouseReport.buttons &= ~s->currentMacroAction.mouseButton.mouseButtonsMask;
            break;
        case MacroSubAction_Release:
            MacroMouseReport.buttons &= ~s->currentMacroAction.mouseButton.mouseButtonsMask;
            break;
        case MacroSubAction_Press:
            MacroMouseReport.buttons |= s->currentMacroAction.mouseButton.mouseButtonsMask;
            break;
    }
    return false;
}

bool processMoveMouseAction(void)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    if (s->mouseMoveInMotion) {
        MacroMouseReport.x = 0;
        MacroMouseReport.y = 0;
        s->mouseMoveInMotion = false;
    } else {
        MacroMouseReport.x = s->currentMacroAction.moveMouse.x;
        MacroMouseReport.y = s->currentMacroAction.moveMouse.y;
        s->mouseMoveInMotion = true;
    }
    return s->mouseMoveInMotion;
}

bool processScrollMouseAction(void)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    if (s->mouseScrollInMotion) {
        MacroMouseReport.wheelX = 0;
        MacroMouseReport.wheelY = 0;
        s->mouseScrollInMotion = false;
    } else {
        MacroMouseReport.wheelX = s->currentMacroAction.scrollMouse.x;
        MacroMouseReport.wheelY = s->currentMacroAction.scrollMouse.y;
        s->mouseScrollInMotion = true;
    }
    return s->mouseScrollInMotion;
}

//textEnd is allowed to be null if text is null-terminated
void setStatusString(const char* text, const char *textEnd)
{
    while(*text && statusBufferLen < STATUS_BUFFER_MAX_LENGTH && (text < textEnd || textEnd == NULL)) {
        statusBuffer[statusBufferLen] = *text;
        text++;
        statusBufferLen++;
    }
}

void setStatusBool(bool b)
{
    setStatusString(b ? "1" : "0", NULL);
}

void setStatusNum(uint32_t n)
{
    uint32_t orig = n;
    char buff[2];
    buff[0] = ' ';
    buff[1] = '\0';
    setStatusString(buff, NULL);
    for(uint32_t div = 1000000000; div > 0; div /= 10) {
        buff[0] = (char)(((uint8_t)(n/div)) + '0');
        n = n%div;
        if(n!=orig || div == 1) {
          setStatusString(buff, NULL);
        }
    }
}

void reportError(const char* err, const char* arg, const char *argEnd)
{
    LedDisplay_SetText(3, "ERR");
    setStatusString("line ", NULL);
    setStatusNum(s->currentMacroActionIndex);
    setStatusString(": ", NULL);
    setStatusString(err, NULL);
    if(arg != NULL) {
        setStatusString(": ", NULL);
        setStatusString(arg, argEnd);
    }
    setStatusString("\n", NULL);
}

void Macros_ReportError(const char* err, const char* arg, const char *argEnd)
{
    LedDisplay_SetText(3, "ERR");
    setStatusString(err, NULL);
    if(arg != NULL) {
        setStatusString(": ", NULL);
        setStatusString(arg, argEnd);
    }
    setStatusString("\n", NULL);
}

void Macros_ReportErrorNum(const char* err, uint32_t num)
{
    LedDisplay_SetText(3, "ERR");
    setStatusString(err, NULL);
    setStatusNum(num);
    setStatusString("\n", NULL);
}

void printReport(usb_basic_keyboard_report_t *report) {
    setStatusNum(report->modifiers);
    for(int i = 0; i < 6; i++) {
        setStatusNum(report->scancodes[i]);
    }
    setStatusString("\n", NULL);
}

bool dispatchText(const char* text, uint16_t textLen)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    char character;
    uint8_t scancode;

    if (s->dispatchTextIndex == textLen) {
        s->dispatchTextIndex = 0;
        s->dispatchReportIndex = USB_BASIC_KEYBOARD_MAX_KEYS;
        memset(&MacroBasicKeyboardReport, 0, sizeof MacroBasicKeyboardReport);
        return false;
    }
    if (s->dispatchReportIndex == USB_BASIC_KEYBOARD_MAX_KEYS) {
        s->dispatchReportIndex = 0;
        memset(&MacroBasicKeyboardReport, 0, sizeof MacroBasicKeyboardReport);
        return true;
    }
    character = text[s->dispatchTextIndex];
    scancode = characterToScancode(character);
    for (uint8_t i = 0; i < s->dispatchReportIndex; i++) {
        if (MacroBasicKeyboardReport.scancodes[i] == scancode) {
            s->dispatchReportIndex = USB_BASIC_KEYBOARD_MAX_KEYS;
            return true;
        }
    }
    MacroBasicKeyboardReport.scancodes[s->dispatchReportIndex++] = scancode;
    MacroBasicKeyboardReport.modifiers = characterToShift(character) ? HID_KEYBOARD_MODIFIER_LEFTSHIFT : 0;
    ++s->dispatchTextIndex;
    return true;
}

bool processTextAction(void)
{
    return dispatchText(s->currentMacroAction.text.text, s->currentMacroAction.text.textLen);
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

uint32_t parseUInt32(const char *a, const char *aEnd)
{
    uint32_t n = 0;
    while(*a > 32 && a < aEnd) {
        n = n*10 + ((uint8_t)(*a))-48;
        a++;
    }
    return n;
}

bool processSwitchKeymapCommand(const char* arg1, const char* arg1End)
{
    int tmpKeymapIdx = CurrentKeymapIndex;
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
    uint8_t tmpLayerIdx = ToggledLayer;
    if(tokenMatches(arg1, arg1End, "previous")) {
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
        else {
            reportError("unrecognized layer id", arg1, arg1End);
        }
        ToggleLayer(layerIdxStack[layerIdxStackTop]);
        layerIdxStackSize = layerIdxStackSize < LAYER_STACK_SIZE - 1 ? layerIdxStackSize+1 : layerIdxStackSize;
    }
    lastLayerIdx = tmpLayerIdx;
    return false;
}

bool processDelayUntilReleaseCommand()
{
    if(s->currentMacroKey->current) {
        return true;
    }
    return processDelay(50);
}

bool processIfDoubletapCommand(bool negate)
{
    bool doubletapFound = false;

    for(uint8_t i = 0; i < MACRO_STATE_POOL_SIZE; i++) {
        if (macroState[i].macroPlaying && Timer_GetElapsedTime(&macroState[i].previousMacroEndTime) <= 250 && s->currentMacroIndex == macroState[i].previousMacroIndex) {
            doubletapFound = true;
        }
        if (!macroState[i].macroPlaying && Timer_GetElapsedTime(&macroState[i].previousMacroEndTime) <= 250 && s->currentMacroIndex == macroState[i].currentMacroIndex) {
            doubletapFound = true;
        }
    }

    return doubletapFound != negate;
}

bool processIfModifierCommand(bool negate, uint8_t modmask)
{
    return ((OldModifierState & modmask) > 0) != negate;
}

bool processIfPlaytimeCommand(bool negate, const char* arg, const char *argEnd)
{
    uint32_t timeout = parseUInt32(arg, argEnd);
    uint32_t delay = Timer_GetElapsedTime(&s->currentMacroStartTime);
    return (delay > timeout) != negate;
}

bool processIfInterruptedCommand(bool negate)
{
   return s->macroInterrupted != negate;
}

bool processBreakCommand()
{
    s->macroBroken = true;
    return false;
}

bool processPrintStatusCommand()
{
    bool res = dispatchText(statusBuffer, statusBufferLen);
    if(!res) {
        statusBufferLen = 0;
    }
    LedDisplay_UpdateText();
    return res;
}

bool processSetStatusCommand(const char* arg, const char *argEnd)
{
    setStatusString(arg, argEnd);
    return false;
}

bool processGoToCommand(const char* arg, const char *argEnd)
{
    uint8_t address = parseUInt32(arg, argEnd);
    s->currentMacroActionIndex = address - 1;
    ValidatedUserConfigBuffer.offset = AllMacros[s->currentMacroIndex].firstMacroActionOffset;
    for(uint8_t i = 0; i < address; i++) {
        ParseMacroAction(&ValidatedUserConfigBuffer, &s->currentMacroAction);
    }
    s->bufferOffset = ValidatedUserConfigBuffer.offset;
    return false;
}

bool processMouseCommand(bool enable, const char* arg1, const char *argEnd)
{
    const char* arg2 = nextTok(arg1, argEnd);
    uint8_t dirOffset = 0;

    serialized_mouse_action_t baseAction = SerializedMouseAction_LeftClick;

    if(tokenMatches(arg1, argEnd, "move")) {
        baseAction = SerializedMouseAction_MoveUp;
    }
    else if(tokenMatches(arg1, argEnd, "scroll")) {
        baseAction = SerializedMouseAction_ScrollUp;
    }
    else if(tokenMatches(arg1, argEnd, "accelerate")) {
        baseAction = SerializedMouseAction_Accelerate;
    }
    else if(tokenMatches(arg1, argEnd, "decelerate")) {
        baseAction = SerializedMouseAction_Decelerate;
    }
    else {
        reportError("unrecognized argument", arg1, argEnd);
    }

    if(baseAction == SerializedMouseAction_MoveUp || baseAction == SerializedMouseAction_ScrollUp) {
        if(tokenMatches(arg2, argEnd, "up")) {
            dirOffset = 0;
        }
        else if(tokenMatches(arg2, argEnd, "down")) {
            dirOffset = 1;
        }
        else if(tokenMatches(arg2, argEnd, "left")) {
            dirOffset = 2;
        }
        else if(tokenMatches(arg2, argEnd, "right")) {
            dirOffset = 3;
        }
        else {
            reportError("unrecognized argument", arg2, argEnd);
        }
    }

    if(baseAction != SerializedMouseAction_LeftClick) {
        ToggleMouseState(baseAction + dirOffset, enable);
    }
    return false;
}

bool processRecordMacroCommand(const char* arg, const char *argEnd)
{
    uint8_t id = arg == argEnd ? 0 : *arg;
    RecordRuntimeMacroSmart(id);
    return false;
}

bool processPlayMacroCommand(const char* arg, const char *argEnd)
{
    uint8_t id = arg == argEnd ? 0 : *arg;
    return PlayRuntimeMacroSmart(id);
}

bool processCommandAction(void)
{
    const char* cmd = s->currentMacroAction.text.text+1;
    const char* cmdEnd = s->currentMacroAction.text.text + s->currentMacroAction.text.textLen;
    while(*cmd) {
        const char* arg1 = nextTok(cmd, cmdEnd);
        switch(*cmd) {
        case 'b':
            if(tokenMatches(cmd, cmdEnd, "break")) {
                return processBreakCommand();
            }
            else {
                goto failed;
            }
            break;
        case 'd':
            if(tokenMatches(cmd, cmdEnd, "delayUntilRelease")) {
                return processDelayUntilReleaseCommand();
            }
            else {
                goto failed;
            }
            break;
        case 'g':
            if(tokenMatches(cmd, cmdEnd, "goTo")) {
                return processGoToCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'i':
            if(tokenMatches(cmd, cmdEnd, "ifDoubletap")) {
                if(!processIfDoubletapCommand(false) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotDoubletap")) {
                if(!processIfDoubletapCommand(true) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifInterrupted")) {
                if(!processIfInterruptedCommand(false) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotInterrupted")) {
                if(!processIfInterruptedCommand(true) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifPlaytime")) {
                if(!processIfPlaytimeCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = nextTok(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotPlaytime")) {
                if(!processIfPlaytimeCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = nextTok(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "ifShift")) {
                if(!processIfModifierCommand(false, SHIFTMASK)  && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotShift")) {
                if(!processIfModifierCommand(true, SHIFTMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifCtrl")) {
                if(!processIfModifierCommand(false, CTRLMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotCtrl")) {
                if(!processIfModifierCommand(true, CTRLMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifAlt")) {
                if(!processIfModifierCommand(false, ALTMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotAlt")) {
                if(!processIfModifierCommand(true, ALTMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else {
                goto failed;
            }
            break;
        case 'p':
            if(tokenMatches(cmd, cmdEnd, "printStatus")) {
                return processPrintStatusCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "playMacro")) {
                return processPlayMacroCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'r':
            if(tokenMatches(cmd, cmdEnd, "recordMacro")) {
                return processRecordMacroCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 's':
            if(tokenMatches(cmd, cmdEnd, "setStatus")) {
                return processSetStatusCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "switchKeymap")) {
                return processSwitchKeymapCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "switchLayer")) {
                return processSwitchLayerCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "startMouse")) {
                return processMouseCommand(true, arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "stopMouse")) {
                return processMouseCommand(false, arg1, cmdEnd);
            }
            else {
                goto failed;
            }
        default:
        failed:
            reportError("unrecognized command", cmd, cmdEnd);
            break;
        }
        cmd = arg1;
    }
    return false;
}

bool processTextOrCommandAction(void)
{
    if(s->currentMacroAction.text.text[0] == '$') {
        bool actionInProgress = processCommandAction();
        s->currentConditionPassed = actionInProgress;
        return actionInProgress;
    }
    else {
        return processTextAction();
    }
}

bool processCurrentMacroAction(void)
{
    switch (s->currentMacroAction.type) {
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

bool findFreeStateSlot() {
    for(uint8_t i = 0; i < MACRO_STATE_POOL_SIZE; i++) {
        if(!macroState[i].macroPlaying) {
            s = &macroState[i];
            return true;
        }
    }
    reportError("Too many macros running at one time", "", NULL);
    return false;
}

void Macros_StartMacro(uint8_t index, key_state_t *keyState)
{
    if(!findFreeStateSlot()) {
        return;
    }
    MacroPlaying = true;
    s->macroPlaying = true;
    s->macroInterrupted = false;
    s->currentMacroIndex = index;
    s->currentMacroActionIndex = 0;
    s->currentMacroKey = keyState;
    s->currentMacroStartTime = CurrentTime;
    s->currentConditionPassed = false;
    ValidatedUserConfigBuffer.offset = AllMacros[index].firstMacroActionOffset;
    ParseMacroAction(&ValidatedUserConfigBuffer, &s->currentMacroAction);
    s->bufferOffset = ValidatedUserConfigBuffer.offset;
    memset(&MacroMouseReport, 0, sizeof MacroMouseReport);
    memset(&MacroBasicKeyboardReport, 0, sizeof MacroBasicKeyboardReport);
    memset(&MacroMediaKeyboardReport, 0, sizeof MacroMediaKeyboardReport);
    memset(&MacroSystemKeyboardReport, 0, sizeof MacroSystemKeyboardReport);
}

void continueMacro(void)
{
    if (processCurrentMacroAction() && !s->macroBroken) {
        return;
    }
    if (++s->currentMacroActionIndex == AllMacros[s->currentMacroIndex].macroActionsCount || s->macroBroken) {
        s->macroPlaying = false;
        s->macroBroken = false;
        s->previousMacroIndex = s->currentMacroIndex;
        s->previousMacroEndTime = CurrentTime;
        return;
    }
    ValidatedUserConfigBuffer.offset = s->bufferOffset;
    ParseMacroAction(&ValidatedUserConfigBuffer, &s->currentMacroAction);
    s->bufferOffset = ValidatedUserConfigBuffer.offset;
    s->currentConditionPassed = false;
}

void Macros_ContinueMacro(void)
{
    bool someonePlaying = false;
    for(uint8_t i = 0; i < MACRO_STATE_POOL_SIZE; i++) {
        if(macroState[i].macroPlaying) {
            someonePlaying = true;
            s = &macroState[i];
            continueMacro();
        }
    }
    MacroPlaying &= someonePlaying;
}

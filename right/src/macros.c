#include "macros.h"
#include "config_parser/parse_macro.h"
#include "config_parser/config_globals.h"
#include "timer.h"
#include "keymap.h"
#include "key_matrix.h"
#include "usb_report_updater.h"
#include "led_display.h"
#include "postponer.h"
#include "macro_recorder.h"

macro_reference_t AllMacros[MAX_MACRO_NUM];
uint8_t AllMacrosCount;
bool MacroPlaying = false;
usb_mouse_report_t MacroMouseReport;
usb_basic_keyboard_report_t MacroBasicKeyboardReport;
usb_media_keyboard_report_t MacroMediaKeyboardReport;
usb_system_keyboard_report_t MacroSystemKeyboardReport;

uint8_t MacroBasicScancodeIndex = 0;
uint8_t MacroMediaScancodeIndex = 0;
uint8_t MacroSystemScancodeIndex = 0;

static char statusBuffer[STATUS_BUFFER_MAX_LENGTH];
static uint16_t statusBufferLen;

static layerStackRecord layerIdxStack[LAYER_STACK_SIZE];
static uint8_t layerIdxStackTop;
static uint8_t layerIdxStackSize;
static uint8_t lastLayerIdx;
static uint8_t lastKeymapIdx;

static int32_t regs[32];

static bool initialized = false;

macro_state_t MacroState[MACRO_STATE_POOL_SIZE];
static macro_state_t *s = MacroState;

bool Macros_ClaimReports() {
    s->reportsUsed = true;
    return true;
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
        if(MacroState[i].macroPlaying) {
            MacroState[i].macroInterrupted = true;
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
        if (s->macroBasicKeyboardReport.scancodes[i] == scancode) {
            return;
        }
    }
    for (uint8_t i = 0; i < USB_BASIC_KEYBOARD_MAX_KEYS; i++) {
        if (!s->macroBasicKeyboardReport.scancodes[i]) {
            s->macroBasicKeyboardReport.scancodes[i] = scancode;
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
        if (s->macroBasicKeyboardReport.scancodes[i] == scancode) {
            s->macroBasicKeyboardReport.scancodes[i] = 0;
            return;
        }
    }
}

void addModifiers(uint8_t modifiers)
{
    s->macroBasicKeyboardReport.modifiers |= modifiers;
}

void deleteModifiers(uint8_t modifiers)
{
    s->macroBasicKeyboardReport.modifiers &= ~modifiers;
}

void addMediaScancode(uint16_t scancode)
{
    if (!scancode) {
        return;
    }
    for (uint8_t i = 0; i < USB_MEDIA_KEYBOARD_MAX_KEYS; i++) {
        if (s->macroMediaKeyboardReport.scancodes[i] == scancode) {
            return;
        }
    }
    for (uint8_t i = 0; i < USB_MEDIA_KEYBOARD_MAX_KEYS; i++) {
        if (!s->macroMediaKeyboardReport.scancodes[i]) {
            s->macroMediaKeyboardReport.scancodes[i] = scancode;
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
        if (s->macroMediaKeyboardReport.scancodes[i] == scancode) {
            s->macroMediaKeyboardReport.scancodes[i] = 0;
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
        if (s->macroSystemKeyboardReport.scancodes[i] == scancode) {
            return;
        }
    }
    for (uint8_t i = 0; i < USB_SYSTEM_KEYBOARD_MAX_KEYS; i++) {
        if (!s->macroSystemKeyboardReport.scancodes[i]) {
            s->macroSystemKeyboardReport.scancodes[i] = scancode;
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
        if (s->macroSystemKeyboardReport.scancodes[i] == scancode) {
            s->macroSystemKeyboardReport.scancodes[i] = 0;
            return;
        }
    }
}

void addScancode(uint16_t scancode, keystroke_type_t type)
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

void deleteScancode(uint16_t scancode, keystroke_type_t type)
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

bool processKey(macro_action_t macro_action)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    macro_sub_action_t action = macro_action.key.action;
    keystroke_type_t type = macro_action.key.type;
    uint8_t modifierMask = macro_action.key.modifierMask;
    uint16_t scancode = macro_action.key.scancode;

    switch (action) {
        case MacroSubAction_Hold:
        case MacroSubAction_Tap:
            if (s->pressPhase == 0) {
                s->pressPhase = 1;
                addModifiers(modifierMask);
                if (modifierMask != 0 && SplitCompositeKeystroke != 0) {
                    return true;
                }
            }
            if (s->pressPhase == 1) {
                s->pressPhase = 2;
                addScancode(scancode, type);
                return true;
            }
            if (s->pressPhase == 2) {
                if(ACTIVE(s->currentMacroKey) && action == MacroSubAction_Hold) {
                    return true;
                }
                s->pressPhase = 3;
            }
            if (s->pressPhase == 3) {
                s->pressPhase = 0;
                deleteModifiers(modifierMask);
                deleteScancode(scancode, type);
            }
            break;
        case MacroSubAction_Release:
            deleteModifiers(modifierMask);
            deleteScancode(scancode, type);
            break;
        case MacroSubAction_Press:
            addModifiers(modifierMask);
            addScancode(scancode, type);
            break;
    }
    return false;
}

bool processKeyAction()
{
    return processKey(s->currentMacroAction);
}

bool processMouseButton(macro_action_t macro_action)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    uint8_t mouseButtonMask = macro_action.mouseButton.mouseButtonsMask;
    macro_sub_action_t action = macro_action.mouseButton.action;

    switch (macro_action.mouseButton.action) {
        case MacroSubAction_Hold:
        case MacroSubAction_Tap:
            if (s->pressPhase == 0) {
                s->macroMouseReport.buttons |= mouseButtonMask;
                s->pressPhase = 1;
                return true;
            }
            if (s->pressPhase == 1) {
                if(ACTIVE(s->currentMacroKey) && action == MacroSubAction_Hold) {
                    return true;
                }
                s->pressPhase = 2;
            }
            if (s->pressPhase == 2) {
                s->macroMouseReport.buttons &= ~mouseButtonMask;
                s->pressPhase = 0;
            }
            break;
        case MacroSubAction_Release:
            s->macroMouseReport.buttons &= ~mouseButtonMask;
            break;
        case MacroSubAction_Press:
            s->macroMouseReport.buttons |= mouseButtonMask;
            break;
    }
    return false;
}

bool processMouseButtonAction(void) {
    return processMouseButton(s->currentMacroAction);
}

bool processMoveMouseAction(void)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    if (s->mouseMoveInMotion) {
        s->macroMouseReport.x = 0;
        s->macroMouseReport.y = 0;
        s->mouseMoveInMotion = false;
    } else {
        s->macroMouseReport.x = s->currentMacroAction.moveMouse.x;
        s->macroMouseReport.y = s->currentMacroAction.moveMouse.y;
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
        s->macroMouseReport.wheelX = 0;
        s->macroMouseReport.wheelY = 0;
        s->mouseScrollInMotion = false;
    } else {
        s->macroMouseReport.wheelX = s->currentMacroAction.scrollMouse.x;
        s->macroMouseReport.wheelY = s->currentMacroAction.scrollMouse.y;
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
    uint8_t mods;

    uint8_t max_keys = USB_BASIC_KEYBOARD_MAX_KEYS/2;

    if (s->dispatchTextIndex == textLen) {
        s->dispatchTextIndex = 0;
        s->dispatchReportIndex = max_keys;
        memset(&s->macroBasicKeyboardReport, 0, sizeof s->macroBasicKeyboardReport);
        return false;
    }
    if (s->dispatchReportIndex == max_keys) {
        s->dispatchReportIndex = 0;
        memset(&s->macroBasicKeyboardReport, 0, sizeof s->macroBasicKeyboardReport);
        return true;
    }
    character = text[s->dispatchTextIndex];
    mods = characterToShift(character) ? HID_KEYBOARD_MODIFIER_LEFTSHIFT : 0;
    scancode = characterToScancode(character);
    for (uint8_t i = 0; i < s->dispatchReportIndex; i++) {
        if (s->macroBasicKeyboardReport.scancodes[i] == scancode) {
            s->dispatchReportIndex = max_keys;
            return true;
        }
    }
    if(mods != 0 && (s->macroBasicKeyboardReport.modifiers & mods) != mods && SplitCompositeKeystroke) {
        s->macroBasicKeyboardReport.modifiers = mods;
    } else {
        s->macroBasicKeyboardReport.scancodes[s->dispatchReportIndex++] = scancode;
        s->macroBasicKeyboardReport.modifiers = mods;
        ++s->dispatchTextIndex;
    }
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

int32_t parseInt32(const char *a, const char *aEnd)
{
    if(*a == '#') {
        a++;
        uint8_t adr = parseInt32(a, aEnd);
        return regs[adr];
    }
    else
    {
        bool negate = false;
        if(*a == '-')
        {
            negate = !negate;
            a++;
        }
        int32_t n = 0;
        bool numFound = false;
        while(*a > 47 && *a < 58 && a < aEnd) {
            n = n*10 + ((uint8_t)(*a))-48;
            a++;
            numFound = true;
        }
        if(negate)
        {
            n = -n;
        }
        if(!numFound) {
            reportError("Integer expected", NULL, NULL);
        }
        return n;
    }
}

void removeStackTop(void) {
    layerIdxStackTop = (layerIdxStackTop + LAYER_STACK_SIZE - 1) % LAYER_STACK_SIZE;
    layerIdxStackSize--;
}

void popLayerStack(bool forceRemoveTop) {
    if(layerIdxStackSize > 0 && forceRemoveTop) {
        removeStackTop();
    }
    while(layerIdxStackSize > 0 && layerIdxStack[layerIdxStackTop].removed) {
        removeStackTop();
    }
    if(layerIdxStackSize == 0) {
        layerIdxStack[layerIdxStackTop].layer = LayerId_Base;
        layerIdxStack[layerIdxStackTop].removed = false;
        layerIdxStack[layerIdxStackTop].held = false;
    }
    if(layerIdxStack[layerIdxStackTop].keymap != CurrentKeymapIndex) {
        SwitchKeymapById(layerIdxStack[layerIdxStackTop].keymap);
    }
    ToggleLayer(layerIdxStack[layerIdxStackTop].layer);
}

void Macros_UpdateLayerStack() {
    for(int i = 0; i < LAYER_STACK_SIZE; i++) {
        layerIdxStack[i].keymap = CurrentKeymapIndex;
    }
}

void pushStack(uint8_t layer, uint8_t keymap, bool hold) {
    layerIdxStackTop = (layerIdxStackTop + 1) % LAYER_STACK_SIZE;
    layerIdxStack[layerIdxStackTop].layer = layer;
    layerIdxStack[layerIdxStackTop].keymap = keymap;
    layerIdxStack[layerIdxStackTop].held = hold;
    layerIdxStack[layerIdxStackTop].removed = false;
    if(keymap != CurrentKeymapIndex) {
        SwitchKeymapById(keymap);
    }
    ToggleLayer(layerIdxStack[layerIdxStackTop].layer);
    layerIdxStackSize = layerIdxStackSize < LAYER_STACK_SIZE - 1 ? layerIdxStackSize+1 : layerIdxStackSize;
}

uint8_t parseKeymapId(const char* arg1, const char* cmdEnd) {
    if(tokenMatches(arg1, cmdEnd, "last")) {
        return lastKeymapIdx;
    } else {
        uint8_t idx = FindKeymapByAbbreviation(tokLen(arg1, cmdEnd), arg1);
        if(idx == 0xFF) {
            reportError("Keymap not recognized: ", arg1, cmdEnd);
        }
        return idx;
    }
}

uint8_t parseLayerId(const char* arg1, const char* cmdEnd) {
    if(tokenMatches(arg1, cmdEnd, "fn")) {
        return LayerId_Fn;
    }
    else if(tokenMatches(arg1, cmdEnd, "mouse")) {
        return LayerId_Mouse;
    }
    else if(tokenMatches(arg1, cmdEnd, "mod")) {
        return LayerId_Mod;
    }
    else if(tokenMatches(arg1, cmdEnd, "base")) {
        return LayerId_Base;
    }
    else if(tokenMatches(arg1, cmdEnd, "last")) {
        return lastLayerIdx;
    }
    else {
        return false;
    }
}

bool processSwitchKeymapCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpKeymapIdx = CurrentKeymapIndex;
    {
        uint8_t newKeymapIdx = parseKeymapId(arg1, cmdEnd);
        SwitchKeymapById(newKeymapIdx);
        Macros_UpdateLayerStack();
    }
    lastKeymapIdx = tmpKeymapIdx;
    return false;
}

bool processSwitchKeymapLayerCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpLayerIdx = ToggledLayer;
    pushStack(parseLayerId(nextTok(arg1, cmdEnd), cmdEnd), parseKeymapId(arg1, cmdEnd), false);
    lastLayerIdx = tmpLayerIdx;
    return false;
}

bool processSwitchLayerCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpLayerIdx = ToggledLayer;
    if(tokenMatches(arg1, cmdEnd, "previous")) {
        popLayerStack(true);
    }
    else {
        pushStack(parseLayerId(arg1, cmdEnd), CurrentKeymapIndex, false);
    }
    lastLayerIdx = tmpLayerIdx;
    return false;
}


bool processHoldLayer(uint8_t layer, uint8_t keymap, uint16_t timeout)
{
    if(!s->holdActive) {
        s->holdActive = true;
        pushStack(layer, keymap, true);
        s->holdLayerIdx = layerIdxStackTop;
        return true;
    }
    else {
        if(ACTIVE(s->currentMacroKey) && (Timer_GetElapsedTime(&s->currentMacroStartTime) < timeout || s->macroInterrupted)) {
            return true;
        }
        else {
            s->holdActive = false;
            layerIdxStack[s->holdLayerIdx].removed = true;
            layerIdxStack[s->holdLayerIdx].held = false;
            popLayerStack(false);
            return false;
        }
    }
}

bool Macros_IsLayerHeld() {
    return layerIdxStack[layerIdxStackTop].held;
}

bool processHoldLayerCommand(const char* arg1, const char* cmdEnd)
{
    return processHoldLayer(parseLayerId(arg1, cmdEnd), CurrentKeymapIndex, 0xFFFF);
}

bool processHoldLayerMaxCommand(const char* arg1, const char* cmdEnd)
{
    const char* arg2 = nextTok(arg1, cmdEnd);
    return processHoldLayer(parseLayerId(arg1, cmdEnd), CurrentKeymapIndex, parseInt32(arg2, cmdEnd));
}

bool processHoldKeymapLayerCommand(const char* arg1, const char* cmdEnd)
{
    const char* arg2 = nextTok(arg1, cmdEnd);
    return processHoldLayer(parseLayerId(arg2, cmdEnd), parseKeymapId(arg1, cmdEnd), 0xFFFF);
}

bool processHoldKeymapLayerMaxCommand(const char* arg1, const char* cmdEnd)
{
    const char* arg2 = nextTok(arg1, cmdEnd);
    const char* arg3 = nextTok(arg2, cmdEnd);
    return processHoldLayer(parseLayerId(arg2, cmdEnd), parseKeymapId(arg1, cmdEnd), parseInt32(arg3, cmdEnd));
}

bool processDelayUntilReleaseMaxCommand(const char* arg1, const char* cmdEnd)
{
    uint32_t timeout = parseInt32(arg1, cmdEnd);
    if(ACTIVE(s->currentMacroKey) && Timer_GetElapsedTime(&s->currentMacroStartTime) < timeout) {
        return true;
    }
    return false;
}

bool processDelayUntilReleaseCommand()
{
    if(ACTIVE(s->currentMacroKey)) {
        return true;
    }
    return false;
}

bool processDelayUntilCommand(const char* arg1, const char* cmdEnd)
{
    uint32_t time = parseInt32(arg1,  cmdEnd);
    return processDelay(time);
}

bool processRecordMacroDelayCommand()
{
    if(ACTIVE(s->currentMacroKey)) {
        return true;
    }
    uint16_t delay = Timer_GetElapsedTime(&s->currentMacroStartTime);
    MacroRecorder_RecordDelay(delay);
    return false;
}

bool processIfDoubletapCommand(bool negate)
{
    bool doubletapFound = false;

    for(uint8_t i = 0; i < MACRO_STATE_POOL_SIZE; i++) {
        if (MacroState[i].macroPlaying && Timer_GetElapsedTime(&MacroState[i].previousMacroEndTime) <= 250 && s->currentMacroIndex == MacroState[i].previousMacroIndex) {
            doubletapFound = true;
        }
        if (!MacroState[i].macroPlaying && Timer_GetElapsedTime(&MacroState[i].previousMacroEndTime) <= 250 && s->currentMacroIndex == MacroState[i].currentMacroIndex) {
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
    uint32_t timeout = parseInt32(arg, argEnd);
    uint32_t delay = Timer_GetElapsedTime(&s->currentMacroStartTime);
    return (delay > timeout) != negate;
}

bool processIfInterruptedCommand(bool negate)
{
   return s->macroInterrupted != negate;
}


bool processIfRegEqCommand(bool negate, const char* arg1, const char *argEnd)
{
    uint8_t address = parseInt32(arg1, argEnd);
    uint8_t param = parseInt32(nextTok(arg1, argEnd), argEnd);
    bool res = regs[address] == param;
    return res != negate;
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

bool processSetRegCommand(const char* arg1, const char *argEnd)
{
    uint8_t address = parseInt32(arg1, argEnd);
    int32_t param = parseInt32(nextTok(arg1, argEnd), argEnd);
    regs[address] = param;
    return false;
}

bool processRegAddCommand(const char* arg1, const char *argEnd, bool invert)
{
    uint8_t address = parseInt32(arg1, argEnd);
    int32_t param = parseInt32(nextTok(arg1, argEnd), argEnd);
    //substract is needed when referencing other registers via param
    if(invert) {
        regs[address] = regs[address] - param;
    } else {
        regs[address] = regs[address] + param;
    }
    return false;
}

bool goTo(uint8_t address)
{
    s->currentMacroActionIndex = address - 1;
    ValidatedUserConfigBuffer.offset = AllMacros[s->currentMacroIndex].firstMacroActionOffset;
    for(uint8_t i = 0; i < address; i++) {
        ParseMacroAction(&ValidatedUserConfigBuffer, &s->currentMacroAction);
    }
    s->bufferOffset = ValidatedUserConfigBuffer.offset;
    return false;
}

bool processGoToCommand(const char* arg, const char *argEnd)
{
    uint8_t address = parseInt32(arg, argEnd);
    return goTo(address);
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
    MacroRecorder_RecordRuntimeMacroSmart(id);
    return false;
}

bool processPlayMacroCommand(const char* arg, const char *argEnd)
{
    uint8_t id = arg == argEnd ? 0 : *arg;
    return MacroRecorder_PlayRuntimeMacroSmart(id, &s->macroBasicKeyboardReport);
}

bool processWriteCommand(const char* arg, const char *argEnd)
{
    return dispatchText(arg, argEnd - arg);
}

bool processSuppressModsCommand()
{
    SuppressMods = true;
    return false;
}

bool processSuppressKeysCommand()
{
    SuppressKeys = true;
    return false;
}

bool processPostponeKeysCommand()
{
    PostponeKeys = true;
    return false;
}

bool processSetStickyModsEnabledCommand(const char* arg, const char *argEnd)
{
    uint8_t enabled = parseInt32(arg,  argEnd);
    StickyModifiersEnabled = enabled;
    return false;
}

bool processSetActivateOnReleaseCommand(const char* arg, const char *argEnd)
{
    uint8_t enabled = parseInt32(arg,  argEnd);
    ActivateOnRelease = enabled;
    return false;
}

bool processSetSplitCompositeKeystrokeCommand(const char* arg, const char *argEnd)
{
    SplitCompositeKeystroke  = parseInt32(arg,  argEnd);
    return false;
}

bool processSetKeystrokeDelayCommand(const char* arg, const char *argEnd)
{
    KeystrokeDelay  = parseInt32(arg,  argEnd);
    KeystrokeDelay = KeystrokeDelay < 250 ? KeystrokeDelay : 250;
    return false;
}

bool processSetDebounceDelayCommand(const char* arg, const char *argEnd)
{
    uint16_t delay = parseInt32(arg,  argEnd);
    delay = delay < 250 ? delay : 250;
    DebounceTimePress = delay;
    DebounceTimeRelease = delay;
    return false;
}

bool processStatsRuntimeCommand()
{
    int ms = Timer_GetElapsedTime(&s->currentMacroStartTime);
    setStatusString("macro runtime is: ", NULL);
    setStatusNum(ms);
    setStatusString(" ms\n", NULL);
    return false;
}


bool processNoOpCommand()
{
    return false;
}


void postponeNextN(uint8_t count) {
    s->postponeNext = count + 1;
}

bool processResolveSecondary(uint16_t timeout1, uint16_t timeout2, uint8_t primaryAdr, uint8_t secondaryAdr) {
    PostponeKeys = true;

    //phase 1 - wait until some other key is released, then write down its release time
    bool timer1Exceeded = Timer_GetElapsedTime(&s->currentMacroStartTime) >= timeout1;
    if(!timer1Exceeded && ACTIVE(s->currentMacroKey) && !Postponer_IsPendingReleased()) {
        s->resolveSecondaryPhase2StartTime = 0;
        return true;
    }
    if(s->resolveSecondaryPhase2StartTime == 0) {
        s->resolveSecondaryPhase2StartTime = CurrentTime;
    }
    //phase 2 - "safety margin" - wait another `timeout2` ms, and if the switcher is released during this time, still interpret it as a primary action
    bool timer2Exceeded = Timer_GetElapsedTime(&s->resolveSecondaryPhase2StartTime) >= timeout2;
    if(!timer1Exceeded && !timer2Exceeded && ACTIVE(s->currentMacroKey) && Postponer_IsPendingReleased() && Postponer_PendingCount() < 3) {
        return true;
    }
    //phase 3 - resolve the situation - if the switcher is released first or within the "safety margin", interpret it as primary action, otherwise secondary
    if(timer1Exceeded || (Postponer_IsPendingReleased() && timer2Exceeded)) {
        //secondary action
        return goTo(secondaryAdr);
    }
    else {
        //primary action
        //postponeNextN(1);
        return goTo(primaryAdr);
    }

}

bool processResolveSecondaryCommand(const char* arg1, const char* argEnd)
{
    const char* arg2 = nextTok(arg1, argEnd);
    const char* arg3 = nextTok(arg2, argEnd);
    const char* arg4 = nextTok(arg3, argEnd);
    uint16_t num1 = parseInt32(arg1, argEnd);
    uint16_t num2 = parseInt32(arg2, argEnd);
    uint16_t num3 = parseInt32(arg3, argEnd);

    if(arg4 == argEnd) {
        return processResolveSecondary(num1, num1, num2, num3);
    } else {
        uint8_t num4 = parseInt32(arg4, argEnd);
        return processResolveSecondary(num1, num2, num3, num4);
    }
}

macro_action_t decodeKey(const char* arg1, const char* argEnd) {
    macro_action_t action;
    uint8_t len = tokLen(arg1, argEnd);
    if(len == 1) {
        action.key.type = KeystrokeType_Basic;
        action.key.scancode = characterToScancode(*arg1);
        action.key.modifierMask = characterToShift(*arg1) ? HID_KEYBOARD_MODIFIER_LEFTSHIFT : 0;
    } else {
        reportError("Failed to decode key ", arg1, argEnd);
        //TODO: implement decoding of all keys
    }
    return action;
}

bool processKeyCommand(macro_sub_action_t type, const char* arg1, const char* argEnd) {
    macro_action_t action = decodeKey(arg1, argEnd);
    action.key.action = type;
    return processKey(action);
}

bool processResolveNextKeyIdCommand() {
    char num[4];
    PostponeKeys = true;
    if(Postponer_PendingCount() == 0) {
        return true;
    }
    num[0] = Postponer_PendingId() / 100 % 10 + 48;
    num[1] = Postponer_PendingId() / 10 % 10 + 48;
    num[2] = Postponer_PendingId() % 10 + 48;
    num[3] = '\0';
    if(!dispatchText(num, 3)){
        Postponer_ConsumePending();
        return false;
    }
    return true;
}

bool processResolveNextKeyEqCommand(const char* arg1, const char* argEnd) {
    PostponeKeys = true;
    const char* arg2 = nextTok(arg1, argEnd);
    const char* arg3 = nextTok(arg2, argEnd);
    const char* arg4 = nextTok(arg3, argEnd);
    const char* arg5 = nextTok(arg4, argEnd);
    uint16_t idx = parseInt32(arg1, argEnd);
    uint16_t key = parseInt32(arg2, argEnd);
    uint16_t timeout = parseInt32(arg3, argEnd);
    uint16_t adr1 = parseInt32(arg4, argEnd);
    uint16_t adr2 = parseInt32(arg5, argEnd);

    if(Timer_GetElapsedTime(&s->currentMacroStartTime) >= timeout) {
        return goTo(adr2);
    }
    if(Postponer_PendingCount() < idx + 1) {
        return true;
    }

    if(Postponer_PendingId(idx) == key) {
        return goTo(adr1);
    } else {
        return goTo(adr2);
    }
}

bool processConsumePendingCommand(const char* arg1, const char* argEnd) {
    uint16_t cnt = parseInt32(arg1, argEnd);
    Postponer_ConsumePending(cnt);
    return false;
}

bool processPostponeNextNCommand(const char* arg1, const char* argEnd) {
    uint16_t cnt = parseInt32(arg1, argEnd);
    PostponeKeys = true;
    postponeNextN(cnt);
    return false;
}

bool processCommandAction(void)
{
    const char* cmd = s->currentMacroAction.text.text+1;
    const char* cmdEnd = s->currentMacroAction.text.text + s->currentMacroAction.text.textLen;
    while(*cmd) {
        const char* arg1 = nextTok(cmd, cmdEnd);
        switch(*cmd) {
        case 'a':
            if(tokenMatches(cmd, cmdEnd, "addReg")) {
                return processRegAddCommand(arg1, cmdEnd, false);
            }
            else {
                goto failed;
            }
            break;
        case 'b':
            if(tokenMatches(cmd, cmdEnd, "break")) {
                return processBreakCommand();
            }
            else {
                goto failed;
            }
            break;
        case 'c':
            if(tokenMatches(cmd, cmdEnd, "consumePending")) {
                return processConsumePendingCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'd':
            if(tokenMatches(cmd, cmdEnd, "delayUntilRelease")) {
                return processDelayUntilReleaseCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "delayUntilReleaseMax")) {
                return processDelayUntilReleaseMaxCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "delayUntil")) {
                return processDelayUntilCommand(cmd, cmdEnd);
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
        case 'h':
            if(tokenMatches(cmd, cmdEnd, "holdLayer")) {
                return processHoldLayerCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "holdLayerMax")) {
                return processHoldLayerMaxCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "holdKeymapLayer")) {
                return processHoldKeymapLayerCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "holdKeymapLayerMax")) {
                return processHoldKeymapLayerMaxCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "holdKey")) {
                return processKeyCommand(MacroSubAction_Hold, arg1, cmdEnd);
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
            else if(tokenMatches(cmd, cmdEnd, "ifRegEq")) {
                if(!processIfRegEqCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = nextTok(arg1, cmdEnd); //shift by 2
                arg1 = nextTok(cmd, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotRegEq")) {
                if(!processIfRegEqCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = nextTok(arg1, cmdEnd); //shift by 2
                arg1 = nextTok(cmd, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "ifPlaytime")) {
                if(!processIfPlaytimeCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;  //shift by 1
                arg1 = nextTok(cmd, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotPlaytime")) {
                if(!processIfPlaytimeCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = nextTok(cmd, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "ifAnyMod")) {
                if(!processIfModifierCommand(false, 0xFF)  && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotAnyMod")) {
                if(!processIfModifierCommand(true, 0xFF)  && !s->currentConditionPassed) {
                    return false;
                }
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
            else if(tokenMatches(cmd, cmdEnd, "ifGui")) {
                if(!processIfModifierCommand(false, GUIMASK)  && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(tokenMatches(cmd, cmdEnd, "ifNotGui")) {
                if(!processIfModifierCommand(true, GUIMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else {
                goto failed;
            }
            break;
        case 'n':
            if(tokenMatches(cmd, cmdEnd, "noOp")) {
                return processNoOpCommand();
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
            else if(tokenMatches(cmd, cmdEnd, "pressKey")) {
                return processKeyCommand(MacroSubAction_Press, arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "postponeKeys")) {
                processPostponeKeysCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "postponeNext")) {
                processPostponeNextNCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'r':
            if(tokenMatches(cmd, cmdEnd, "recordMacro")) {
                return processRecordMacroCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "recordMacroDelay")) {
                return processRecordMacroDelayCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "resolveSecondary")) {
                return processResolveSecondaryCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "resolveNextKeyId")) {
                return processResolveNextKeyIdCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "resolveNextKeyEq")) {
                return processResolveNextKeyEqCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "releaseKey")) {
                return processKeyCommand(MacroSubAction_Release, arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 's':
            if(tokenMatches(cmd, cmdEnd, "setStatus")) {
                return processSetStatusCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "setReg")) {
                return processSetRegCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "statsRuntime")) {
                return processStatsRuntimeCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "subReg")) {
                return processRegAddCommand(arg1, cmdEnd, true);
            }
            else if(tokenMatches(cmd, cmdEnd, "switchKeymap")) {
                return processSwitchKeymapCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "switchKeymapLayer")) {
                return processSwitchKeymapLayerCommand(arg1, cmdEnd);
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
            else if(tokenMatches(cmd, cmdEnd, "suppressMods")) {
                processSuppressModsCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "suppressKeys")) {
                processSuppressKeysCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "setStickyModsEnabled")) {
                return processSetStickyModsEnabledCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "setActivateOnRelease")) {
                return processSetActivateOnReleaseCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "setSplitCompositeKeystroke")) {
                return processSetSplitCompositeKeystrokeCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "setKeystrokeDelay")) {
                return processSetKeystrokeDelayCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "setDebounceDelay")) {
                return processSetDebounceDelayCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 't':
            if(tokenMatches(cmd, cmdEnd, "tapKey")) {
                return processKeyCommand(MacroSubAction_Tap, arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'w':
            if(tokenMatches(cmd, cmdEnd, "write")) {
                return processWriteCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        default:
        failed:
            reportError("unrecognized command", cmd, cmdEnd);
            return false;
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
    } else if (s->currentMacroAction.text.text[0] == '#') {
        return false;
    } else {
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
        if(!MacroState[i].macroPlaying) {
            s = &MacroState[i];
            return true;
        }
    }
    reportError("Too many macros running at one time", "", NULL);
    return false;
}

void initialize() {
    Macros_UpdateLayerStack();
    initialized = true;
}

void Macros_StartMacro(uint8_t index, key_state_t *keyState)
{
    if(!findFreeStateSlot() || AllMacros[index].macroActionsCount == 0)  {
       return;
    }
    if(!initialized) {
        initialize();
    }
    MacroPlaying = true;
    s->macroPlaying = true;
    s->macroInterrupted = false;
    s->currentMacroIndex = index;
    s->currentMacroActionIndex = 0;
    s->currentMacroKey = keyState;
    s->currentMacroStartTime = CurrentTime;
    s->currentConditionPassed = false;
    s->reportsUsed = false;
    s->postponeNext = 0;
    ValidatedUserConfigBuffer.offset = AllMacros[index].firstMacroActionOffset;
    ParseMacroAction(&ValidatedUserConfigBuffer, &s->currentMacroAction);
    s->bufferOffset = ValidatedUserConfigBuffer.offset;
    memset(&s->macroMouseReport, 0, sizeof s->macroMouseReport);
    memset(&s->macroBasicKeyboardReport, 0, sizeof s->macroBasicKeyboardReport);
    memset(&s->macroMediaKeyboardReport, 0, sizeof s->macroMediaKeyboardReport);
    memset(&s->macroSystemKeyboardReport, 0, sizeof s->macroSystemKeyboardReport);
}

void continueMacro(void)
{
    PostponeKeys = s->postponeNext > 0;
    if (processCurrentMacroAction() && !s->macroBroken) {
        return;
    }
    s->postponeNext = s->postponeNext > 0 ? s->postponeNext - 1 : 0;
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
        if(MacroState[i].macroPlaying) {
            someonePlaying = true;
            s = &MacroState[i];
            continueMacro();
        }
    }
    MacroPlaying &= someonePlaying;
}

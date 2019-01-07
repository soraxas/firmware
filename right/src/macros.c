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

bool processKeyAction(void)
{
    if(!Macros_ClaimReports()) {
        return true;
    }

    switch (s->currentMacroAction.key.action) {
        case MacroSubAction_Tap:
            if (s->keyActionPressPhase == 0) {
                s->keyActionPressPhase = 1;
                addModifiers(s->currentMacroAction.key.modifierMask);
                if (s->currentMacroAction.key.modifierMask != 0 && SplitCompositeKeystroke != 0) {
                    return true;
                }
            }
            if (s->keyActionPressPhase == 1) {
                s->keyActionPressPhase = 2;
                addScancode(s->currentMacroAction.key.scancode, s->currentMacroAction.key.type);
                return true;
            }
            if (s->keyActionPressPhase > 1) {
                s->keyActionPressPhase = 0;
                deleteModifiers(s->currentMacroAction.key.modifierMask);
                deleteScancode(s->currentMacroAction.key.scancode, s->currentMacroAction.key.type);
            }
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

bool processMouseButtonAction(void)
{
    if(!Macros_ClaimReports()) {
        return true;
    }
    switch (s->currentMacroAction.key.action) {
        case MacroSubAction_Tap:
            if (!s->mouseButtonPressStarted) {
                s->mouseButtonPressStarted = true;
                s->macroMouseReport.buttons |= s->currentMacroAction.mouseButton.mouseButtonsMask;
                return true;
            }
            s->mouseButtonPressStarted = false;
            s->macroMouseReport.buttons &= ~s->currentMacroAction.mouseButton.mouseButtonsMask;
            break;
        case MacroSubAction_Release:
            s->macroMouseReport.buttons &= ~s->currentMacroAction.mouseButton.mouseButtonsMask;
            break;
        case MacroSubAction_Press:
            s->macroMouseReport.buttons |= s->currentMacroAction.mouseButton.mouseButtonsMask;
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
    scancode = characterToScancode(character);
    for (uint8_t i = 0; i < s->dispatchReportIndex; i++) {
        if (s->macroBasicKeyboardReport.scancodes[i] == scancode) {
            s->dispatchReportIndex = max_keys;
            return true;
        }
    }
    s->macroBasicKeyboardReport.scancodes[s->dispatchReportIndex++] = scancode;
    s->macroBasicKeyboardReport.modifiers = characterToShift(character) ? HID_KEYBOARD_MODIFIER_LEFTSHIFT : 0;
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

bool processSwitchKeymapCommand(const char* arg1, const char* cmdEnd)
{
    int tmpKeymapIdx = CurrentKeymapIndex;
    if(tokenMatches(arg1, cmdEnd, "last")) {
        SwitchKeymapById(lastKeymapIdx);
    }
    else {
        SwitchKeymapByAbbreviation(tokLen(arg1, cmdEnd), arg1);
    }
    lastKeymapIdx = tmpKeymapIdx;
    return false;
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
    }
    ToggleLayer(layerIdxStack[layerIdxStackTop].layer);
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
        reportError("unrecognized layer id", arg1, cmdEnd);
        return false;
    }
}

void pushStack(uint8_t layer) {
    layerIdxStackTop = (layerIdxStackTop + 1) % LAYER_STACK_SIZE;
    layerIdxStack[layerIdxStackTop].layer = layer;
    layerIdxStack[layerIdxStackTop].removed = false;
    ToggleLayer(layerIdxStack[layerIdxStackTop].layer);
    layerIdxStackSize = layerIdxStackSize < LAYER_STACK_SIZE - 1 ? layerIdxStackSize+1 : layerIdxStackSize;
}

bool processSwitchLayerCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpLayerIdx = ToggledLayer;
    if(tokenMatches(arg1, cmdEnd, "previous")) {
        popLayerStack(true);
    }
    else {
        pushStack(parseLayerId(arg1, cmdEnd));
    }
    lastLayerIdx = tmpLayerIdx;
    return false;
}


bool processHoldLayer(uint8_t layer, uint16_t timeout)
{
    if(!s->holdActive) {
        s->holdActive = true;
        pushStack(layer);
        s->holdLayerIdx = layerIdxStackTop;
        return true;
    }
    else {
        if(s->currentMacroKey->previous && (Timer_GetElapsedTime(&s->currentMacroStartTime) < timeout || s->macroInterrupted)) {
            return true;
        }
        else {
            s->holdActive = false;
            if(layerIdxStack[s->holdLayerIdx].layer == layer) {
                layerIdxStack[s->holdLayerIdx].removed = true;
            }
            popLayerStack(false);
            return false;
        }
    }
}

bool processHoldLayerCommand(const char* arg1, const char* cmdEnd)
{
    return processHoldLayer(parseLayerId(arg1, cmdEnd), 0xFFFF);
}

bool processHoldLayerMaxCommand(const char* arg1, const char* cmdEnd)
{
    const char* arg2 = nextTok(arg1, cmdEnd);
    uint16_t timeout = parseInt32(arg2, cmdEnd);
    return processHoldLayer(parseLayerId(arg1, cmdEnd), timeout);
}

bool processDelayUntilReleaseMaxCommand(const char* arg1, const char* cmdEnd)
{
    uint32_t timeout = parseInt32(arg1, cmdEnd);
    //debouncing takes place later in the event loop, i.e., at this time, only s->currentMacroKey->previous is debounced
    if(s->currentMacroKey->previous && Timer_GetElapsedTime(&s->currentMacroStartTime) < timeout) {
        return true;
    }
    return false;
}

bool processDelayUntilReleaseCommand()
{
    if(s->currentMacroKey->previous) {
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
    if(s->currentMacroKey->previous) {
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
    uint8_t param = parseInt32(nextTok(arg1, argEnd), argEnd);
    regs[address] = param;
    return false;
}

bool processGoToCommand(const char* arg, const char *argEnd)
{
    uint8_t address = parseInt32(arg, argEnd);
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

bool processSetStickyModsEnabledCommand(const char* arg, const char *argEnd)
{
    uint8_t enabled = parseInt32(arg,  argEnd);
    StickyModifiersEnabled = enabled;
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
            else if(tokenMatches(cmd, cmdEnd, "recordMacroDelay")) {
                return processRecordMacroDelayCommand();
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
            else if(tokenMatches(cmd, cmdEnd, "suppressMods")) {
                processSuppressModsCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "suppressKeys")) {
                processSuppressKeysCommand();
            }
            else if(tokenMatches(cmd, cmdEnd, "setStickyModsEnabled")) {
                return processSetStickyModsEnabledCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "setSplitCompositeKeystroke")) {
                return processSetSplitCompositeKeystrokeCommand(arg1, cmdEnd);
            }
            else if(tokenMatches(cmd, cmdEnd, "setKeystrokeDelay")) {
                return processSetKeystrokeDelayCommand(arg1, cmdEnd);
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
    s->reportsUsed = false;
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
        if(MacroState[i].macroPlaying) {
            someonePlaying = true;
            s = &MacroState[i];
            continueMacro();
        }
    }
    MacroPlaying &= someonePlaying;
}

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
#include "macro_shortcut_parser.h"
#include "str_utils.h"


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
static uint8_t lastLayerKeymapIdx;
static uint8_t lastKeymapIdx;

static int32_t regs[MAX_REG_COUNT];

static bool initialized = false;

macro_state_t MacroState[MACRO_STATE_POOL_SIZE];
static macro_state_t *s = MacroState;

bool processCommand(const char* cmd, const char* cmdEnd);

bool Macros_ClaimReports() {
    s->reportsUsed = true;
    return true;
}

void Macros_SignalInterrupt()
{
    for(uint8_t i = 0; i < MACRO_STATE_POOL_SIZE; i++) {
        if(MacroState[i].macroPlaying) {
            MacroState[i].macroInterrupted = true;
        }
    }
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


void postponeNextN(uint8_t count) {
    s->postponeNext = count + 1;
    PostponeKeys = true;
}

void postponeCurrent() {
    s->postponeNext = 1;
    PostponeKeys = true;
}

/**
 * Both key press and release are subject to postponing, therefore we need to ensure
 * that macros which actively initiate postponing and wait until release ignore
 * postponed key releases. The s->postponeNext indicates that the running macro
 * initiates postponing in the current cycle.
 */
bool currentMacroKeyIsActive() {
    if(s->postponeNext > 0) {
        return ACTIVE(s->currentMacroKey) && !Postponer_IsKeyReleased(s->currentMacroKey);
    } else {
        return ACTIVE(s->currentMacroKey);
    }
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
                if(currentMacroKeyIsActive() && action == MacroSubAction_Hold) {
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
                if(currentMacroKeyIsActive() && action == MacroSubAction_Hold) {
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
    mods = MacroShortcutParser_CharacterToShift(character) ? HID_KEYBOARD_MODIFIER_LEFTSHIFT : 0;
    scancode = MacroShortcutParser_CharacterToScancode(character);
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

bool validReg(uint8_t idx) {
    if(idx >= MAX_REG_COUNT) {
        Macros_ReportErrorNum("Invalid register index:", idx);
        return false;
    }
    return true;
}

bool isNUM(const char *a, const char *aEnd) {
    switch(*a) {
    case '0'...'9':
    case '#':
    case '-':
    case '@':
        return true;
    default:
        return false;
    }
}

int32_t parseNUM(const char *a, const char *aEnd)
{
    if(*a == '#') {
        a++;
        if(TokenMatches(a, aEnd, "key")) {
            return Postponer_KeyId(s->currentMacroKey);
        }
        uint8_t adr = parseNUM(a, aEnd);
        if(validReg(adr)) {
            return regs[adr];
        } else {
            return 0;
        }
    }
    else if(*a == '@') {
        a++;
        return s->currentMacroActionIndex + parseNUM(a, aEnd);
    }
    else
    {
        return ParseInt32(a, aEnd);
    }
}


int32_t parseMacroId(const char *a, const char *aEnd) {
    const char* end = TokEnd(a, aEnd);
    static uint16_t lastMacroId = 0;
    if(TokenMatches(a, aEnd, "last")) {
        return lastMacroId;
    }
    else if(a == aEnd) {
        lastMacroId = Postponer_KeyId(s->currentMacroKey);
    } else if(end == a+1) {
        lastMacroId = (uint8_t)(*a);
    } else {
        lastMacroId = 128 + parseNUM(a, aEnd);
    }
    return lastMacroId;
}

void removeStackTop(bool toggledInsteadOfTop) {
    if(toggledInsteadOfTop) {
        for(int i = 0; i < layerIdxStackSize-1; i++) {
            uint8_t pos = (layerIdxStackTop + LAYER_STACK_SIZE - i) % LAYER_STACK_SIZE;
            if(!layerIdxStack[pos].held && !layerIdxStack[pos].removed) {
                layerIdxStack[pos].removed = true;
                return;
            }
        }
    } else {
        layerIdxStackTop = (layerIdxStackTop + LAYER_STACK_SIZE - 1) % LAYER_STACK_SIZE;
        layerIdxStackSize--;
    }
}

uint8_t findPreviousLayerRecordIdx() {
    for(int i = 1; i < layerIdxStackSize; i++) {
        uint8_t pos = (layerIdxStackTop + LAYER_STACK_SIZE - i) % LAYER_STACK_SIZE;
        if(!layerIdxStack[pos].removed) {
            return pos;
        }
    }
    return layerIdxStackTop;
}

void popLayerStack(bool forceRemoveTop, bool toggledInsteadOfTop) {
    if(layerIdxStackSize > 0 && forceRemoveTop) {
        removeStackTop(toggledInsteadOfTop);
    }
    while(layerIdxStackSize > 0 && layerIdxStack[layerIdxStackTop].removed) {
        removeStackTop(false);
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

void Macros_ResetLayerStack() {
    for(int i = 0; i < LAYER_STACK_SIZE; i++) {
        layerIdxStack[i].keymap = CurrentKeymapIndex;
    }
    layerIdxStackSize = 1;
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
    if(TokenMatches(arg1, cmdEnd, "last")) {
        return lastKeymapIdx;
    } else {
        uint8_t idx = FindKeymapByAbbreviation(TokLen(arg1, cmdEnd), arg1);
        if(idx == 0xFF) {
            reportError("Keymap not recognized: ", arg1, cmdEnd);
        }
        return idx;
    }
}

uint8_t parseLayerId(const char* arg1, const char* cmdEnd) {
    if(TokenMatches(arg1, cmdEnd, "fn")) {
        return LayerId_Fn;
    }
    else if(TokenMatches(arg1, cmdEnd, "mouse")) {
        return LayerId_Mouse;
    }
    else if(TokenMatches(arg1, cmdEnd, "mod")) {
        return LayerId_Mod;
    }
    else if(TokenMatches(arg1, cmdEnd, "base")) {
        return LayerId_Base;
    }
    else if(TokenMatches(arg1, cmdEnd, "last")) {
        return lastLayerIdx;
    }
    else if(TokenMatches(arg1, cmdEnd, "previous")) {
        return layerIdxStack[findPreviousLayerRecordIdx()].layer;
    }
    else {
        return false;
    }
}

uint8_t parseLayerKeymapId(const char* arg1, const char* cmdEnd) {
    if(TokenMatches(arg1, cmdEnd, "fn")) {
        return CurrentKeymapIndex;
    }
    else if(TokenMatches(arg1, cmdEnd, "mouse")) {
        return CurrentKeymapIndex;
    }
    else if(TokenMatches(arg1, cmdEnd, "mod")) {
        return CurrentKeymapIndex;
    }
    else if(TokenMatches(arg1, cmdEnd, "base")) {
        return CurrentKeymapIndex;
    }
    else if(TokenMatches(arg1, cmdEnd, "last")) {
        return lastLayerKeymapIdx;
    }
    else if(TokenMatches(arg1, cmdEnd, "previous")) {
        return layerIdxStack[findPreviousLayerRecordIdx()].keymap;
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
        Macros_ResetLayerStack();
    }
    lastKeymapIdx = tmpKeymapIdx;
    return false;
}

/**DEPRECATED**/
bool processSwitchKeymapLayerCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpLayerIdx = ToggledLayer;
    uint8_t tmpLayerKeymapIdx = CurrentKeymapIndex;
    pushStack(parseLayerId(NextTok(arg1, cmdEnd), cmdEnd), parseKeymapId(arg1, cmdEnd), false);
    lastLayerIdx = tmpLayerIdx;
    lastLayerKeymapIdx = tmpLayerKeymapIdx;
    return false;
}

/**DEPRECATED**/
bool processSwitchLayerCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpLayerIdx = ToggledLayer;
    uint8_t tmpLayerKeymapIdx = CurrentKeymapIndex;
    if(TokenMatches(arg1, cmdEnd, "previous")) {
        popLayerStack(true, false);
    }
    else {
        pushStack(parseLayerId(arg1, cmdEnd), parseLayerKeymapId(arg1, cmdEnd), false);
    }
    lastLayerIdx = tmpLayerIdx;
    lastLayerKeymapIdx = tmpLayerKeymapIdx;
    return false;
}


bool processToggleKeymapLayerCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpLayerIdx = ToggledLayer;
    uint8_t tmpLayerKeymapIdx = CurrentKeymapIndex;
    pushStack(parseLayerId(NextTok(arg1, cmdEnd), cmdEnd), parseKeymapId(arg1, cmdEnd), false);
    lastLayerIdx = tmpLayerIdx;
    lastLayerKeymapIdx = tmpLayerKeymapIdx;
    return false;
}

bool processToggleLayerCommand(const char* arg1, const char* cmdEnd)
{
    uint8_t tmpLayerIdx = ToggledLayer;
    uint8_t tmpLayerKeymapIdx = CurrentKeymapIndex;
    pushStack(parseLayerId(arg1, cmdEnd), parseLayerKeymapId(arg1, cmdEnd), false);
    lastLayerIdx = tmpLayerIdx;
    lastLayerKeymapIdx = tmpLayerKeymapIdx;
    return false;
}

bool processUnToggleLayerCommand()
{
    uint8_t tmpLayerIdx = ToggledLayer;
    uint8_t tmpLayerKeymapIdx = CurrentKeymapIndex;
    popLayerStack(true, true);
    lastLayerIdx = tmpLayerIdx;
    lastLayerKeymapIdx = tmpLayerKeymapIdx;
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
        if(currentMacroKeyIsActive() && (Timer_GetElapsedTime(&s->currentMacroStartTime) < timeout || s->macroInterrupted)) {
            return true;
        }
        else {
            s->holdActive = false;
            layerIdxStack[s->holdLayerIdx].removed = true;
            layerIdxStack[s->holdLayerIdx].held = false;
            popLayerStack(false, false);
            return false;
        }
    }
}

bool Macros_IsLayerHeld() {
    return layerIdxStack[layerIdxStackTop].held;
}

bool processHoldLayerCommand(const char* arg1, const char* cmdEnd)
{
    return processHoldLayer(parseLayerId(arg1, cmdEnd), parseLayerKeymapId(arg1, cmdEnd), 0xFFFF);
}

bool processHoldLayerMaxCommand(const char* arg1, const char* cmdEnd)
{
    const char* arg2 = NextTok(arg1, cmdEnd);
    return processHoldLayer(parseLayerId(arg1, cmdEnd), parseLayerKeymapId(arg1, cmdEnd), parseNUM(arg2, cmdEnd));
}

bool processHoldKeymapLayerCommand(const char* arg1, const char* cmdEnd)
{
    const char* arg2 = NextTok(arg1, cmdEnd);
    return processHoldLayer(parseLayerId(arg2, cmdEnd), parseKeymapId(arg1, cmdEnd), 0xFFFF);
}

bool processHoldKeymapLayerMaxCommand(const char* arg1, const char* cmdEnd)
{
    const char* arg2 = NextTok(arg1, cmdEnd);
    const char* arg3 = NextTok(arg2, cmdEnd);
    return processHoldLayer(parseLayerId(arg2, cmdEnd), parseKeymapId(arg1, cmdEnd), parseNUM(arg3, cmdEnd));
}

bool processDelayUntilReleaseMaxCommand(const char* arg1, const char* cmdEnd)
{
    uint32_t timeout = parseNUM(arg1, cmdEnd);
    if(currentMacroKeyIsActive() && Timer_GetElapsedTime(&s->currentMacroStartTime) < timeout) {
        return true;
    }
    return false;
}

bool processDelayUntilReleaseCommand()
{
    if(currentMacroKeyIsActive()) {
        return true;
    }
    return false;
}

bool processDelayUntilCommand(const char* arg1, const char* cmdEnd)
{
    uint32_t time = parseNUM(arg1,  cmdEnd);
    return processDelay(time);
}

bool processRecordMacroDelayCommand()
{
    if(currentMacroKeyIsActive()) {
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

bool processIfRecordingCommand(bool negate)
{
    return MacroRecorder_IsRecording() != negate;
}

bool processIfRecordingIdCommand(bool negate, const char* arg, const char *argEnd)
{
    uint16_t id = parseMacroId(arg, argEnd);
    bool res = MacroRecorder_RecordingId() == id;
    return res != negate;
}

bool processIfPendingCommand(bool negate, const char* arg, const char *argEnd)
{
    uint32_t cnt = parseNUM(arg, argEnd);

    return (Postponer_PendingCount() >= cnt) != negate;
}

bool processIfPlaytimeCommand(bool negate, const char* arg, const char *argEnd)
{
    uint32_t timeout = parseNUM(arg, argEnd);
    uint32_t delay = Timer_GetElapsedTime(&s->currentMacroStartTime);
    return (delay > timeout) != negate;
}

bool processIfInterruptedCommand(bool negate)
{
   return s->macroInterrupted != negate;
}

bool processIfReleasedCommand(bool negate)
{
   return (!currentMacroKeyIsActive()) != negate;
}

bool processIfRegEqCommand(bool negate, const char* arg1, const char *argEnd)
{
    uint8_t address = parseNUM(arg1, argEnd);
    uint8_t param = parseNUM(NextTok(arg1, argEnd), argEnd);
    if(validReg(address)) {
        bool res = regs[address] == param;
        return res != negate;
    } else {
        return false;
    }
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

bool processSetLedTxtCommand(const char* arg1, const char *argEnd)
{
    int16_t time = parseNUM(arg1, argEnd);
    const char* str = NextTok(arg1, argEnd);
    LedDisplay_SetText(TokLen(str, argEnd), str);
    if(!processDelay(time)) {
        LedDisplay_UpdateText();
        return false;
    } else {
        return true;
    }
}

bool processSetRegCommand(const char* arg1, const char *argEnd)
{
    uint8_t address = parseNUM(arg1, argEnd);
    int32_t param = parseNUM(NextTok(arg1, argEnd), argEnd);
    if(validReg(address)) {
        regs[address] = param;
    }
    return false;
}

bool processRegAddCommand(const char* arg1, const char *argEnd, bool invert)
{
    uint8_t address = parseNUM(arg1, argEnd);
    int32_t param = parseNUM(NextTok(arg1, argEnd), argEnd);
    if(validReg(address)) {
        if(invert) {
            regs[address] = regs[address] - param;
        } else {
            regs[address] = regs[address] + param;
        }
    }
    return false;
}

bool processRegMulCommand(const char* arg1, const char *argEnd)
{
    uint8_t address = parseNUM(arg1, argEnd);
    int32_t param = parseNUM(NextTok(arg1, argEnd), argEnd);
    if(validReg(address)) {
        regs[address] = regs[address]*param;
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
    uint8_t address = parseNUM(arg, argEnd);
    return goTo(address);
}

bool processStopRecordingCommand()
{
    MacroRecorder_StopRecording();
    return false;
}

bool processMouseCommand(bool enable, const char* arg1, const char *argEnd)
{
    const char* arg2 = NextTok(arg1, argEnd);
    uint8_t dirOffset = 0;

    serialized_mouse_action_t baseAction = SerializedMouseAction_LeftClick;

    if(TokenMatches(arg1, argEnd, "move")) {
        baseAction = SerializedMouseAction_MoveUp;
    }
    else if(TokenMatches(arg1, argEnd, "scroll")) {
        baseAction = SerializedMouseAction_ScrollUp;
    }
    else if(TokenMatches(arg1, argEnd, "accelerate")) {
        baseAction = SerializedMouseAction_Accelerate;
    }
    else if(TokenMatches(arg1, argEnd, "decelerate")) {
        baseAction = SerializedMouseAction_Decelerate;
    }
    else {
        reportError("unrecognized argument", arg1, argEnd);
    }

    if(baseAction == SerializedMouseAction_MoveUp || baseAction == SerializedMouseAction_ScrollUp) {
        if(TokenMatches(arg2, argEnd, "up")) {
            dirOffset = 0;
        }
        else if(TokenMatches(arg2, argEnd, "down")) {
            dirOffset = 1;
        }
        else if(TokenMatches(arg2, argEnd, "left")) {
            dirOffset = 2;
        }
        else if(TokenMatches(arg2, argEnd, "right")) {
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
    uint16_t id = parseMacroId(arg, argEnd);
    MacroRecorder_RecordRuntimeMacroSmart(id);
    return false;
}

bool processPlayMacroCommand(const char* arg, const char *argEnd)
{
    uint16_t id = parseMacroId(arg, argEnd);
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
    postponeCurrent();
    return false;
}

bool processSetStickyModsEnabledCommand(const char* arg, const char *argEnd)
{
    uint8_t enabled = parseNUM(arg,  argEnd);
    StickyModifiersEnabled = enabled;
    return false;
}

bool processSetActivateOnReleaseCommand(const char* arg, const char *argEnd)
{
    uint8_t enabled = parseNUM(arg,  argEnd);
    ActivateOnRelease = enabled;
    return false;
}

bool processSetSplitCompositeKeystrokeCommand(const char* arg, const char *argEnd)
{
    SplitCompositeKeystroke  = parseNUM(arg,  argEnd);
    return false;
}

bool processSetKeystrokeDelayCommand(const char* arg, const char *argEnd)
{
    KeystrokeDelay  = parseNUM(arg,  argEnd);
    KeystrokeDelay = KeystrokeDelay < 250 ? KeystrokeDelay : 250;
    return false;
}

bool processSetDebounceDelayCommand(const char* arg, const char *argEnd)
{
    uint16_t delay = parseNUM(arg,  argEnd);
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

bool processResolveSecondary(uint16_t timeout1, uint16_t timeout2, uint8_t primaryAdr, uint8_t secondaryAdr) {
    postponeCurrent();

    //phase 1 - wait until some other key is released, then write down its release time
    bool timer1Exceeded = Timer_GetElapsedTime(&s->currentMacroStartTime) >= timeout1;
    if(!timer1Exceeded && currentMacroKeyIsActive() && !Postponer_IsPendingReleased()) {
        s->resolveSecondaryPhase2StartTime = 0;
        return true;
    }
    if(s->resolveSecondaryPhase2StartTime == 0) {
        s->resolveSecondaryPhase2StartTime = CurrentTime;
    }
    //phase 2 - "safety margin" - wait another `timeout2` ms, and if the switcher is released during this time, still interpret it as a primary action
    bool timer2Exceeded = Timer_GetElapsedTime(&s->resolveSecondaryPhase2StartTime) >= timeout2;
    if(!timer1Exceeded && !timer2Exceeded && currentMacroKeyIsActive() && Postponer_IsPendingReleased() && Postponer_PendingCount() < 3) {
        return true;
    }
    //phase 3 - resolve the situation - if the switcher is released first or within the "safety margin", interpret it as primary action, otherwise secondary
    if(timer1Exceeded || (Postponer_IsPendingReleased() && timer2Exceeded)) {
        //secondary action
        return goTo(secondaryAdr);
    }
    else {
        //primary action
        postponeNextN(1);
        return goTo(primaryAdr);
    }

}

bool processResolveSecondaryCommand(const char* arg1, const char* argEnd)
{
    const char* arg2 = NextTok(arg1, argEnd);
    const char* arg3 = NextTok(arg2, argEnd);
    const char* arg4 = NextTok(arg3, argEnd);
    uint16_t num1 = parseNUM(arg1, argEnd);
    uint16_t num2 = parseNUM(arg2, argEnd);
    uint16_t num3 = parseNUM(arg3, argEnd);

    if(arg4 == argEnd) {
        return processResolveSecondary(num1, num1, num2, num3);
    } else {
        uint8_t num4 = parseNUM(arg4, argEnd);
        return processResolveSecondary(num1, num2, num3, num4);
    }
}

macro_action_t decodeKey(const char* arg1, const char* argEnd) {
    macro_action_t action = MacroShortcutParser_Parse(arg1, TokEnd(arg1, argEnd));
    /*
    macro_action_t action;
    uint8_t len = tokLen(arg1, argEnd);
    if(len == 1) {
        action.key.type = KeystrokeType_Basic;
        action.key.scancode = characterToScancode(*arg1);
        action.key.modifierMask = characterToShift(*arg1) ? HID_KEYBOARD_MODIFIER_LEFTSHIFT : 0;
    } else {
        reportError("Failed to decode key ", arg1, argEnd);
        //TODO: implement decoding of all keys
    }*/
    return action;
}

bool processKeyCommand(macro_sub_action_t type, const char* arg1, const char* argEnd) {
    macro_action_t action = decodeKey(arg1, argEnd);
    action.key.action = type;

    switch (action.type) {
        case MacroActionType_Key:
            return processKey(action);
        case MacroActionType_MouseButton:
            return processMouseButton(action);
        default:
            return false;
    }
}

bool processResolveNextKeyIdCommand() {
    char num[4];
    postponeCurrent();
    if(Postponer_PendingCount() == 0) {
        return true;
    }
    num[0] = Postponer_PendingId(0) / 100 % 10 + 48;
    num[1] = Postponer_PendingId(0) / 10 % 10 + 48;
    num[2] = Postponer_PendingId(0) % 10 + 48;
    num[3] = '\0';
    if(!dispatchText(num, 3)){
        Postponer_ConsumePending(1, true);
        return false;
    }
    return true;
}

bool processResolveNextKeyEqCommand(const char* arg1, const char* argEnd) {
    postponeCurrent();
    const char* arg2 = NextTok(arg1, argEnd);
    const char* arg3 = NextTok(arg2, argEnd);
    const char* arg4 = NextTok(arg3, argEnd);
    const char* arg5 = NextTok(arg4, argEnd);
    uint16_t idx = parseNUM(arg1, argEnd);
    uint16_t key = parseNUM(arg2, argEnd);
    uint16_t timeout = 0;
    bool untilRelease = false;
    if(TokenMatches(arg3, argEnd, "untilRelease")) {
       untilRelease = true;
    } else {
       timeout = parseNUM(arg3, argEnd);
    }
    uint16_t adr1 = parseNUM(arg4, argEnd);
    uint16_t adr2 = parseNUM(arg5, argEnd);


    if(idx > POSTPONER_MAX_FILL) {
        Macros_ReportErrorNum("Invalid argument 1, allowed at most: ", idx);
    }

    if(untilRelease ? !currentMacroKeyIsActive() : Timer_GetElapsedTime(&s->currentMacroStartTime) >= timeout) {
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

bool processIfShortcutCommand(const char* arg, const char* argEnd) {
    if(s->currentIfShortcutConditionPassed) {
        while(isNUM(arg, argEnd) && arg < argEnd) {
            arg = NextTok(arg, argEnd);
        }
        return processCommand(arg, argEnd);
    }

    postponeCurrent();
    uint8_t pendingCount = Postponer_PendingCount();
    uint8_t numArgs = 0;
    while(isNUM(arg, argEnd)) {
        numArgs++;
        uint8_t argKeyid = parseNUM(arg, argEnd);
        arg = NextTok(arg, argEnd);
        if(pendingCount < numArgs) {
            return currentMacroKeyIsActive();
        }
        else if (Postponer_PendingId(numArgs - 1) != argKeyid) {
            return false;
        }
    }
    Postponer_ConsumePending(numArgs, true);
    s->currentIfShortcutConditionPassed = true;
    s->currentConditionPassed = false; //otherwise following conditions would be skipped
    return processCommand(arg, argEnd);
}

bool processIfPendingIdCommand(bool negate, const char* arg1, const char* argEnd) {
    const char* arg2 = NextTok(arg1, argEnd);
    uint16_t idx = parseNUM(arg1, argEnd);
    uint16_t key = parseNUM(arg2, argEnd);

    return (Postponer_PendingId(idx) == key) != negate;
}

bool processConsumePendingCommand(const char* arg1, const char* argEnd) {
    uint16_t cnt = parseNUM(arg1, argEnd);
    Postponer_ConsumePending(cnt, true);
    return false;
}

bool processPostponeNextNCommand(const char* arg1, const char* argEnd) {
    uint16_t cnt = parseNUM(arg1, argEnd);
    PostponeKeys = true;
    postponeNextN(cnt);
    return false;
}


bool processRepeatForCommand(const char* arg1, const char* argEnd) {
    uint8_t idx = parseNUM(arg1, argEnd);
    uint8_t adr = parseNUM(NextTok(arg1, argEnd), argEnd);
    if(validReg(idx)) {
        if(regs[idx] > 0) {
            regs[idx]--;
            if(regs[idx] > 0) {
                return goTo(adr);
            }
        }
    }
    return false;
}

bool processCommand(const char* cmd, const char* cmdEnd)
{
    while(*cmd) {
        const char* arg1 = NextTok(cmd, cmdEnd);
        switch(*cmd) {
        case 'a':
            if(TokenMatches(cmd, cmdEnd, "addReg")) {
                return processRegAddCommand(arg1, cmdEnd, false);
            }
            else {
                goto failed;
            }
            break;
        case 'b':
            if(TokenMatches(cmd, cmdEnd, "break")) {
                return processBreakCommand();
            }
            else {
                goto failed;
            }
            break;
        case 'c':
            if(TokenMatches(cmd, cmdEnd, "consumePending")) {
                return processConsumePendingCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'd':
            if(TokenMatches(cmd, cmdEnd, "delayUntilRelease")) {
                return processDelayUntilReleaseCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "delayUntilReleaseMax")) {
                return processDelayUntilReleaseMaxCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "delayUntil")) {
                return processDelayUntilCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'f':
            if(TokenMatches(cmd, cmdEnd, "final")) {
                if(processCommand(NextTok(cmd, cmdEnd), cmdEnd)) {
                    return true;
                } else {
                    s->macroBroken = true;
                    return false;
                }
            }
            else {
                goto failed;
            }
            break;
        case 'g':
            if(TokenMatches(cmd, cmdEnd, "goTo")) {
                return processGoToCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'h':
            if(TokenMatches(cmd, cmdEnd, "holdLayer")) {
                return processHoldLayerCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "holdLayerMax")) {
                return processHoldLayerMaxCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "holdKeymapLayer")) {
                return processHoldKeymapLayerCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "holdKeymapLayerMax")) {
                return processHoldKeymapLayerMaxCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "holdKey")) {
                return processKeyCommand(MacroSubAction_Hold, arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'i':
            if(TokenMatches(cmd, cmdEnd, "ifDoubletap")) {
                if(!processIfDoubletapCommand(false) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotDoubletap")) {
                if(!processIfDoubletapCommand(true) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifInterrupted")) {
                if(!processIfInterruptedCommand(false) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotInterrupted")) {
                if(!processIfInterruptedCommand(true) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifReleased")) {
                if(!processIfReleasedCommand(false) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotReleased")) {
                if(!processIfReleasedCommand(true) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifRegEq")) {
                if(!processIfRegEqCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = NextTok(arg1, cmdEnd); //shift by 2
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotRegEq")) {
                if(!processIfRegEqCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = NextTok(arg1, cmdEnd); //shift by 2
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifPlaytime")) {
                if(!processIfPlaytimeCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;  //shift by 1
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotPlaytime")) {
                if(!processIfPlaytimeCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifAnyMod")) {
                if(!processIfModifierCommand(false, 0xFF)  && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotAnyMod")) {
                if(!processIfModifierCommand(true, 0xFF)  && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifShift")) {
                if(!processIfModifierCommand(false, SHIFTMASK)  && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotShift")) {
                if(!processIfModifierCommand(true, SHIFTMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifCtrl")) {
                if(!processIfModifierCommand(false, CTRLMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotCtrl")) {
                if(!processIfModifierCommand(true, CTRLMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifAlt")) {
                if(!processIfModifierCommand(false, ALTMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotAlt")) {
                if(!processIfModifierCommand(true, ALTMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifGui")) {
                if(!processIfModifierCommand(false, GUIMASK)  && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotGui")) {
                if(!processIfModifierCommand(true, GUIMASK) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifRecording")) {
                if(!processIfRecordingCommand(false) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotRecording")) {
                if(!processIfRecordingCommand(true) && !s->currentConditionPassed) {
                    return false;
                }
            }
            else if(TokenMatches(cmd, cmdEnd, "ifRecordingId")) {
                if(!processIfRecordingIdCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotRecordingId")) {
                if(!processIfRecordingIdCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotPending")) {
                if(!processIfPendingCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifPending")) {
                if(!processIfPendingCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                cmd = arg1;
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifPendingId")) {
                if(!processIfPendingIdCommand(false, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                //shift by two
                cmd = NextTok(arg1, cmdEnd);
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifNotPendingId")) {
                if(!processIfPendingIdCommand(true, arg1, cmdEnd) && !s->currentConditionPassed) {
                    return false;
                }
                //shift by two
                cmd = NextTok(arg1, cmdEnd);
                arg1 = NextTok(cmd, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "ifShortcut")) {
                return processIfShortcutCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'm':
            if(TokenMatches(cmd, cmdEnd, "mulReg")) {
                return processRegMulCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'n':
            if(TokenMatches(cmd, cmdEnd, "noOp")) {
                return processNoOpCommand();
            }
            else {
                goto failed;
            }
            break;
        case 'p':
            if(TokenMatches(cmd, cmdEnd, "printStatus")) {
                return processPrintStatusCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "playMacro")) {
                return processPlayMacroCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "pressKey")) {
                return processKeyCommand(MacroSubAction_Press, arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "postponeKeys")) {
                processPostponeKeysCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "postponeNext")) {
                return processPostponeNextNCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'r':
            if(TokenMatches(cmd, cmdEnd, "recordMacro")) {
                return processRecordMacroCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "recordMacroDelay")) {
                return processRecordMacroDelayCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "resolveSecondary")) {
                return processResolveSecondaryCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "resolveNextKeyId")) {
                return processResolveNextKeyIdCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "resolveNextKeyEq")) {
                return processResolveNextKeyEqCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "releaseKey")) {
                return processKeyCommand(MacroSubAction_Release, arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "repeatFor")) {
                return processRepeatForCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 's':
            if(TokenMatches(cmd, cmdEnd, "setStatus")) {
                return processSetStatusCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "setLedTxt")) {
                return processSetLedTxtCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "setReg")) {
                return processSetRegCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "statsRuntime")) {
                return processStatsRuntimeCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "subReg")) {
                return processRegAddCommand(arg1, cmdEnd, true);
            }
            else if(TokenMatches(cmd, cmdEnd, "switchKeymap")) {
                return processSwitchKeymapCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "switchKeymapLayer")) {
                return processSwitchKeymapLayerCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "switchLayer")) {
                return processSwitchLayerCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "startMouse")) {
                return processMouseCommand(true, arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "stopMouse")) {
                return processMouseCommand(false, arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "stopRecording")) {
                return processStopRecordingCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "suppressMods")) {
                processSuppressModsCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "suppressKeys")) {
                processSuppressKeysCommand();
            }
            else if(TokenMatches(cmd, cmdEnd, "setStickyModsEnabled")) {
                return processSetStickyModsEnabledCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "setActivateOnRelease")) {
                return processSetActivateOnReleaseCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "setSplitCompositeKeystroke")) {
                return processSetSplitCompositeKeystrokeCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "setKeystrokeDelay")) {
                return processSetKeystrokeDelayCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "setDebounceDelay")) {
                return processSetDebounceDelayCommand(arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 't':
            if(TokenMatches(cmd, cmdEnd, "toggleKeymapLayer")) {
                return processToggleKeymapLayerCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "toggleLayer")) {
                return processToggleLayerCommand(arg1, cmdEnd);
            }
            else if(TokenMatches(cmd, cmdEnd, "tapKey")) {
                return processKeyCommand(MacroSubAction_Tap, arg1, cmdEnd);
            }
            else {
                goto failed;
            }
            break;
        case 'u':
            if(TokenMatches(cmd, cmdEnd, "unToggleLayer")) {
                return processUnToggleLayerCommand();
            }
            else {
                goto failed;
            }
            break;
        case 'w':
            if(TokenMatches(cmd, cmdEnd, "write")) {
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


bool processCommandAction(void) {
    const char* cmd = s->currentMacroAction.text.text+1;
    const char* cmdEnd = s->currentMacroAction.text.text + s->currentMacroAction.text.textLen;
    return processCommand(cmd, cmdEnd);
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
    s->currentIfShortcutConditionPassed = false;
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
    if (++s->currentMacroActionIndex >= AllMacros[s->currentMacroIndex].macroActionsCount || s->macroBroken) {
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
    s->currentIfShortcutConditionPassed = false;
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

#include "macro_recorder.h"
#include "led_display.h"
#include "macros.h"
#include "timer.h"

bool RuntimeMacroPlaying = false;
bool RuntimeMacroRecording = false;

static uint8_t reportBuffer[REPORT_BUFFER_MAX_LENGTH];
static uint8_t reportBufferLength = 0;

static runtime_macro_header headers[MAX_RUNTIME_MACROS];
static uint8_t headersLen = 0;

static runtime_macro_header *recordingHeader;
static runtime_macro_header *playbackHeader;
static uint8_t playbackPosition;

static bool delayActive;
static uint32_t delayStart;

void initHeaderSlot(uint8_t id) {
    recordingHeader = &headers[headersLen];
    headersLen++;
    recordingHeader->id = id;
    recordingHeader->offset = reportBufferLength;
    recordingHeader->length = 0;
}

void discardHeaderSlot(uint8_t headerSlot) {
    uint16_t offsetLeft = headers[headerSlot].offset;
    uint16_t offsetRight = headers[headerSlot].offset + headers[headerSlot].length;
    uint16_t length = headers[headerSlot].length;
    uint8_t *leftPtr = &reportBuffer[offsetLeft];
    uint8_t *rightPtr = &reportBuffer[offsetRight];
    memcpy(leftPtr, rightPtr, reportBufferLength - offsetRight);
    reportBufferLength = reportBufferLength - length;
    memcpy(&headers[headerSlot], &headers[headerSlot+1], (headersLen-headerSlot) * sizeof (*recordingHeader));
    headersLen = headersLen-1;
    for(int i = headerSlot; i < headersLen; i++) {
        headers[i].offset -= length;
    }
}

void resolveRecordingHeader(uint8_t id) {
    for(int i = 0; i < headersLen; i++)
    {
        if(headers[i].id == id)
        {
            discardHeaderSlot(i);
            break;
        }
    }
    while(headersLen == MAX_RUNTIME_MACROS || reportBufferLength > REPORT_BUFFER_MAX_LENGTH - REPORT_BUFFER_MIN_GAP) {
        discardHeaderSlot(0);
        return;
    }
    initHeaderSlot(id);
}

bool resolvePlaybackHeader(uint8_t id) {
    for(int i = 0; i < headersLen; i++)
    {
        if(headers[i].id == id)
        {
            if(&headers[i] == recordingHeader && RuntimeMacroRecording) {
                return false;
            }
            playbackHeader = &headers[i];
            return true;
        }
    }
    //Macros_ReportErrorNum("Macro slot not found ", id);
    return false;
}

//id is an arbitrary slot identifier
void recordRuntimeMacroStart(uint8_t id) {
    resolveRecordingHeader(id);
    RuntimeMacroRecording = true;
    LedDisplay_SetIcon(LedDisplayIcon_Adaptive, true);
}

void writeByte(uint8_t b) {
    reportBuffer[reportBufferLength] = b;
    reportBufferLength++;
    recordingHeader->length++;
}


void writeUInt16(uint16_t b) {
    writeByte(((uint8_t*)&b)[0]);
    writeByte(((uint8_t*)&b)[1]);
}

void MacroRecorder_RecordBasicReport(usb_basic_keyboard_report_t *report) {
    if(!RuntimeMacroRecording) {
        return;
    }
    if(report->modifiers == 0 && report->scancodes[0] == 0) {
        writeByte(BasicKeyboardEmpty);
        return;
    }
    if(report->modifiers == 0 && report->scancodes[1] == 0) {
        writeByte(BasicKeyboardSimple);
        writeByte(report->scancodes[0]);
        return;
    }
    writeByte(BasicKeyboard);
    uint8_t size = 0;
    while( size < USB_BASIC_KEYBOARD_MAX_KEYS && report->scancodes[size] != 0) {
        size++;
    }
    writeByte(size);
    writeByte(report->modifiers);
    for(int i = 0; i < size; i++) {
        writeByte(report->scancodes[i]);
    }
}

void recordRuntimeMacroEnd() {
    RuntimeMacroRecording = false;
    LedDisplay_SetIcon(LedDisplayIcon_Adaptive, false);
}

uint8_t readByte() {
    return reportBuffer[playbackPosition++];
}

uint16_t readUInt16() {
    uint16_t b;
    ((uint8_t*)&b)[0] = reportBuffer[playbackPosition++];
    ((uint8_t*)&b)[1] = reportBuffer[playbackPosition++];
    return b;
}

void playReport(usb_basic_keyboard_report_t *report) {
    macro_report_type_t type = readByte();
    switch(type) {
    case BasicKeyboardEmpty:
        memset(report, 0, sizeof *report);
        break;
    case BasicKeyboardSimple:
        memset(report, 0, sizeof *report);
        report->scancodes[0] = readByte();
        break;
    case BasicKeyboard:
        memset(report, 0, sizeof *report);
        {
            uint8_t size = readByte();
            report->modifiers = readByte();
            for(int i = 0; i < size; i++) {
                report->scancodes[i] = readByte();
            }
        }
        break;
    case Delay:
        {
            uint16_t timeout = readUInt16();
            if(!delayActive) {
                delayActive = true;
                delayStart = CurrentTime;
                playbackPosition -= 3;
            } else {
                if(Timer_GetElapsedTime(&delayStart) < timeout) {
                    playbackPosition -= 3;
                }
                else {
                    delayActive = false;
                }
            }
        }
        break;
    default:
        Macros_ReportErrorNum("PlayReport decode failed at ", type);
    }
}

bool playRuntimeMacroBegin(uint8_t id) {
    if(!resolvePlaybackHeader(id)) {
        return false;
    }
    playbackPosition = playbackHeader->offset;
    RuntimeMacroPlaying = true;
    return true;
}

bool playRuntimeMacroContinue(usb_basic_keyboard_report_t* report) {
    if(!RuntimeMacroPlaying) {
        return false;
    }
    playReport(report);
    RuntimeMacroPlaying = playbackPosition < playbackHeader->offset + playbackHeader->length;
    return RuntimeMacroPlaying;
}

void MacroRecorder_RecordDelay(uint16_t delay) {
    if(!RuntimeMacroRecording) {
        return;
    }
    writeByte(Delay);
    writeUInt16(delay);
}

bool MacroRecorder_PlayRuntimeMacroSmart(uint8_t id, usb_basic_keyboard_report_t* report) {
    if(!Macros_ClaimReports()) {
        return true;
    }
    if(!RuntimeMacroPlaying) {
        if(!playRuntimeMacroBegin(id)) {
            return false;
        }
    }
    return playRuntimeMacroContinue(report);
}

void MacroRecorder_RecordRuntimeMacroSmart(uint8_t id) {
    if(RuntimeMacroPlaying) {
        return;
    }
    if(!RuntimeMacroRecording) {
        recordRuntimeMacroStart(id);
    }
    else {
        recordRuntimeMacroEnd();
    }
}

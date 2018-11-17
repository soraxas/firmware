#include "macro_recorder.h"
#include "led_display.h"
#include "macros.h"

bool RuntimeMacroPlaying = false;
bool RuntimeMacroRecording = false;

static uint8_t reportBuffer[REPORT_BUFFER_MAX_LENGTH];
static uint8_t reportBufferLength = 0;

static runtime_macro_header headers[MAX_RUNTIME_MACROS];
static uint8_t headersLen = 0;

static runtime_macro_header *recordingHeader;
static runtime_macro_header *playbackHeader;
static uint8_t playbackPosition;

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
            playbackHeader = &headers[i];
            return true;
        }
    }
    Macros_ReportErrorNum("Macro slot not found ", id);
    return false;
}

//id is an arbitrary slot identifier
void RecordRuntimeMacroStart(uint8_t id) {
    resolveRecordingHeader(id);
    RuntimeMacroRecording = true;
    LedDisplay_SetIcon(LedDisplayIcon_Adaptive, true);
}

void WriteByte(uint8_t b) {
    reportBuffer[reportBufferLength] = b;
    reportBufferLength++;
    recordingHeader->length++;
}

void RecordBasicReport(usb_basic_keyboard_report_t *report) {
    if(!RuntimeMacroRecording) {
        return;
    }
    if(report->modifiers == 0 && report->scancodes[0] == 0) {
        WriteByte(BasicKeyboardEmpty);
        return;
    }
    if(report->modifiers == 0 && report->scancodes[1] == 0) {
        WriteByte(BasicKeyboardSimple);
        WriteByte(report->scancodes[0]);
        return;
    }
    WriteByte(BasicKeyboard);
    uint8_t size = 0;
    while( size < USB_BASIC_KEYBOARD_MAX_KEYS && report->scancodes[size] != 0) {
        size++;
    }
    WriteByte(size);
    WriteByte(report->modifiers);
    for(int i = 0; i < size; i++) {
        WriteByte(report->scancodes[i]);
    }
}

void RecordRuntimeMacroEnd() {
    RuntimeMacroRecording = false;
    LedDisplay_SetIcon(LedDisplayIcon_Adaptive, false);
}

uint8_t ReadByte() {
    return reportBuffer[playbackPosition++];
}

void PlayReport(usb_basic_keyboard_report_t *report) {
    memset(report, 0, sizeof *report);
    macro_report_type_t type = ReadByte();
    switch(type) {
    case BasicKeyboardEmpty:
        break;
    case BasicKeyboardSimple:
        report->scancodes[0] = ReadByte();
        break;
    case BasicKeyboard:
        {
            uint8_t size = ReadByte();
            report->modifiers = ReadByte();
            for(int i = 0; i < size; i++) {
                report->scancodes[i] = ReadByte();
            }
            break;
        }
    default:
        Macros_ReportErrorNum("PlayReport decode failed at ", type);
    }
}

bool PlayRuntimeMacroBegin(uint8_t id) {
    if(!resolvePlaybackHeader(id)) {
        return false;
    }
    playbackPosition = playbackHeader->offset;
    RuntimeMacroPlaying = true;
    return true;
}

bool PlayRuntimeMacroContinue(usb_basic_keyboard_report_t* report) {
    if(!RuntimeMacroPlaying) {
        return false;
    }
    PlayReport(report);
    RuntimeMacroPlaying = playbackPosition < playbackHeader->offset + playbackHeader->length;
    return RuntimeMacroPlaying;
}


bool PlayRuntimeMacroSmart(uint8_t id, usb_basic_keyboard_report_t* report) {
    if(!Macros_ClaimReports()) {
        return true;
    }
    if(!RuntimeMacroPlaying) {
        if(!PlayRuntimeMacroBegin(id)) {
            return false;
        }
    }
    return PlayRuntimeMacroContinue(report);
}

void RecordRuntimeMacroSmart(uint8_t id) {
    if(RuntimeMacroPlaying) {
        return;
    }
    if(!RuntimeMacroRecording) {
        RecordRuntimeMacroStart(id);
    }
    else {
        RecordRuntimeMacroEnd();
    }
}

#include "macro_recorder.h"
#include "led_display.h"
#include "macros.h"

bool RuntimeMacroPlaying = false;
bool RuntimeMacroRecording = false;

#define REPORT_BUFFER_MAX_LENGTH 1000
static uint8_t reportBuffer[REPORT_BUFFER_MAX_LENGTH];
static uint8_t reportBufferLength = 0;

static runtime_macro_header header;
static uint8_t playbackPosition;

void RecordRuntimeMacroStart() {
    reportBufferLength = 0;
    header.offset = reportBufferLength;
    header.length = 0;
    RuntimeMacroRecording = true;
    memset(&header, 0, sizeof header);
    LedDisplay_SetText(3, "REC");
}

void WriteByte(uint8_t b) {
    reportBuffer[reportBufferLength] = b;
    reportBufferLength++;
    header.length++;
}

void RecordReport(usb_basic_keyboard_report_t *report, uint8_t size) {
    if(!RuntimeMacroRecording) {
        return;
    }
    if(report->modifiers == 0 && report->scancodes[0] == 0) {
        WriteByte(EmptyReport);
        return;
    }
    WriteByte(BasicKeyboard);
    WriteByte(size);
    WriteByte(report->modifiers);
    for(int i = 0; i < size; i++) {
        WriteByte(report->scancodes[i]);
    }
}

void RecordRuntimeMacroEnd() {
    RuntimeMacroRecording = false;
    LedDisplay_UpdateText();
}

uint8_t ReadByte() {
    return reportBuffer[playbackPosition++];
}

void PlayReport(usb_basic_keyboard_report_t *report) {
    memset(report, 0, sizeof *report);
    macro_report_type_t type = ReadByte();
    if(type != EmptyReport) {
        uint8_t size = ReadByte();
        report->modifiers = ReadByte();
        for(int i = 0; i < size; i++) {
            report->scancodes[i] = ReadByte();
        }
    }
}

bool PlayRuntimeMacroBegin() {
    playbackPosition = header.offset;
    RuntimeMacroPlaying = true;
    return true;
}

bool PlayRuntimeMacroContinue() {
    if(!RuntimeMacroPlaying || playbackPosition >= header.offset + header.length) {
        return false;
    }
    PlayReport(&MacroBasicKeyboardReport);
    RuntimeMacroPlaying = playbackPosition < header.offset + header.length;
    return RuntimeMacroPlaying;
}


bool PlayRuntimeMacroSmart() {
    if(!RuntimeMacroPlaying) {
        PlayRuntimeMacroBegin();
    }
    return PlayRuntimeMacroContinue();
}

void RecordRuntimeMacroSmart() {
    if(!RuntimeMacroRecording) {
        RecordRuntimeMacroStart();
    }
    else {
        RecordRuntimeMacroEnd();
    }
}

#include "led_display.h"
#include "slave_drivers/is31fl3731_driver.h"
#include "layer.h"
#include "keymap.h"

uint8_t IconsAndLayerTextsBrightness = 0xff;
uint8_t AlphanumericSegmentsBrightness = 0xff;
bool ledIconStates[LedDisplayIcon_Last];
char LedDisplay_DebugString[] = "   ";

static const uint16_t capitalLetterToSegmentMap[] = {
    0b0000000011110111,
    0b0001001010001111,
    0b0000000000111001,
    0b0001001000001111,
    0b0000000011111001,
    0b0000000011110001,
    0b0000000010111101,
    0b0000000011110110,
    0b0001001000001001,
    0b0000000000001110,
    0b0010010001110000,
    0b0000000000111000,
    0b0000010100110110,
    0b0010000100110110,
    0b0000000000111111,
    0b0000000011110011,
    0b0010000000111111,
    0b0010000011110011,
    0b0000000011101101,
    0b0001001000000001,
    0b0000000000111110,
    0b0000110000110000,
    0b0010100000110110,
    0b0010110100000000,
    0b0001010100000000,
    0b0000110000001001,
};

static const uint16_t digitToSegmentMap[] = {
    0b0000110000111111,
    0b0000010000000110,
    0b0000100010001011,
    0b0000000011001111,
    0b0000000011100110,
    0b0010000001101001,
    0b0000000011111101,
    0b0001010000000001,
    0b0000000011111111,
    0b0000000011101111,
};

static const uint16_t whiteToNumsToSegmentMap[] = {
    0b0000000000000110,
    0b0000001000100000,
    0b0001001011001110,
    0b0001001011101101,
    0b0000110000100100,
    0b0010001101011101,
    0b0000010000000000,
    0b0010010000000000,
    0b0000100100000000,
    0b0011111111000000,
    0b0001001011000000,
    0b0000100000000000,
    0b0000000011000000,
    0b0000000000000000,
    0b0000110000000000,
};

static const uint16_t numsToCharsToSegmentMap[] = {
    0b0001001000000000,
    0b0000101000000000,
    0b0010010000000000,
    0b0000000011001000,
    0b0000100100000000,
    0b0001000010000011,
    0b0000001010111011,
};

static const uint16_t lowerToUpperToSegmentMap[] = {
    0b0000000000111001,
    0b0010000100000000,
    0b0000000000001111,
    0b0000110000000011,
    0b0000000000001000,
    0b0000000100000000,
};

static const uint16_t aboveUpperToSegmentMap[] = {
    0b0000100101001001,
    0b0001001000000000,
    0b0010010010001001,
    0b0000010100100000,
};

static uint16_t characterToSegmentMap(char character)
{
    switch (character) {
        case '!' ... '/':
            return whiteToNumsToSegmentMap[character - '!'];
        case '0' ... '9':
            return digitToSegmentMap[character - '0'];
        case ':' ... '@':
            return numsToCharsToSegmentMap[character - ':'];
        case 'a' ... 'z':
            return capitalLetterToSegmentMap[character - 'a'];
        case '[' ... '`':
            return lowerToUpperToSegmentMap[character - '['];
        case 'A' ... 'Z':
            return capitalLetterToSegmentMap[character - 'A'];
        case '{' ... '~':
            return aboveUpperToSegmentMap[character - '{'];
    }
    return 0;
}

void LedDisplay_SetText(uint8_t length, const char* text)
{
    uint64_t allSegmentSets = 0;

    switch (length) {
        case 3:
            allSegmentSets = (uint64_t)characterToSegmentMap(text[2]) << 28;
        case 2:
            allSegmentSets |= characterToSegmentMap(text[1]) << 14;
        case 1:
            allSegmentSets |= characterToSegmentMap(text[0]);
    }

    LedDriverValues[LedDriverId_Left][11] = allSegmentSets & 0b00000001 ? AlphanumericSegmentsBrightness : 0;
    LedDriverValues[LedDriverId_Left][12] = allSegmentSets & 0b00000010 ? AlphanumericSegmentsBrightness : 0;
    allSegmentSets >>= 2;

    for (uint8_t i = 24; i <= 136; i += 16) {
        for (uint8_t j = 0; j < 5; j++) {
            LedDriverValues[LedDriverId_Left][i + j] = allSegmentSets & 1 << j ? AlphanumericSegmentsBrightness : 0;
        }
        allSegmentSets >>= 5;
    }
}

void LedDisplay_SetLayer(layer_id_t layerId)
{
    for (uint8_t i = 13; i <= 45; i += 16) {
        LedDriverValues[LedDriverId_Left][i] = 0;
    }

    if (layerId >= LayerId_Mod && layerId <= LayerId_Mouse) {
        LedDriverValues[LedDriverId_Left][16 * layerId - 3] = IconsAndLayerTextsBrightness;
    }
}

bool LedDisplay_GetIcon(led_display_icon_t icon)
{
    return LedDriverValues[LedDriverId_Left][8 + icon];
}

void LedDisplay_SetIcon(led_display_icon_t icon, bool isEnabled)
{
    ledIconStates[icon] = isEnabled;
    LedDriverValues[LedDriverId_Left][icon + 8] = isEnabled ? IconsAndLayerTextsBrightness : 0;
}

void LedDisplay_UpdateIcons(void)
{
    for (led_display_icon_t i=0; i<=LedDisplayIcon_Last; i++) {
        LedDisplay_SetIcon(i, ledIconStates[i]);
    }
}

void LedDisplay_UpdateText(void)
{
#if LED_DISPLAY_DEBUG_MODE == 0
    keymap_reference_t *currentKeymap = AllKeymaps + CurrentKeymapIndex;
    LedDisplay_SetText(currentKeymap->abbreviationLen, currentKeymap->abbreviation);
#else
    LedDisplay_SetText(strlen(LedDisplay_DebugString), LedDisplay_DebugString);
#endif
}

void LedDisplay_UpdateAll(void)
{
    LedDisplay_UpdateIcons();
    LedDisplay_UpdateText();
}

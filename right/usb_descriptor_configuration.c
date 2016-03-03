#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"
#include "include/usb/usb_device_class.h"
#include "include/usb/usb_device_hid.h"
#include "usb_descriptor_device.h"
#include "usb_composite_device.h"
#include "usb_descriptor_keyboard_report.h"
#include "usb_descriptor_mouse_report.h"
#include "usb_descriptor_generic_hid_report.h"
#include "usb_descriptor_configuration.h"

uint8_t UsbConfigurationDescriptor[USB_CONFIGURATION_DESCRIPTOR_TOTAL_LENGTH] = {

// Configuration descriptor

    USB_DESCRIPTOR_LENGTH_CONFIGURE,
    USB_DESCRIPTOR_TYPE_CONFIGURE,
    USB_SHORT_GET_LOW(USB_CONFIGURATION_DESCRIPTOR_TOTAL_LENGTH),
    USB_SHORT_GET_HIGH(USB_CONFIGURATION_DESCRIPTOR_TOTAL_LENGTH),
    USB_COMPOSITE_INTERFACE_COUNT,
    USB_COMPOSITE_CONFIGURATION_INDEX,
    USB_STRING_DESCRIPTOR_NONE,
    (USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_D7_MASK) |
        (USB_DEVICE_CONFIG_SELF_POWER << USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_SELF_POWERED_SHIFT) |
        (USB_DEVICE_CONFIG_REMOTE_WAKEUP << USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_REMOTE_WAKEUP_SHIFT),
    USB_DEVICE_MAX_POWER,

// Mouse interface descriptor

    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_MOUSE_INTERFACE_INDEX,
    USB_INTERFACE_ALTERNATE_SETTING_NONE,
    USB_MOUSE_ENDPOINT_COUNT,
    USB_MOUSE_CLASS,
    USB_MOUSE_SUBCLASS,
    USB_MOUSE_PROTOCOL,
    USB_STRING_DESCRIPTOR_NONE,

// Mouse HID descriptor

    USB_HID_DESCRIPTOR_LENGTH,
    USB_DESCRIPTOR_TYPE_HID,
    USB_SHORT_GET_LOW(USB_HID_VERSION),
    USB_SHORT_GET_HIGH(USB_HID_VERSION),
    USB_HID_COUNTRY_CODE_NOT_SUPPORTED,
    USB_REPORT_DESCRIPTOR_COUNT_PER_HID_DEVICE,
    USB_DESCRIPTOR_TYPE_HID_REPORT,
    USB_SHORT_GET_LOW(USB_MOUSE_REPORT_DESCRIPTOR_LENGTH),
    USB_SHORT_GET_HIGH(USB_MOUSE_REPORT_DESCRIPTOR_LENGTH),

// Mouse endpoint descriptor

    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_MOUSE_ENDPOINT_ID | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(USB_MOUSE_INTERRUPT_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(USB_MOUSE_INTERRUPT_IN_PACKET_SIZE),
    USB_MOUSE_INTERRUPT_IN_INTERVAL,

// Keyboard interface descriptor

    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_KEYBOARD_INTERFACE_INDEX,
    USB_INTERFACE_ALTERNATE_SETTING_NONE,
    USB_KEYBOARD_ENDPOINT_COUNT,
    USB_KEYBOARD_CLASS,
    USB_KEYBOARD_SUBCLASS,
    USB_KEYBOARD_PROTOCOL,
    USB_STRING_DESCRIPTOR_NONE,

// Keyboard HID descriptor

    USB_HID_DESCRIPTOR_LENGTH,
    USB_DESCRIPTOR_TYPE_HID,
    USB_SHORT_GET_LOW(USB_HID_VERSION),
    USB_SHORT_GET_HIGH(USB_HID_VERSION),
    USB_HID_COUNTRY_CODE_NOT_SUPPORTED,
    USB_REPORT_DESCRIPTOR_COUNT_PER_HID_DEVICE,
    USB_DESCRIPTOR_TYPE_HID_REPORT,
    USB_SHORT_GET_LOW(USB_KEYBOARD_REPORT_DESCRIPTOR_LENGTH),
    USB_SHORT_GET_HIGH(USB_KEYBOARD_REPORT_DESCRIPTOR_LENGTH),

// Keyboard endpoint descriptor

    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_KEYBOARD_ENDPOINT_ID | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(USB_KEYBOARD_INTERRUPT_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(USB_KEYBOARD_INTERRUPT_IN_PACKET_SIZE),
    USB_KEYBOARD_INTERRUPT_IN_INTERVAL,

// Generic HID interface descriptor

    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_GENERIC_HID_INTERFACE_INDEX,
    USB_INTERFACE_ALTERNATE_SETTING_NONE,
    USB_GENERIC_HID_ENDPOINT_COUNT,
    USB_GENERIC_HID_CLASS,
    USB_GENERIC_HID_SUBCLASS,
    USB_GENERIC_HID_PROTOCOL,
    USB_STRING_DESCRIPTOR_NONE,

// Generic HID descriptor

    USB_HID_DESCRIPTOR_LENGTH,
    USB_DESCRIPTOR_TYPE_HID,
    USB_SHORT_GET_LOW(USB_HID_VERSION),
    USB_SHORT_GET_HIGH(USB_HID_VERSION),
    USB_HID_COUNTRY_CODE_NOT_SUPPORTED,
    USB_REPORT_DESCRIPTOR_COUNT_PER_HID_DEVICE,
    USB_DESCRIPTOR_TYPE_HID_REPORT,
    USB_SHORT_GET_LOW(USB_GENERIC_HID_REPORT_DESCRIPTOR_LENGTH),
    USB_SHORT_GET_HIGH(USB_GENERIC_HID_REPORT_DESCRIPTOR_LENGTH),

// Generic HID IN endpoint descriptor

    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_GENERIC_HID_ENDPOINT_IN_ID | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(USB_GENERIC_HID_INTERRUPT_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(USB_GENERIC_HID_INTERRUPT_IN_PACKET_SIZE),
    USB_GENERIC_HID_INTERRUPT_IN_INTERVAL,

// Generic HID OUT endpoint descriptor

    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_GENERIC_HID_ENDPOINT_OUT_ID | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(USB_GENERIC_HID_INTERRUPT_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(USB_GENERIC_HID_INTERRUPT_OUT_PACKET_SIZE),
    USB_GENERIC_HID_INTERRUPT_IN_INTERVAL,
};

usb_status_t USB_DeviceGetConfigurationDescriptor(
    usb_device_handle handle, usb_device_get_configuration_descriptor_struct_t *configurationDescriptor)
{
    if (USB_COMPOSITE_CONFIGURATION_INDEX > configurationDescriptor->configuration) {
        configurationDescriptor->buffer = UsbConfigurationDescriptor;
        configurationDescriptor->length = USB_CONFIGURATION_DESCRIPTOR_TOTAL_LENGTH;
        return kStatus_USB_Success;
    }
    return kStatus_USB_InvalidRequest;
}

usb_status_t USB_DeviceGetHidDescriptor(
    usb_device_handle handle, usb_device_get_hid_descriptor_struct_t *hidDescriptor)
{
    return kStatus_USB_InvalidRequest;
}

usb_status_t USB_DeviceGetHidReportDescriptor(
    usb_device_handle handle, usb_device_get_hid_report_descriptor_struct_t *hidReportDescriptor)
{
    if (USB_MOUSE_INTERFACE_INDEX == hidReportDescriptor->interfaceNumber) {
        hidReportDescriptor->buffer = UsbMouseReportDescriptor;
        hidReportDescriptor->length = USB_MOUSE_REPORT_DESCRIPTOR_LENGTH;
    } else if (USB_KEYBOARD_INTERFACE_INDEX == hidReportDescriptor->interfaceNumber) {
        hidReportDescriptor->buffer = UsbKeyboardReportDescriptor;
        hidReportDescriptor->length = USB_KEYBOARD_REPORT_DESCRIPTOR_LENGTH;
    } else if (USB_GENERIC_HID_INTERFACE_INDEX == hidReportDescriptor->interfaceNumber) {
        hidReportDescriptor->buffer = UsbGenericHidReportDescriptor;
        hidReportDescriptor->length = USB_GENERIC_HID_REPORT_DESCRIPTOR_LENGTH;
    } else {
        return kStatus_USB_InvalidRequest;
    }
    return kStatus_USB_Success;
}

usb_status_t USB_DeviceGetHidPhysicalDescriptor(
    usb_device_handle handle, usb_device_get_hid_physical_descriptor_struct_t *hidPhysicalDescriptor)
{
    return kStatus_USB_InvalidRequest;
}

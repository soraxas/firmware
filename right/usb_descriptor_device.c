#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"
#include "include/usb/usb_device_class.h"
#include "include/usb/usb_device_hid.h"
#include "usb_descriptor_device.h"
#include "usb_composite_device.h"
#include "usb_descriptor_strings.h"

uint8_t UsbDeviceDescriptor[USB_DESCRIPTOR_LENGTH_DEVICE] = {
    USB_DESCRIPTOR_LENGTH_DEVICE,
    USB_DESCRIPTOR_TYPE_DEVICE,
    USB_SHORT_GET_LOW(USB_DEVICE_SPECIFICATION_VERSION),
    USB_SHORT_GET_HIGH(USB_DEVICE_SPECIFICATION_VERSION),
    USB_DEVICE_CLASS,
    USB_DEVICE_SUBCLASS,
    USB_DEVICE_PROTOCOL,
    USB_CONTROL_MAX_PACKET_SIZE,
    USB_SHORT_GET_LOW(USB_DEVICE_VENDOR_ID),
    USB_SHORT_GET_HIGH(USB_DEVICE_VENDOR_ID),
    USB_SHORT_GET_LOW(USB_DEVICE_PRODUCT_ID),
    USB_SHORT_GET_HIGH(USB_DEVICE_PRODUCT_ID),
    USB_SHORT_GET_LOW(USB_DEVICE_RELEASE_NUMBER),
    USB_SHORT_GET_HIGH(USB_DEVICE_RELEASE_NUMBER),
    USB_STRING_DESCRIPTOR_ID_MANUFACTURER,
    USB_STRING_DESCRIPTOR_ID_PRODUCT,
    USB_STRING_DESCRIPTOR_ID_SUPPORTED_LANGUAGES,
    USB_DEVICE_CONFIGURATION_COUNT,
};

usb_status_t USB_DeviceGetDeviceDescriptor(
    usb_device_handle handle, usb_device_get_device_descriptor_struct_t *deviceDescriptor)
{
    deviceDescriptor->buffer = UsbDeviceDescriptor;
    deviceDescriptor->length = USB_DESCRIPTOR_LENGTH_DEVICE;
    return kStatus_USB_Success;
}

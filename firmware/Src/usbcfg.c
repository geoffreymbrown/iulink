/*
     Modified by Geoffrey Brown 2018

    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "hal.h"
#include "usbcfg.h"

// USB Device Descriptor.

static const uint8_t vcom_device_descriptor_data[18] = {
  USB_DESC_DEVICE       (0x0200,        /* bcdUSB (2.0).                    */
                         0xEF,          /* bDeviceClass (MISC).             */
                         0x02,          /* bDeviceSubClass.                 */
                         0x01,          /* bDeviceProtocol.                 */
                         0x40,          /* bMaxPacketSize.                  */
                         USBD_VID,      /* idVendor (ST).                   */
                         USBD_PID,      /* idProduct.                       */
                         0x0200,        /* bcdDevice.                       */
                         1,             /* iManufacturer.                   */
                         2,             /* iProduct.                        */
                         3,             /* iSerialNumber.                   */
                         1)             /* bNumConfigurations.              */
};

// Device Descriptor wrapper.

static const USBDescriptor vcom_device_descriptor = {
  sizeof vcom_device_descriptor_data,
  vcom_device_descriptor_data
};

// Configuration Descriptor

static const uint8_t vcom_configuration_descriptor_data[] = {
  /* Configuration Descriptor.*/
  USB_DESC_CONFIGURATION(32,            /* wTotalLength.                    */
                         0x03,          /* bNumInterfaces.                  */
                         0x01,          /* bConfigurationValue.             */
                         0,             /* iConfiguration.                  */
                         0xC0,          /* bmAttributes (self powered).     */
                         50),           /* bMaxPower (100mA).               */

  // bulk usb
  USB_DESC_INTERFACE    (0x00,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x02,          /* bNumEndpoints.                   */
                         0xFF,          /* bInterfaceClass (Vendor Specific).*/
                         0x00,
                         0x00,
                         4),            /* iInterface.                      */
  /* Endpoint 2  Descriptor.*/
  USB_DESC_ENDPOINT     (BULK_OUT_EP,   /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00),         /* bInterval.                       */
  /* Endpoint 1 Descriptor.*/
  USB_DESC_ENDPOINT     (BULK_IN_EP|0x80,    /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00),         /* bInterval.                       */
};


// Configuration Descriptor wrapper.

static const USBDescriptor vcom_configuration_descriptor = {
  sizeof vcom_configuration_descriptor_data,
  vcom_configuration_descriptor_data
};


// U.S. English language identifier.

static const uint8_t vcom_string0[] = {
  USB_DESC_BYTE(4),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  USB_DESC_WORD(0x0409)                 /* wLANGID (U.S. English).          */
};


// Vendor string.

static const uint8_t vcom_string1[] = {
  USB_DESC_BYTE(10),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'I', 0, 'U', 0, 'C', 0, 'S',0
};

// Device Description string.

static const uint8_t vcom_string2[] = {
  USB_DESC_BYTE(14),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'I', 0, 'U', 0, 'L', 0,'i', 0, 'n', 0, 'k',0
};


// Serial Number string.

static uint8_t vcom_string3[50] __attribute__ ((aligned (2))) = {
  USB_DESC_BYTE(50),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  '0' + CH_KERNEL_MAJOR, 0,
  '0' + CH_KERNEL_MINOR, 0,
  '0' + CH_KERNEL_PATCH, 
  0
};


// Strings wrappers array.

static const USBDescriptor vcom_strings[] = {
  {sizeof vcom_string0, vcom_string0},
  {sizeof vcom_string1, vcom_string1},
  {sizeof vcom_string2, vcom_string2},
  {sizeof vcom_string3, vcom_string3}
};

static inline uint16_t toASCII(uint8_t v) {
  v = v & 0xff;
  if (v > 10)
    return v + 'A';
  else
    return v + '0';
}

/*
 * Handles the GET_DESCRIPTOR callback. All required descriptors must be
 * handled here.
 */

static const USBDescriptor *get_descriptor(USBDriver *usbp,
                                           uint8_t dtype,
                                           uint8_t dindex,
                                           uint16_t lang) {

  (void)usbp;
  (void)lang;
  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:
    return &vcom_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION:
    return &vcom_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex == 3) {
      uint16_t *str = (uint16_t *) &vcom_string3[2];
      int i;
      uint8_t *ID = (uint8_t *) UID_BASE;
      for (i =  0; i < 12; i++) {
	  *str++ = toASCII(ID[i] & 0xf);
	  *str++ = toASCII((ID[i]>>4) & 0xf);
      }
    }
    if (dindex < 4) 
      return &vcom_strings[dindex];
  }
  return NULL;
}


//  IN EP1 state.

static USBInEndpointState ep1instate;

//  EP1 initialization structure (IN only)

static const USBEndpointConfig ep1config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  NULL, 
  NULL, 
  0x0040,
  0,
  &ep1instate,
  NULL,
  1,
  NULL
};

//   OUT EP2 state.

static USBOutEndpointState ep2outstate;

//  EP2 initialization structure (OUT only)

static const USBEndpointConfig ep2config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  NULL,
  NULL,
  0,
  0x0040,
  NULL,
  &ep2outstate,
  1,
  NULL
};


// Handles the USB driver global events.

static void usb_event(USBDriver *usbp, usbevent_t event) {

  switch (event) {
  case USB_EVENT_RESET:
    return;
  case USB_EVENT_ADDRESS:
    return;
  case USB_EVENT_CONFIGURED:
    chSysLockFromISR();

    // enable endpoints

    usbInitEndpointI(usbp, BULK_IN_EP,                 &ep1config);
    usbInitEndpointI(usbp, BULK_OUT_EP,                &ep2config);

    chSysUnlockFromISR();
    return;
  case USB_EVENT_UNCONFIGURED:
    return;
  case USB_EVENT_SUSPEND:
    return;
  case USB_EVENT_WAKEUP:
    return;
  case USB_EVENT_STALLED:
    return;
  }
  return;
}

// USB driver configuration.

const USBConfig usbcfg = {
  usb_event,
  get_descriptor,
  0,
  0, //sof_handler
};


//  Helpers for bulk endpoints

msg_t BULK_Receive(uint8_t *Buf, uint16_t len) {
  return usbReceive(&USBD1,BULK_OUT_EP,Buf,len);
}

msg_t BULK_Transmit(uint8_t *Buf, uint16_t len){
  if (usbTransmit(&USBD1,BULK_IN_EP,Buf,len))
    return 0;
  else
    return len;
}


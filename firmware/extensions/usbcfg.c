/*
    ChibiOS - Copyright (C) 2006..2016 Giovanni Di Sirio

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

/* Virtual serial port over USB.*/

SerialUSBDriver SDU1;

/*
 * USB Device Descriptor.
 */

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

/*
 * Device Descriptor wrapper.
 */
static const USBDescriptor vcom_device_descriptor = {
  sizeof vcom_device_descriptor_data,
  vcom_device_descriptor_data
};

/* Configuration Descriptor tree for a CDC.*/

static const uint8_t vcom_configuration_descriptor_data[106] = {
  /* Configuration Descriptor.*/
  USB_DESC_CONFIGURATION(98,//106,           /* wTotalLength.                    */
                         0x03,          /* bNumInterfaces.                  */
                         0x01,          /* bConfigurationValue.             */
                         0,             /* iConfiguration.                  */
                         0xC0,          /* bmAttributes (self powered).     */
                         50),           /* bMaxPower (100mA).               */

  // bulk usb
#if 0
  USB_DESC_INTERFACE_ASSOCIATION(0x00, /* bFirstInterface.                  */
				 0x01, /* bInterfaceCount.                  */
				 0xFF, /* bFunctionClass (Vendor Specific).  */
				 0x00, /* bFunctionSubClass.                */
				 0x00, /* bFunctionProcotol                 */
				 4),   /* iInterface.                       */
#endif
  /* Interface Descriptor.*/
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
  // CDC
  /* IAD Descriptor */
  USB_DESC_INTERFACE_ASSOCIATION(0x01, /* bFirstInterface.                  */
                                 0x02, /* bInterfaceCount.                  */
                                 0x02, /* bFunctionClass (CDC).             */
                                 0x00, /* bFunctionSubClass.                */
                                 0x00, /* bFunctionProcotol                 */
                                 2),   /* iInterface.                       */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x01,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x01,          /* bNumEndpoints.                   */
                         0x02,          /* bInterfaceClass (Communications
                                           Interface Class, CDC section
                                           4.2).                            */
                         0x02,          /* bInterfaceSubClass (Abstract
                                         Control Model, CDC section 4.3).   */
                         0x01,          /* bInterfaceProtocol (AT commands,
                                           CDC section 4.4).                */
                         0),            /* iInterface.                      */
  /* Header Functional Descriptor (CDC section 5.2.3).*/
  USB_DESC_BYTE         (5),            /* bLength.                         */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x00),         /* bDescriptorSubtype (Header
                                           Functional Descriptor.           */
  USB_DESC_BCD          (0x0110),       /* bcdCDC.                          */
  /* Call Management Functional Descriptor. */
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x01),         /* bDescriptorSubtype (Call Management
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x00),         /* bmCapabilities (D0+D1).          */
  USB_DESC_BYTE         (0x02),         /* bDataInterface.                */
  /* ACM Functional Descriptor.*/
  USB_DESC_BYTE         (4),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x02),         /* bDescriptorSubtype (Abstract
                                           Control Management Descriptor).  */
  USB_DESC_BYTE         (0x02),         /* bmCapabilities.                  */
  /* Union Functional Descriptor.*/
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x06),         /* bDescriptorSubtype (Union
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x01),         /* bMasterInterface (Communication
                                           Class Interface).                */
  USB_DESC_BYTE         (0x02),         /* bSlaveInterface0 (Data Class
                                           Interface).                      */
  /* Endpoint 5 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_INTERRUPT_REQUEST_EP|0x80,
                         0x03,          /* bmAttributes (Interrupt).        */
                         0x0008,        /* wMaxPacketSize.                  */
                         0xFF),         /* bInterval.                       */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x02,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x02,          /* bNumEndpoints.                   */
                         0x0A,          /* bInterfaceClass (Data Class
                                           Interface, CDC section 4.5).     */
                         0x00,          /* bInterfaceSubClass (CDC section
                                           4.6).                            */
                         0x00,          /* bInterfaceProtocol (CDC section
                                           4.7).                            */
                         0x00),         /* iInterface.                      */
  /* Endpoint 4 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_DATA_AVAILABLE_EP,       /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00),         /* bInterval.                       */
  /* Endpoint 4 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_DATA_REQUEST_EP|0x80,    /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00)          /* bInterval.                       */
};

/*
 * Configuration Descriptor wrapper.
 */
static const USBDescriptor vcom_configuration_descriptor = {
  sizeof vcom_configuration_descriptor_data,
  vcom_configuration_descriptor_data
};

/*
 * U.S. English language identifier.
 */
static const uint8_t vcom_string0[] = {
  USB_DESC_BYTE(4),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  USB_DESC_WORD(0x0409)                 /* wLANGID (U.S. English).          */
};

/*
 * Vendor string.
 */
static const uint8_t vcom_string1[] = {
  USB_DESC_BYTE(10),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'I', 0, 'U', 0, 'C', 0, 'S',0
};

/*
 * Device Description string.
 */
static const uint8_t vcom_string2[] = {
  USB_DESC_BYTE(14),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'S', 0, 'T', 0, 'L', 0,'i', 0, 'n', 0, 'k',0
};

/*
 * Serial Number string.
 */
static uint8_t vcom_string3[50] __attribute__ ((aligned (2))) = {
  USB_DESC_BYTE(50),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  '0' + CH_KERNEL_MAJOR, 0,
  '0' + CH_KERNEL_MINOR, 0,
  '0' + CH_KERNEL_PATCH, 
  0
};

/*
 * Strings wrappers array.
 */
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

/**
 * @brief   IN EP1 state.
 */

static USBInEndpointState ep1instate;

/**
 * @brief   EP1 initialization structure (IN only)
 */

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

/**
 * @brief   OUT EP2 state.
 */

static USBOutEndpointState ep2outstate;

/**
 * @brief   EP2 initialization structure (OUT only)
 */


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

/**
 * @brief   IN EP4 state.
 */

static USBInEndpointState ep4instate;

/**
 * @brief   OUT EP4 state.
 */

static USBOutEndpointState ep4outstate;

/**
 * @brief   EP4 initialization structure (both IN and OUT).
 */

static const USBEndpointConfig ep4config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  sduDataTransmitted,
  sduDataReceived,
  0x0040,
  0x0040,
  &ep4instate,
  &ep4outstate,
  1,
  NULL
};

/**
 * @brief   IN EP5 state.
 */
static USBInEndpointState ep5instate;

/**
 * @brief   EP2 initialization structure (IN only).
 */
static const USBEndpointConfig ep5config = {
  USB_EP_MODE_TYPE_INTR,
  NULL,
  sduInterruptTransmitted,
  NULL,
  0x0010,
  0x0000,
  &ep5instate,
  NULL,
  1,
  NULL
};


/*
 * Handles the USB driver global events.
 */

static void usb_event(USBDriver *usbp, usbevent_t event) {
  extern SerialUSBDriver SDU1;

  switch (event) {
  case USB_EVENT_RESET:
    return;
  case USB_EVENT_ADDRESS:
    return;
  case USB_EVENT_CONFIGURED:
    chSysLockFromISR();

    /* Enables the endpoints specified into the configuration.
       Note, this callback is invoked from an ISR so I-Class functions
       must be used.*/

    usbInitEndpointI(usbp, BULK_IN_EP,                 &ep1config);
    usbInitEndpointI(usbp, BULK_OUT_EP,                &ep2config);
    usbInitEndpointI(usbp, USBD1_DATA_REQUEST_EP,      &ep4config);
    usbInitEndpointI(usbp, USBD1_INTERRUPT_REQUEST_EP, &ep5config);

    /* Resetting the state of the CDC subsystem.*/
    sduConfigureHookI(&SDU1);

    chSysUnlockFromISR();
    return;
  case USB_EVENT_UNCONFIGURED:
    return;
  case USB_EVENT_SUSPEND:
    chSysLockFromISR();

    /* Disconnection event on suspend.*/
    //    sduDisconnectI(&SDU1);

    chSysUnlockFromISR();
    return;
  case USB_EVENT_WAKEUP:
    return;
  case USB_EVENT_STALLED:
    return;
  }
  return;
}

/*
 * Handles the USB driver global events.
 */
static void sof_handler(USBDriver *usbp) {

  (void)usbp;

  osalSysLockFromISR();
  sduSOFHookI(&SDU1);
  osalSysUnlockFromISR();
}

/*
 * USB driver configuration.
 */
const USBConfig usbcfg = {
  usb_event,
  get_descriptor,
  sduRequestsHook,
  sof_handler
};

/*
 * Serial over USB driver configuration.
 */
const SerialUSBConfig serusbcfg = {
  &USBD1,
  USBD1_DATA_REQUEST_EP,
  USBD1_DATA_AVAILABLE_EP,
  USBD1_INTERRUPT_REQUEST_EP
};


/*
 *  Helpers for bulk endpoints
 */

msg_t BULK_Receive(uint8_t *Buf, uint16_t len) {
  return usbReceive(&USBD1,BULK_OUT_EP,Buf,len);
}

msg_t BULK_Transmit(uint8_t *Buf, uint16_t len){
  if (usbTransmit(&USBD1,BULK_IN_EP,Buf,len))
    return 0;
  else
    return len;
}


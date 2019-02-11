#ifndef USBCFG_H
#define USBCFG_H

/* VID, PID */

#define USBD_VID   0x483
#define USBD_PID   0x5744

/* Endpoints */

#define BULK_IN_EP   1
#define BULK_OUT_EP  2

/* Interface */

extern const USBConfig usbcfg;
msg_t BULK_Receive(uint8_t *Buf, uint16_t len);
msg_t BULK_Transmit(uint8_t *Buf, uint16_t len);

#endif  /* USBCFG_H */


/**************************************************************************
*      Copyright 2018  Geoffrey Brown                                     *
*                                                                         *
*                                                                         *
*                                                                         *
* Licensed under the Apache License, Version 2.0 (the "License");         *
* you may not use this file except in compliance with the License.        *
* You may obtain a copy of the License at                                 *
*                                                                         *
*     http://www.apache.org/licenses/LICENSE-2.0                          *
*                                                                         *
* Unless required by applicable law or agreed to in writing, software     *
* distributed under the License is distributed on an "AS IS" BASIS,       *
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
* See the License for the specific language governing permissions and     *
* limitations under the License.                                          *
**************************************************************************/

#include "hal.h"
#include "stlink.h"
#include "dp_swd.h"
#include "usbcfg.h"
//#include "chprintf.h"
#include "app.h"

#define DATABUFSIZE 512
static enum stlink_mode mode = STLINK_MODE_UNKNOWN;

static uint8_t txbuf[128] __attribute__ ((aligned (4)));
static uint8_t databuf[DATABUFSIZE] __attribute__ ((aligned (4)));

static uint16_t lastrwstatus = STLINK_DEBUG_ERR_OK;

static inline uint8_t *PACK16(uint8_t *buf, uint16_t val) {
  buf[0] = val;
  buf[1] = val>>8;
  return buf + 2;
}

static inline uint8_t *PACK32(uint8_t *buf, uint32_t val) {
  buf[0] = val;
  buf[1] = val >> 8;
  buf[2] = val >> 16;
  buf[3] = val >> 24;
  return buf + 4;
}

static inline uint32_t UNPACK32(uint8_t *buf) {
  return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static inline uint16_t UNPACK16(uint8_t *buf) {
  return buf[0] | (buf[1] << 8);
}

int stlink_eval(uint8_t *buf) {
  uint8_t  idx;
  uint16_t len;
  uint32_t addr;
  uint32_t value;

  switch (*buf++) {
  case STLINK_GET_VERSION:  
    PACK16(txbuf, STLINK_VERSION);
    PACK16(txbuf+2,USBD_VID);
    PACK16(txbuf+4,USBD_PID);
    BULK_Transmit(txbuf, 6);    // return 6 bytes
    return 0;
  case STLINK_DEBUG_COMMAND:    // return 0 bytes
    break;
  case STLINK_DFU_COMMAND:      // return 0 bytes
    return 0;
  case STLINK_GET_CURRENT_MODE: 
    PACK16(txbuf,mode);
    BULK_Transmit(txbuf,2);     // return 2 bytes
    return 0;
  case STLINK_GET_TARGET_VOLTAGE:
    PACK32(txbuf,240);
    PACK32(txbuf+4,vlipo100);
    BULK_Transmit(txbuf,8);     // return 8 bytes
    return 0;
  default:
    return -1;
  }

  // evaluate debug command

  int swderr;
  uint32_t tmpreg;
  msg_t rlen;
  switch (*buf++) {

  case STLINK_DEBUG_ENTER_JTAG:   
    PACK16(txbuf,STLINK_DEBUG_ERR_FAULT);
    BULK_Transmit(txbuf,2);    // return 2 bytes
    break;
  case STLINK_DEBUG_GETSTATUS:
    PACK16(txbuf,STLINK_DEBUG_ERR_OK);
    BULK_Transmit(txbuf,2);    // return 2 bytes
    break;
  case STLINK_DEBUG_RESETSYS:
    PACK16(txbuf,STLINK_DEBUG_ERR_FAULT); 
    BULK_Transmit(txbuf,2);    // no reset capability ?
    break;
  case STLINK_DEBUG_READMEM_32BIT:
    addr = UNPACK32(buf);
    len =  UNPACK16(&buf[4]);
    while (len) {
      int tmplen = len > DATABUFSIZE ? DATABUFSIZE : len;
      len -= tmplen;
      swderr = SWD_readMem32(addr, (uint32_t *) databuf, tmplen);
      addr += tmplen;
      if (swderr) {
	lastrwstatus = STLINK_DEBUG_ERR_FAULT;
	EPRINTF("error on mem32 read: 0x%x\r\n", addr);
	SWD_Open();  // reset interface
	break;
      } else {
	lastrwstatus = STLINK_DEBUG_ERR_OK;
	if (BULK_Transmit(databuf,tmplen) == 0){
	  lastrwstatus = STLINK_DEBUG_ERR_FAULT;
	  break;
	}
      }
    }
    break;
  case STLINK_DEBUG_WRITEMEM_32BIT:
    addr = UNPACK32(buf);
    len =  UNPACK16(&buf[4]);
    swderr = 0;
    lastrwstatus = STLINK_DEBUG_ERR_OK;
    while (0 < len) {
      int tmplen = len > DATABUFSIZE ? DATABUFSIZE : len;
      len -= tmplen;
      rlen = BULK_Receive(databuf, tmplen);
      if (tmplen != rlen) {
	EPRINTF("Received %d bytes, expected %d bytes\n", tmplen, rlen);
	lastrwstatus = STLINK_DEBUG_ERR_FAULT;
	break;
      }
      if (!swderr)
	swderr |= SWD_writeMem32(addr, (uint32_t *) databuf, tmplen);
      addr += tmplen;
    }
    if (swderr) {
      lastrwstatus = STLINK_DEBUG_ERR_FAULT;
      EPRINTF("error on write mem32 \n");
      SWD_Open();  // reset interface
    }
    break;
  case STLINK_DEBUG_WRITEMEM_8BIT:
    addr = UNPACK32(buf);
    len =  UNPACK16(&buf[4]);
    swderr = 0;
    lastrwstatus = STLINK_DEBUG_ERR_OK;
    while (0 < len) {
      int tmplen = len > DATABUFSIZE ? DATABUFSIZE : len;
      len -= tmplen;
      rlen = BULK_Receive(databuf, tmplen);
      if (tmplen != rlen) {
	EPRINTF("Received %d bytes, expected %d bytes\n", tmplen, rlen);
	lastrwstatus = STLINK_DEBUG_ERR_FAULT;
	break;
      }
      if (!swderr)
	swderr = SWD_writeMem8(addr, databuf, tmplen);
      addr += tmplen;
    }
    if (swderr) {
      lastrwstatus = STLINK_DEBUG_ERR_FAULT;
      EPRINTF("error on write mem8 \n");
      SWD_Open();  // reset interface
    }
    break;
  case STLINK_DEBUG_READMEM_8BIT:
    addr = UNPACK32(buf);
    len =  UNPACK16(&buf[4]);

    while (0 < len) {
      int tmplen = len > DATABUFSIZE ? DATABUFSIZE : len;
      len -= tmplen;
      swderr = SWD_readMem8(addr, databuf, tmplen);
      addr += tmplen;
      if (swderr) {
	lastrwstatus = STLINK_DEBUG_ERR_FAULT;
	EPRINTF("error on mem8 read: 0x%x\r\n", addr);
	SWD_Open();  // reset interface
	break;
      } else {
	lastrwstatus = STLINK_DEBUG_ERR_OK;
	if (BULK_Transmit(databuf,tmplen) == 0) {
	  lastrwstatus = STLINK_DEBUG_ERR_FAULT;
	  EPRINTF("error on mem8 read: 0x%x\r\n", addr);
	  break;
	}
      }
    }
    break;
  case STLINK_DEBUG_EXIT:
    mode = STLINK_MODE_UNKNOWN;
    EPRINTF("debug exit\r\n");
    SWD_Close();
    // no return packet
    break;
  case STLINK_DEBUG_READCOREID:
    PACK32(txbuf,CoreID);
    BULK_Transmit(txbuf,4);     // return 4 bytes
    break;
  case STLINK_DEBUG_APIV2_ENTER:   // here's where we enter swd
    if ((swderr = SWD_Open())) {
      mode = STLINK_MODE_UNKNOWN;
      PACK16(txbuf,STLINK_DEBUG_ERR_FAULT);
      EPRINTF("swd error %d\r\n", swderr);
    } else {
      mode = STLINK_MODE_DEBUG_SWD;
      PACK16(txbuf,STLINK_DEBUG_ERR_OK);
    }
    BULK_Transmit(txbuf,2);    // return 2 bytes
    break;
  case STLINK_DEBUG_APIV2_READREG:
    idx = *buf++; 
    if ((swderr = SWD_readReg(idx, &tmpreg)))
      swderr = SWD_readReg(idx, &tmpreg);
    if (swderr)
      EPRINTF("get reg %d %d error %d\n", idx, tmpreg, swderr);
    PACK32(txbuf+4,tmpreg);
    BULK_Transmit(txbuf,8);    // return 4 bytes
    break;
   case STLINK_DEBUG_APIV2_WRITEREG:
    idx = *buf++;
    value = UNPACK32(buf);
    swderr = SWD_writeReg(idx, value);
    if (swderr) {
      EPRINTF("write reg error %d %x\n", idx, value);
      PACK16(txbuf,STLINK_DEBUG_ERR_FAULT);
    }
    else
      PACK16(txbuf,STLINK_DEBUG_ERR_OK);
    BULK_Transmit(txbuf,2);    // return 2 bytes
    break;
  case STLINK_DEBUG_APIV2_WRITEDEBUGREG:  // write a single 32 bit value
    addr  = UNPACK32(buf);
    value =  UNPACK32(&buf[4]); 
    swderr = SWD_writeWord(addr,value);
    if (swderr)
      PACK16(txbuf,STLINK_DEBUG_ERR_FAULT);
    else
      PACK16(txbuf,STLINK_DEBUG_ERR_OK);
    BULK_Transmit(txbuf,2);        // return 2 bytes
    break;
  case STLINK_DEBUG_APIV2_READDEBUGREG:   // read a single 32 bit value
    addr  = UNPACK32(buf);
    swderr = SWD_readWord(addr,&tmpreg);
    if (swderr)
      PACK16(txbuf,STLINK_DEBUG_ERR_FAULT);
    else
      PACK16(txbuf,STLINK_DEBUG_ERR_OK);
    PACK32(txbuf+4,tmpreg);
    BULK_Transmit(txbuf,8);        // return 8 bytes
    break;
  case STLINK_DEBUG_APIV2_READALLREGS:
    lastrwstatus = STLINK_DEBUG_ERR_OK;
    for (int i = 0; i < 21; i++) {
      if (SWD_readReg(i, &tmpreg)) {
	lastrwstatus = STLINK_DEBUG_ERR_FAULT;
	break;
      }
      PACK32(txbuf+i*4,tmpreg);
    }
    BULK_Transmit(txbuf,84);   // return 84 bytes
    break;
  case STLINK_DEBUG_APIV2_GETLASTRWSTATUS:
    PACK16(txbuf,lastrwstatus);
    BULK_Transmit(txbuf,2);    // return 2 bytes
    break;
  case STLINK_DEBUG_APIV2_DRIVE_NRST:
    value = *buf;
    switch(value) {
    case 0:
      palSetLine(LINE_TGT_RESET);
      break;
    case 1:
      palClearLine(LINE_TGT_RESET);
      break;
    case 2:
      palSetLine(LINE_TGT_RESET);
      chThdSleepMilliseconds(1);
      palClearLine(LINE_TGT_RESET);
	break;
    default:
      EPRINTF("unexpected Drive_nrst parameter %d\n", value);
    }
    PACK16(txbuf,STLINK_DEBUG_ERR_OK);
    BULK_Transmit(txbuf,2);        // return 2 bytes
    break;
  case STLINK_DEBUG_APIV2_RESETSYS:
    SWD_LineReset(&value);
    BULK_Transmit(txbuf,2);        // return 2 bytes
    break;
  case STLINK_DEBUG_APIV2_SWD_SET_FREQ:
    PACK16(txbuf,STLINK_DEBUG_ERR_OK);
    BULK_Transmit(txbuf,2);        // return 2 bytes
    break;
  case STLINK_DEBUG_FORCEDEBUG:
  case STLINK_DEBUG_RUNCORE:
  case STLINK_DEBUG_STEPCORE:
  case STLINK_DEBUG_APIV2_START_TRACE_RX:
  case STLINK_DEBUG_APIV2_STOP_TRACE_RX:
  case STLINK_DEBUG_APIV2_GET_TRACE_NB:

  case STLINK_DEBUG_ENTER_SWD:
  default:
    EPRINTF("Unsupported command %x\n", *(buf - 1));
    PACK16(txbuf,STLINK_DEBUG_ERR_FAULT);
    BULK_Transmit(txbuf,2);        // return 2 bytes
    return -1;
  }
  return 0;
}


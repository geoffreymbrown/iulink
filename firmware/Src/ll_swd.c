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

#include <stdint.h>
#include <dp_swd.h>
#include <debug_cm.h>
#include "hal.h"
#include "usbcfg.h"
#include "app.h"
#include "board.h"

#define REGRETRIES 20
#define DELCNT 1

static const int AUTO_INCREMENT_PAGE_SIZE = 1024 ;
static const int CSW_VALUE = (CSW_RESERVED | CSW_MSTRDBG | CSW_HPROT
			      | CSW_DBGSTAT | CSW_SADDRINC);
uint32_t CoreID = 0;

static inline void delay(int i){
  for (; i > 0; i--) {
    asm("mov r0,r0");
  }
}

static inline void toAlternate(ioline_t line) {
  stm32_gpio_t *port = PAL_PORT(line);
  uint32_t pin       = PAL_PAD(line);
  ;  MODIFY_REG(port->MODER, 3 << (pin * 2), 2 << (pin * 2));
}

static inline void toInput(ioline_t line) {
  stm32_gpio_t *port = PAL_PORT(line);
  uint32_t pin       = PAL_PAD(line);
  MODIFY_REG(port->MODER, 3 << (pin * 2), 0);
}

static inline void toOutput(ioline_t line) {
  stm32_gpio_t *port = PAL_PORT(line);
  uint32_t pin       = PAL_PAD(line);
  MODIFY_REG(port->MODER, 3 << (pin * 2), 1 << (pin * 2));
}

static void _SetSWPinsIdle(void){
  palClearLine(LINE_TGT_SWCLK);
  palSetLine(LINE_TGT_SWDIO);

  toInput(LINE_TGT_SWDIO);
  toOutput(LINE_TGT_SWCLK);
}

static void _ResetDebugPins(void){
  toInput(LINE_TGT_SWDIO);
}

static inline void _SetSWDIOasOutput(void){
  toOutput(LINE_TGT_SWDIO);
}
static inline void _SetSWDIOasInput(void){
  toInput(LINE_TGT_SWDIO);
}

static inline uint32_t SWDIO_IN(void){
  uint8_t b;

  b = palReadLine(LINE_TGT_SWDIO);
  palSetLine(LINE_TGT_SWCLK);
  delay(DELCNT);
  palClearLine(LINE_TGT_SWCLK);

  return b;
}

static inline uint32_t SW_ShiftIn(uint8_t bits){
  int i;
  uint32_t in = 0;
  for (i = 0; i < bits; i++) {
    in = (in >> 1) | ((SWDIO_IN()&1) << (bits - 1));
  }
  return in;
}

static inline uint32_t SW_ShiftInBytes(uint8_t bytes) {
 int i;
 uint32_t tmp;
 if (bytes > 4) return 0;

 for (i = 0; i < bytes; i++) {
   ((uint8_t *) &tmp)[i] = SW_ShiftIn(8);
 }
 return tmp;
}

static inline void SW_ShiftOutBytes(uint32_t data, uint8_t bytes)
{
  int i;
  if (bytes > 4) return;

  for (i = 0; i < bytes*8; i++) {
    if (data & 1)
      palSetLine(LINE_TGT_SWDIO);
    else
      palClearLine(LINE_TGT_SWDIO);
    palSetLine(LINE_TGT_SWCLK);
    delay(DELCNT);
    data = data >> 1;
    palClearLine(LINE_TGT_SWCLK);
  }
}

static inline uint32_t Parity(uint32_t x) {
  uint32_t y;
  y = x ^ (x >> 1);
  y = y ^ (y >> 2);
  y = y ^ (y >> 4);
  y = y ^ (y >> 8);
  y = y ^ (y >>16);
  return y & 1;
}

static uint32_t SWD_TransactionBB(uint32_t req, uint32_t *data) {

  uint32_t ack;
  uint32_t pbit;

  _SetSWDIOasOutput();                   // Set pin direction
  SW_ShiftOutBytes(req,1);               // Send header

  _SetSWDIOasInput();                    // Set pin direction
  ack = (SW_ShiftIn(4) >> 1) & 7;        // ACK, toss the turnaround bit

  switch (ack) {
  case SW_ACK_OK :                       // good to go
    if (req & SW_REQ_RnW) {              // read
      *data = SW_ShiftInBytes(4);        // get data
      pbit = SW_ShiftIn(2)&1;            // get parity bit, toss turnaround
      if (pbit ^ Parity(*data)) {        // parity check
	ack = SW_ACK_PARITY_ERR;
	EPRINTF("parity error data 0x%x pbit %x\r\n", *data, pbit);
      }
      _SetSWDIOasOutput();               // restore direction
    } else {                             // write
      SW_ShiftIn(1);                     // turnaround
      _SetSWDIOasOutput();               // restore direction
      SW_ShiftOutBytes(*data,4);         // data
      SW_ShiftOutBytes(Parity(*data), 1);// parity 
    }
    break;

  case SW_ACK_WAIT  :
  case SW_ACK_FAULT :
      SW_ShiftIn(1);                      // turnaround
      _SetSWDIOasOutput();                // restore direction
      break;

  default :                               // no ack, back off in case of data phase
    SW_ShiftInBytes(4);                   // data
    SW_ShiftIn(2);                        // parity + turn
    _SetSWDIOasOutput();                  // restore direction
  }
  _SetSWDIOasInput();                    // Set pin direction
  return ack;
}

static uint32_t SWD_Transaction(uint32_t req, uint32_t *data, uint32_t retry){
  uint32_t ack   = 0;
  // try transaction  (always at least once)
  do {  
    ack = SWD_TransactionBB(req, data);
    if ((ack == SW_ACK_WAIT) && retry--)
      continue;
    else
      break;
  } while (retry);
  return ack;   // errors handled up call chain
}

static void SW_ShiftReset(void){
  SW_ShiftOutBytes(0xffffffff, 4);
  SW_ShiftOutBytes(0xffffffff, 3);
}

uint32_t SWD_LineReset(uint32_t *idcode){
  uint32_t ack;

  // SWD reset sequence:  
  //          56 1's, 8 0's, Read IDcode
  //          SW_IDCODE_RD not allowed to wait or fault
  _SetSWDIOasOutput();
  SW_ShiftReset();  
  SW_ShiftOutBytes(0,1);
  ack = SWD_Transaction(SW_IDCODE_RD, idcode, 0);
  return ack;
}

static uint32_t SWD_Connect(uint32_t *idcode){
  // Init Pins 
  _SetSWPinsIdle(); 
  // Select SWD Port
  _SetSWDIOasOutput();
  SW_ShiftReset();              
  SW_ShiftOutBytes(0xE79E,2);
  // Finish with Line reset
  return SWD_LineReset(idcode); 
}

static void SWD_Disconnect(void){
 // set pins to idle state
  _SetSWPinsIdle(); 
 // select JTAG port
  _SetSWDIOasOutput();
  SW_ShiftReset();  
  SW_ShiftOutBytes(0xE73C,2);
  SW_ShiftReset();  
  // Release pins (except nReset)
  _ResetDebugPins();
}

/*
 *   Public Debug Port access functions
 *     TRANSACTION macro used to catch errors and
 *     clear error condition
 */

#define TRANSACTION(reg,op) \
  do { int ack = errorClear(SWD_Transaction(reg,op,MAX_SWD_RETRY)); \
    if (ack) { \
      EPRINTF("trans fail %d reg %x op %x\r\n", ack, reg, op);	\
      return ack; \
    }} while (0)

static uint32_t errorClear(uint32_t ack){
  uint32_t tmp;
  if (ack == SW_ACK_OK)
    return 0;

  // parity error requires no special clearing

  if (ack == SW_ACK_PARITY_ERR)
    return 1;

  // Read CSR if this fails, we need to do a line reset 

  if (ack == 7) {  // nothing came back !
    EPRINTF("transaction returned 7\r\n");
    return 1;
  }

  EPRINTF("Error clear %d\r\n", ack);
  if (SW_ACK_OK != SWD_Transaction(SW_CTRLSTAT_RD, &tmp, 0))  {
    EPRINTF("Error clear line reset\r\n");
    SWD_LineReset(&tmp);
  } 
 
  // clear error/sticky bits in abort reg
  // if this fails, we're truly wedged

  tmp = (STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR);
  if (SW_ACK_OK != SWD_Transaction(SW_ABORT_WR, &tmp,0)) {
    EPRINTF("Clear sticky bits failed !\r\n");
    return 2;
  }


  // set CSW to 32-bit, autoinc
  // this better not fail !

  tmp = CSW_VALUE | CSW_SIZE32;       
  if (SW_ACK_OK != SWD_Transaction(SW_CSW_WR, &tmp, MAX_SWD_RETRY))
    return 2;
  return 1;
}

static uint32_t _SWD_writeMem32(uint32_t address, uint32_t *data, 
				uint32_t size) {
  uint32_t i;
  uint32_t tmp;

  tmp = CSW_VALUE | CSW_SIZE32;
  TRANSACTION(SW_CSW_WR, &tmp);  
  // Write TAR register 
  TRANSACTION(SW_TAR_WR, &address);
  // Write data
  for (i = 0; i < size/4; i++) 
    TRANSACTION(SW_DRW_WR,data++);
  // dummy read to flush transaction
  TRANSACTION(SW_RDBUFF_RD,&tmp);
  return 0;
}

uint32_t SWD_writeMem32(uint32_t address, uint32_t *data, 
		       uint32_t size) {
  uint32_t len;
  while (size) {
    int err;
    len = AUTO_INCREMENT_PAGE_SIZE - 
      (address & (AUTO_INCREMENT_PAGE_SIZE - 1));
    if (size < len)
      len = size;
    if ((err = _SWD_writeMem32(address, data, len)))
      if ((err = _SWD_writeMem32(address, data, len)))
	return err;
    address += len;
    data    += len/4;
    size    -= len;
  }
  return 0;
}

static uint32_t _SWD_readMem32(uint32_t address, uint32_t *data, 
			       uint32_t size) {
  uint32_t i;
  uint32_t tmp;

  tmp = CSW_VALUE | CSW_SIZE32;
  TRANSACTION(SW_CSW_WR, &tmp);  
  // Write TAR register 
  TRANSACTION(SW_TAR_WR, &address);
  // Read first word, discard return value
  TRANSACTION(SW_DRW_RD,data);
  // Read data
  for (i = 1; i < size/4; i++) 
    TRANSACTION(SW_DRW_RD,data++);
  // complete the transaction (last data)
  TRANSACTION(SW_RDBUFF_RD,data);
  return 0;
}

uint32_t SWD_readMem32(uint32_t address, uint32_t *data, 
				uint32_t size) {
  uint32_t len;
  while (size) {
    int err;
    len = AUTO_INCREMENT_PAGE_SIZE - 
      (address & (AUTO_INCREMENT_PAGE_SIZE - 1));
    if (size < len)
      len = size;
    if ((err = _SWD_readMem32(address, data, len)))
      if ((err = _SWD_readMem32(address, data, len)))
	return err;
    address += len;
    data    += len/4;
    size    -= len;
  }
  return 0;
}

static uint32_t SWD_writeByte(uint32_t address, uint8_t data) {
  uint32_t tmp;
  TRANSACTION(SW_TAR_WR, &address);
  tmp = (data) << ((address & 3) << 3);
  TRANSACTION(SW_DRW_WR,&tmp);  
  return 0;
}

uint32_t SWD_writeMem8(uint32_t address, uint8_t *data, uint32_t size) {
  uint32_t tmp;
  int err = 0;
  // Write CSW register

  tmp = CSW_VALUE | CSW_SIZE8;
  TRANSACTION(SW_CSW_WR, &tmp);  

  for (uint32_t i = 0; i < size; i++) {
    // let it fail once
    if ((err = SWD_writeByte(address + i, *data)))
      if ((err = SWD_writeByte(address + i, *data)))
	break;
    data++;
  }

  // reset CSW
  if (err)
    EPRINTF("SWD_writeMem8 failed\r\n");
  tmp = CSW_VALUE | CSW_SIZE32;
  TRANSACTION(SW_CSW_WR, &tmp);  
  return err;
}

static uint32_t SWD_readByte(uint32_t address, uint8_t *data) {
  uint32_t tmp;
  TRANSACTION(SW_TAR_WR, &address);
  TRANSACTION(SW_DRW_RD,&tmp);  
  TRANSACTION(SW_RDBUFF_RD,&tmp);
  *data = (tmp >> ((address & 3) << 3));
  return 0;
}

uint32_t SWD_readMem8(uint32_t address, uint8_t *data, uint32_t size) {
  uint32_t tmp;
  uint32_t i;
  int err = 0;
  // Write CSW register
  tmp = CSW_VALUE | CSW_SIZE8;
  for (i = 0; i < size; i++) 
    if ((err = SWD_readByte(address + i, data + i)))
      if ((err = SWD_readByte(address + i, data + i)))
	break;
  if (err)
    EPRINTF("SWD_readMem8 failed\r\n");
  tmp = CSW_VALUE | CSW_SIZE32;
  TRANSACTION(SW_CSW_WR, &tmp);  
  return err;
}

int32_t SWD_Close() {
  uint32_t tmp;
  CoreID = 0;
  // release debug mode 
  SWD_writeWord(DBG_HCSR, DBGKEY);
  // release power up
  tmp = 0;
  SWD_Transaction(SW_CTRLSTAT_WR, &tmp, 0);
  SWD_Disconnect();
  return 0;
}

int32_t SWD_Open() {
  uint32_t tmp;
  int tries;
  
  //  SW_ShiftReset();  
  if (SWD_Connect(&CoreID) != SW_ACK_OK)
    return 1;
  // clear any pending errors
  //   EPRINTF("write clear ok\r\n");
  tmp = (STKCMPCLR | STKERRCLR | WDERRCLR | ORUNERRCLR);
  TRANSACTION(SW_ABORT_WR, &tmp);
  // Make sure DP bank select is 0
  tmp = 0;
  TRANSACTION(SW_SELECT_WR, &tmp);
  // Power up !
  //  EPRINTF("write ctrlstat\r\n");
  tmp = CSYSPWRUPREQ | CDBGPWRUPREQ; 
  TRANSACTION(SW_CTRLSTAT_WR, &tmp);
  // delay ?
  //  EPRINTF("read ctrlstat\r\n");
  tries = 10;
  while (1) {
    TRANSACTION(SW_CTRLSTAT_RD, &tmp);
    // Check power up
    if ((tmp & 0xF0000000) == 0xF0000000)
      break;
    if (tries--)
      chThdSleepMilliseconds(1);
    else {
      //  EPRINTF("power up failed 0x%x\r\n", tmp);
      return 1;
    }
  };
  // make sure everything is normal
  tmp = CSYSPWRUPREQ | CDBGPWRUPREQ | TRNNORMAL | MASKLANE;
  TRANSACTION(SW_CTRLSTAT_WR, &tmp);
  // Read ID register -- set bank register to 0xF, read 0xFC (same command as DRW_RD)
  tmp = 0xF0;
  TRANSACTION(SW_SELECT_WR, &tmp);    
  TRANSACTION(SW_IDR_RD, &tmp);   // discard result
  TRANSACTION(SW_RDBUFF_RD,&tmp);  // now read the result
  //  EPRINTF("ID register 0x%x\r\n",tmp);
  //  EPRINTF("reset apsel\r\n");
  // reset APSEL to 0
  tmp = 0;
  TRANSACTION(SW_SELECT_WR, &tmp);    
  // set CSW to 32-bit, autoinc
  tmp = CSW_VALUE | CSW_SIZE32;       
  TRANSACTION(SW_CSW_WR, &tmp);  
  // enable debugging

  return SWD_writeWord(DBG_HCSR, (DBGKEY | C_DEBUGEN));
}

uint32_t SWD_writeWord(uint32_t address, uint32_t data) {
  return _SWD_writeMem32(address, &data, 4);
}

uint32_t SWD_readWord(uint32_t address, uint32_t *data) {
  return _SWD_readMem32(address, data, 4);
}

uint32_t SWD_readReg(uint32_t idx, uint32_t *value) {
  int i;
  // request read
  if (SWD_writeWord(DBG_CRSR, idx))
    return 1;
  // wait for data

  for (i = 0; i < REGRETRIES; i++) {
    if (SWD_readWord(DBG_HCSR, value)) 
      return 1;
    if (*value & S_REGRDY)
      return SWD_readWord(DBG_CRDR, value);
  }
  return 1;
}

uint32_t SWD_writeReg(uint32_t idx, uint32_t value) {
  int i;
  // write register value
  if (SWD_writeWord(DBG_CRDR, value))
    return 1;
  // request write
  if (SWD_writeWord(DBG_CRSR, idx | REGWnR))
    return 1;
  // wait for acknowledgement
  for (i = 0; i < REGRETRIES; i++) {
    if (SWD_readWord(DBG_HCSR, &value))
      return 1;
    if (value & S_REGRDY) {
      return 0;
    }
  }
  return 1;
}


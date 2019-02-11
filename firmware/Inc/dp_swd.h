/* Notes

Register access

   address header                             10p32RA1

DP registers
                                              10p32R01
                                              --------
   00   IDCODE  r  -- no wait/fault possible  10p00101  10100101  0xA5
        ABORT   w  -- no wait/fault possible  10p00001  10000001  0x81
   01   STATUS  r  -- no wait/fault possible  10p01101  10001101  0x8D
        CONTROL w                             10p01001  10101001  0xA9
   10   SELECT  w                             10p10001  10110001  0xB1
   11   RDBUFF  r                             10p10001  10111101  0xBD

AP registers we need to access
                                              10p32R11
                                              --------
   00   CSW    r                              10p00111  10000111  0x87
               w                              10p00011  10100011  0xA3
   01   TAR    w                              10p01011  10001011  0x8B
   11   DRW    r                              10p11111  10011111  0x9F
               w                              10p11011  10111011  0xBB

Total of 11 codes

-----------------------------------

access
  after the data phase the host must do one of
    1) Immediately drive a new transaction
    2) Clock at least 8 idle cycles (low)

AP read accesses return on *Next* transaction
    1) On first AP read, discard the result
    2) Subsequent AP reads return correct results
    3) DP RDBUFF can be read to return last result 
       without intiating a new AP read

-----------------------------------

Error handling

DP accesses which may not wait or fault
    1) DPIDR read
    2) CTL/STAT read
    3) ABORT write

FAULT error -- clear by writing ABORT
WAIT  error -- repeat.  After timeout, write ABORT

Protocol error -- target does not respond/drive line
    Clearing.    Attempt to read DPIDR register, if successful
    error is cleared.  Otherwise perform line reset

------------------------------------
*/

#ifndef DP_SWD
#define DP_SWD

// ARM CoreSight SWD-DP packet request values

#define SW_IDCODE_RD            0xA5
#define SW_ABORT_WR             0x81
#define SW_CTRLSTAT_RD          0x8D
#define SW_CTRLSTAT_WR          0xA9
#define SW_RESEND_RD            0x95
#define SW_SELECT_WR            0xB1
#define SW_RDBUFF_RD            0xBD

// ARM CoreSight SWD-AP packet request values
// Select(0)

#define SW_CSW_RD               0x87
#define SW_CSW_WR               0xA3
#define SW_TAR_WR               0x8B
#define SW_DRW_RD               0x9F
#define SW_DRW_WR               0xBB

// Select(0x0F0)

#define SW_IDR_RD               0x9F

// ARM CoreSight SW-DP packet request masks

#define SW_REQ_PARK_START       0x81
#define SW_REQ_PARITY           0x20
#define SW_REQ_A32              0x18
#define SW_REQ_RnW              0x04
#define SW_REQ_APnDP            0x02

// ARM CoreSight SW-DP packet acknowledge values

#define SW_ACK_OK               0x1
#define SW_ACK_WAIT             0x2
#define SW_ACK_FAULT            0x4
#define SW_ACK_PARITY_ERR       0x8

#define DBG_Addr     (0xe000edf0)

#define REGWnR        (1 << 16)
#define MAX_SWD_RETRY 25

// Interface

extern uint32_t CoreID;
int32_t  SWD_Open(void);
int32_t  SWD_Close(void);

uint32_t SWD_writeMem32(uint32_t address, uint32_t *data, uint32_t size);
uint32_t SWD_readMem32(uint32_t address, uint32_t *data, uint32_t size);
uint32_t SWD_writeMem8(uint32_t address, uint8_t *data, uint32_t size);
uint32_t SWD_readMem8(uint32_t address, uint8_t *data, uint32_t size);
uint32_t SWD_writeWord(uint32_t address, uint32_t data);
uint32_t SWD_readWord(uint32_t address, uint32_t *data);
uint32_t SWD_readReg(uint32_t idx, uint32_t *value);
uint32_t SWD_writeReg(uint32_t idx, uint32_t value);
uint32_t SWD_LineReset(uint32_t *idcode);
#endif

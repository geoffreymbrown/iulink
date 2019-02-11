/***************************************************************************
 *   These definitions are from openocd/src/jtag/drivers/stlink_usb.c      *
 *   with modifications by Geoffrey Brown                                  *
 *                                                                         *
 *                                                                         * 
 *   SWIM contributions by Ake Rehnman                                     *
 *   Copyright (C) 2017  Ake Rehnman                                       *
 *   ake.rehnman(at)gmail.com                                              *
 *                                                                         *
 *   Copyright (C) 2011-2012 by Mathias Kuester                            *
 *   Mathias Kuester <kesmtp@freenet.de>                                   *
 *                                                                         *
 *   Copyright (C) 2012 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 *                                                                         *
 *   This code is based on https://github.com/texane/stlink                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/
#ifndef STLINK_H

#define VJTAG 13
#define VSWIM 0
#define VSTLINK 2
#define BESWAP(x) ((((x)&0xff)<<8)|(((x)>>8)&0xff))
#define STLINK_VERSION BESWAP(((VJTAG<<6)|(VSTLINK<<12)|VSWIM))


enum stlink_debug_start {
  STLINK_GET_VERSION                 = 0xF1,
  STLINK_DEBUG_COMMAND               = 0xF2,
  STLINK_DFU_COMMAND                 = 0xF3,
  STLINK_SWIM_COMMAND                = 0xF4,
  STLINK_GET_CURRENT_MODE            = 0xF5,
  STLINK_GET_TARGET_VOLTAGE          = 0xF7,

  STLINK_SWIM_ENTER                  = 0x00,
  STLINK_SWIM_EXIT                   = 0x01,
  STLINK_DFU_EXIT                    = 0x07
};
  
enum stlink_debug_commands {

  STLINK_DEBUG_ENTER_JTAG            = 0x00,
  STLINK_DEBUG_GETSTATUS             = 0x01,
  STLINK_DEBUG_FORCEDEBUG            = 0x02,
  STLINK_DEBUG_RESETSYS              = 0x03,
  STLINK_DEBUG_READALLREGS           = 0x04,
  STLINK_DEBUG_READREG               = 0x05,
  STLINK_DEBUG_WRITEREG              = 0x06,
  STLINK_DEBUG_READMEM_32BIT         = 0x07,
  STLINK_DEBUG_WRITEMEM_32BIT        = 0x08,
  STLINK_DEBUG_RUNCORE               = 0x09,

  STLINK_DEBUG_STEPCORE              = 0x0a,
  STLINK_DEBUG_APIV1_SETFP           = 0x0b,
  STLINK_DEBUG_READMEM_8BIT          = 0x0c,
  STLINK_DEBUG_WRITEMEM_8BIT         = 0x0d,
  STLINK_DEBUG_APIV1_CLEARFP         = 0x0e,
  STLINK_DEBUG_APIV1_WRITEDEBUGREG   = 0x0f,
  STLINK_DEBUG_APIV1_SETWATCHPOINT   = 0x10,

  STLINK_DEBUG_APIV1_ENTER           = 0x20,
  STLINK_DEBUG_EXIT                  = 0x21,
  STLINK_DEBUG_READCOREID            = 0x22,

  STLINK_DEBUG_APIV2_ENTER           = 0x30,
  STLINK_DEBUG_APIV2_READ_IDCODES    = 0x31,
  STLINK_DEBUG_APIV2_RESETSYS        = 0x32,
  STLINK_DEBUG_APIV2_READREG         = 0x33,
  STLINK_DEBUG_APIV2_WRITEREG        = 0x34,
  STLINK_DEBUG_APIV2_WRITEDEBUGREG   = 0x35,
  STLINK_DEBUG_APIV2_READDEBUGREG    = 0x36,

  STLINK_DEBUG_APIV2_READALLREGS     = 0x3A,
  STLINK_DEBUG_APIV2_GETLASTRWSTATUS = 0x3B,
  STLINK_DEBUG_APIV2_DRIVE_NRST      = 0x3C,

  STLINK_DEBUG_APIV2_START_TRACE_RX  = 0x40,
  STLINK_DEBUG_APIV2_STOP_TRACE_RX   = 0x41,
  STLINK_DEBUG_APIV2_GET_TRACE_NB    = 0x42,
  STLINK_DEBUG_APIV2_SWD_SET_FREQ    = 0x43,
  // other
  STLINK_DEBUG_ENTER_SWD             = 0xa3,
};


/** */
enum stlink_mode {
  STLINK_MODE_UNKNOWN = 0,
  STLINK_MODE_DFU,
  STLINK_MODE_MASS,
  STLINK_MODE_DEBUG_JTAG,
  STLINK_MODE_DEBUG_SWD,
  STLINK_MODE_DEBUG_SWIM
};

#define STLINK_DEBUG_ERR_OK             0x80
#define STLINK_DEBUG_ERR_FAULT          0x81
#define STLINK_JTAG_WRITE_ERROR         0x0c
#define STLINK_JTAG_WRITE_VERIF_ERROR   0x0d
#define STLINK_SWD_AP_WAIT              0x10
#define STLINK_SWD_DP_WAIT              0x14


#define STLINK_CORE_RUNNING             0x80
#define STLINK_CORE_HALTED              0x81
#define STLINK_CORE_STAT_UNKNOWN        -1

#define STLINK_DEV_DFU_MODE             0x00
#define STLINK_DEV_MASS_MODE            0x01
#define STLINK_DEV_DEBUG_MODE           0x02
#define STLINK_DEV_UNKNOWN_MODE           -1

#endif

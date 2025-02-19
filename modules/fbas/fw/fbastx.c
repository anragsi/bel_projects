/********************************************************************************************
 *  fbastx.c
 *
 *  created : 2020
 *  author  : Enkhbold Ochirsuren, Dietrich Beck, GSI-Darmstadt
 *  version : 04-February-2021, 14-May-2020
 *
 *  lm32 firmware for SCU running as FBAS TX node
 *  (based on common-libs/fw/example.c)
 *
 * -------------------------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2018  Dietrich Beck
 * GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 * Planckstrasse 1
 * D-64291 Darmstadt
 * Germany
 *
 * Contact: d.beck@gsi.de
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * For all questions and ideas contact: d.beck@gsi.de
 * Last update: 22-November-2018
 ********************************************************************************************/
#define FBASTX_FW_VERSION 0x000002                                     // make this consistent with makefile

// standard includes
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// includes specific for bel_projects
#include "dbg.h"                                                        // debug outputs
#include <stack.h>
#include "pp-printf.h"                                                  // print statement
#include "mini_sdb.h"                                                   // sdb stuff
#include "aux.h"                                                        // cpu and IRQ
#include "uart.h"                                                       // WR console
#include "ebm.h"                                                        // EB master

// includes for this project
#include <common-defs.h>                                                // common defs for firmware
#include <common-fwlib.h>                                               // common routines for firmware
#include <fbastx_shared_mmap.h>                                         // autogenerated upon building firmware
#include <fbas.h>                                                       // application header file

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

volatile uint32_t *pECAQ;               // WB address of ECA queue
volatile uint32_t *pPPSGen;             // WB address of PPS Gen
volatile uint32_t *pWREp;               // WB address of WR Endpoint
volatile uint32_t *pIOCtrl;             // WB address of IO Control

// global variables
// shared memory layout
uint32_t *pShared;                      // pointer to begin of shared memory region
uint32_t *pCpuRamExternal;              // external address (seen from host bridge) of this CPU's RAM
uint32_t *pSharedMacHi;                 // pointer to a "user defined" u32 register; here: high bits of MAC
uint32_t *pSharedMacLo;                 // pointer to a "user defined" u32 register; here: low bits of MAC
uint32_t *pSharedIp;                    // pointer to a "user defined" u32 register; here: IP
uint32_t *pSharedApp;                   // pointer to a "user defined" u32 register set; here: application-specific register set

// other global stuff
uint32_t statusArray;                   // all status infos are ORed bit-wise into sum status, sum status is then published

// application-specific variables
mpsEventData_t mpsEventData;            // data for the MPS event message
uint64_t tsLast = 0;                    // last timestamp of system time
uint32_t cntMpsSignal = 0;              // counter for MPS signals
nodeType_t nodeType = FBAS_NODE_TX;     // default node type
opMode_t opMode = FBAS_OPMODE_DEF;      // default operation mode
uint32_t cntCmd = 0;                    // counter for user commands
uint32_t mpsTask = 0;                   // MPS-relevant tasks
uint64_t mpsTimMsgFlagId = 0;           // timing message ID for MPS flags
uint64_t mpsTimMsgEvntId = 0;           // timing message ID for MPS events
mpsTimParam_t bufMpsFlag[N_MPS_CHANNELS] = {0};   // buffer for MPS flags
timedItr_t rdItr = {0};                 // iterator used to read MPS flags

// application-specific function prototypes
static uint32_t handleEcaEvent(uint32_t usTimeout, uint32_t* mpsTask, timedItr_t* itr, mpsTimParam_t** head);
static void wrConsolePeriodic(uint32_t seconds);
static void sendMpsProtocol(uint64_t deadline, uint8_t mpsFlag);
static uint32_t setIoOe(uint32_t channel, uint32_t idx);
static uint32_t getIoOe(uint32_t channel);
static void driveIo(uint32_t channel, uint32_t idx, uint8_t value);
static void sendMpsFlag(timedItr_t* itr);
static void sendMpsEvent(timedItr_t* itr, mpsTimParam_t* buf, uint8_t n);
static void initItr(timedItr_t* itr, uint8_t total, uint64_t now, uint32_t freq);
static void updateItr(timedItr_t* itr, uint64_t now);
static void clearError(size_t len, mpsTimParam_t* buf);
static void resetMpsFlag(size_t len, mpsTimParam_t* buf);
static mpsTimParam_t* updateMpsFlag(mpsTimParam_t* buf, uint64_t evt);
static mpsTimParam_t* storeMpsFlag(mpsTimParam_t* buf, uint64_t raw);
static uint32_t driveEffLogOut(mpsTimParam_t* buf);
static mpsTimParam_t* expireMpsFlag(timedItr_t* itr);
static void setOpMode(uint64_t mode);
static void qualifyInput(size_t len, mpsTimParam_t* buf);
static void testOutput(size_t len, mpsTimParam_t* buf);
static void preparePerfMeasurement(uint64_t now, uint64_t deadline);
static void measurePerformance(uint32_t tag, uint32_t flag, uint64_t now, uint64_t deadline);
static void storeSystemTime(uint32_t offset, uint64_t now);
static int64_t measureElapsedTime(uint32_t offset, uint64_t now);

// typical init for lm32
void init()
{
  discoverPeriphery();        // mini-sdb ...
  uart_init_hw();             // needed by WR console
  cpuId = getCpuIdx();
} // init


// determine address and clear shared mem
void initSharedMem()
{
  uint32_t idx;
  uint32_t *pSharedTemp;
  uint64_t *pSharedTs;
  int      i;
  const uint32_t c_Max_Rams = 10;
  sdb_location found_sdb[c_Max_Rams];
  sdb_location found_clu;

  // get pointer to shared memory
  pShared           = (uint32_t *)_startshared;

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if(idx >= cpuId) pCpuRamExternal           = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  // print WB addresses (shared RAM, range reserved to user, command buffer etc) to WR console
  pSharedTemp = pCpuRamExternal + (SHARED_OFFS >> 2) + (COMMON_SHARED_CMD >> 2);
  DBPRINT2("fbas: CPU RAM External 0x%8x, begin shared 0x%08x, command 0x%08x\n",
      pCpuRamExternal, SHARED_OFFS, pSharedTemp);

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_BEGIN >> 2 ));
  DBPRINT2("sb_scan: app specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (FBAS_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  }

  // report shared memory usage
  fwlib_publishSharedSize((uint32_t)(pSharedTemp - pShared) << 2);

  // print application-specific register set (in shared mem)
  pSharedApp = (uint32_t *)(pShared + (FBAS_SHARED_SET_GID >> 2));
  DBPRINT2("fbas%d: SHARED_SET_NODETYPE 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_SET_NODETYPE >> 2)));
  DBPRINT2("fbas%d: SHARED_GET_NODETYPE 0x%08x\n", nodeType, (pSharedApp + (FBAS_SHARED_GET_NODETYPE >> 2)));

  pSharedTs = (uint64_t *)(pSharedApp + (FBAS_SHARED_GET_TS1 >> 2));
  DBPRINT2("fbas%d: SHARED_GET_TS1 0x%08x\n", nodeType, pSharedTs);
} // initSharedMem


// clears all statistics
void extern_clearDiag()
{
  // ... insert code here
} // clearDiag

// entry action configured state
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // disable input gate
  fwlib_ioCtrlSetGate(0, 2);

  // configure Etherbone master (src MAC and IP are set by host, i.e. by eb-console or BOOTP)
  if ((status = fwlib_ebmInit(TIM_2000_MS, BROADCAST_MAC, BROADCAST_IP, EBM_NOREPLY)) != COMMON_STATUS_OK) {
    DBPRINT1("fbas%d: ERROR - init of EB master failed! %u\n", nodeType, (unsigned int)status); // IP unset
  }

  fwlib_publishNICData(); // NIC data (MAC, IP) are assigned to global variables (pSharedIp, pSharedMacHi/Lo)

  return status;
} // entryActionConfigured


// entry action state 'op ready'
uint32_t extern_entryActionOperation()
{
  uint32_t status = COMMON_STATUS_OK;

  //... insert code here

  return status;
} // entryActionOperation

// exit action state 'op ready'
uint32_t extern_exitActionOperation(){
  uint32_t status = COMMON_STATUS_OK;

  //... insert code here

  return status;
} // exitActionOperation


// command handler, handles commands specific for this project
void cmdHandler(uint32_t *reqState, uint32_t cmd)
{
  uint32_t u32val;
  uint8_t u8val;
  // check, if the command is valid and request state change
  if (cmd) {                             // check, if cmd is valid
    cntCmd++;
    switch (cmd) {                       // do action according to command
      case FBAS_CMD_SET_NODETYPE:
        u32val = *(pSharedApp + (FBAS_SHARED_SET_NODETYPE >> 2));
        if (u32val < FBAS_NODE_UNDEF) {
          nodeType = u32val;
          *(pSharedApp + (FBAS_SHARED_GET_NODETYPE >> 2)) = nodeType;
          DBPRINT2("fbas%d: node type %x\n", nodeType, nodeType);
        } else {
          DBPRINT2("fbas%d: invalid node type %x\n", nodeType, u32val);
        }
        break;
      case FBAS_CMD_SET_LVDS_OE:
        setIoOe(IO_CFG_CHANNEL_LVDS, 0);  // enable output for the IO1 port
        break;
      case FBAS_CMD_GET_LVDS_OE:
        u32val = getIoOe(IO_CFG_CHANNEL_LVDS);
        if (1) {
          DBPRINT2("fbas%d: GPIO OE %x\n", nodeType, u32val);
        }
        break;
      case FBAS_CMD_TOGGLE_LVDS:
        u8val = cntCmd & 0x01;
        u32val = 0;
        driveIo(IO_CFG_CHANNEL_LVDS, u32val, u8val);
        DBPRINT2("fbas%d: IO%d=%x\n", nodeType, u32val+1, u8val);
        break;
      case FBAS_CMD_EN_MPS_FWD:
        mpsTask |= TSK_TX_MPS_FLAGS;  // enable transmission of the MPS flags
        mpsTask |= TSK_TX_MPS_EVENTS; // enable transmission of the MPS events
        mpsTask |= TSK_TTL_MPS_FLAGS; // enable lifetime monitoring of the MPS flags
        DBPRINT2("fbas%d: enabled MPS %x\n", nodeType, mpsTask);
        break;
      case FBAS_CMD_DIS_MPS_FWD:
        mpsTask &= ~TSK_TX_MPS_FLAGS;  // disable transmission of the MPS flags
        mpsTask &= ~TSK_TX_MPS_EVENTS; // disable transmission of the MPS events
        mpsTask &= ~TSK_TTL_MPS_FLAGS; // disable lifetime monitoring of the MPS flags
        DBPRINT2("fbas%d: disabled MPS %x\n", nodeType, mpsTask);
        break;
      default:
        DBPRINT2("fbas%d: received unknown command '0x%08x'\n", nodeType, cmd);
        break;
    } // switch
  } // if command
} // cmdHandler


// do action state 'op ready' - this is the main code of this FW
uint32_t doActionOperation(uint32_t* pMpsTask,          // MPS-relevant tasks
                           mpsTimParam_t* pBufMpsFlag,  // pointer to MPS flags buffer
                           timedItr_t* pRdItr,          // iterator used to read MPS flags buffer
                           uint32_t actStatus)          // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t nSeconds = 15;                                     // time period in secondes
  uint32_t nUSeconds = 100 * COMMON_ECATIMEOUT;               // time period in microseconds
  uint32_t action;                                            // ECA action tag
  mpsTimParam_t* buf = pBufMpsFlag;                           // pointer to MPS flags buffer

  status = actStatus;

  // action driven by ECA event
  action = handleEcaEvent(nUSeconds, pMpsTask, pRdItr, &buf);   // handle ECA event

  switch (nodeType) {

    case FBAS_NODE_TX:
      // transmit MPS flags (flags are sent in specified period, but events immediately)
      if (*pMpsTask & TSK_TX_MPS_FLAGS)
        sendMpsFlag(pRdItr);
      else
        wrConsolePeriodic(nSeconds);   // periodic debug (level 3) output at console
      break;

    case FBAS_NODE_RX:
      if (*pMpsTask & TSK_TTL_MPS_FLAGS) {
        // monitor lifetime of MPS flags periodically and handle expired MPS flag
        buf = expireMpsFlag(pRdItr);
        if (buf)
          driveEffLogOut(buf);
      }
      break;

    default:
      break;
  }

  return status;
} // doActionOperation

// get MAC/IP address of the Endpoint WB device
uint32_t getEndpointInfo()
{
  uint32_t status;

  status = fwlib_doActionS0();                  // find addresses of common used WB devices
  if (status != COMMON_STATUS_OK) return status;

  status = extern_entryActionConfigured();      // get NIC data
  if (status != COMMON_STATUS_OK) return status;

  uint32_t octet0 = 0x000000ff;
  uint32_t octet1 = octet0 << 8;
  uint32_t octet2 = octet0 << 16;
  uint32_t octet3 = octet0 << 24;

  DBPRINT2("fbas%d: MAC=%02x:%02x:%02x:%02x:%02x:%02x, IP=%d.%d.%d.%d\n", nodeType,
      (*pSharedMacHi & octet1) >> 8, (*pSharedMacHi & octet0),
      (*pSharedMacLo & octet3) >> 24,(*pSharedMacLo & octet2) >> 16,
      (*pSharedMacLo & octet1) >> 8, (*pSharedMacLo & octet0),
      (*pSharedIp & octet3) >> 24,(*pSharedIp & octet2) >> 16,
      (*pSharedIp & octet1) >> 8, (*pSharedIp & octet0));
  return status;
}

// init of the MPS protocol data
void initMpsData()
{
  mpsEventData.evtId = fwlib_buildEvtidV1(FBAS_TM_GID, FBAS_TM_EVTNO,
      FBAS_TM_FLAGS, FBAS_TM_SID, FBAS_TM_BPID, FBAS_TM_RES);
  mpsEventData.mac = *pSharedMacHi;
  mpsEventData.mac <<= 32;
  mpsEventData.mac |= *pSharedMacLo;
  DBPRINT2("fbas%d: MPS protocol (evtId = %llu, mac = %llu)\n", nodeType,
      mpsEventData.evtId, mpsEventData.mac);

  mpsTask = 0;

  mpsTimMsgFlagId = fwlib_buildEvtidV1(FBAS_FLG_GID, FBAS_FLG_EVTNO,
      FBAS_FLG_FLAGS, FBAS_FLG_SID, FBAS_FLG_BPID, FBAS_FLG_RES);
  mpsTimMsgEvntId = fwlib_buildEvtidV1(FBAS_EVT_GID, FBAS_EVT_EVTNO,
      FBAS_EVT_FLAGS, FBAS_EVT_SID, FBAS_EVT_BPID, FBAS_EVT_RES);

  // initialize MPS flags
  for (int i = 0; i < N_MPS_CHANNELS; ++i) {
    bufMpsFlag[i].prot.flag  = MPS_FLAG_TEST;
    bufMpsFlag[i].prot.grpId = 1;
    bufMpsFlag[i].prot.evtId = i;
    bufMpsFlag[i].prot.ttl = -1;
  }

  // initialize the read iterator for MPS flags
  initItr(&rdItr, N_MPS_CHANNELS, 0, 30);
}

// set IO output enable
uint32_t setIoOe(uint32_t channel, uint32_t idx)
{
  uint32_t reg = 0;
  if (channel == IO_CFG_CHANNEL_GPIO) // GPIO channel
    reg = IO_GPIO_OE_SETLOW;
  if (channel == IO_CFG_CHANNEL_LVDS) // LVDS channel
    reg = IO_LVDS_OE_SETLOW;

  if (reg)
    *(pIOCtrl + (reg >> 2)) = (1 << idx);
}

// get IO output enable
uint32_t getIoOe(uint32_t channel)
{
  uint32_t reg = 0;
  if (channel == IO_CFG_CHANNEL_GPIO) // GPIO channel
    reg = IO_GPIO_OE_SETLOW;
  if (channel == IO_CFG_CHANNEL_LVDS) // LVDS channel
    reg = IO_LVDS_OE_SETLOW;

  if (reg)
    return *(pIOCtrl + (reg >> 2));
}

// toggle IO output
void driveIo(uint32_t channel, uint32_t idx, uint8_t value)
{
  uint32_t reg = 0;
  uint32_t outVal = 0;

  if (value == MPS_SIGNAL_INVALID)
    return;

  if (channel == IO_CFG_CHANNEL_GPIO) { // GPIO channel
    reg = IO_GPIO_SET_OUTBEGIN;
    if (value)
      outVal = 0x01;
  }
  if (channel == IO_CFG_CHANNEL_LVDS) { // LVDS channel
    reg = IO_LVDS_SET_OUTBEGIN;
    if (value)
      outVal = 0xff;
  }

  if (reg)
    *(pIOCtrl + (reg >> 2) + idx) = outVal;
}

// init last system time
void initLast()
{
  tsLast = getSysTime();
}

void initAppData()
{
  initLast();             // init the last timestamp of the system time
  getEndpointInfo();      // get MAC/IP address of the Endpoint WB device
  initMpsData();          // init the MPS protocol data
  cntMpsSignal = 0;

  DBPRINT2("fbas%d: pIOCtrl=%08x, pECAQ=%08x\n", nodeType, pIOCtrl, pECAQ);
  setIoOe(IO_CFG_CHANNEL_LVDS, 0);  // enable output for the IO1 port
}

// initialize iterator
void initItr(timedItr_t* itr, uint8_t total, uint64_t now, uint32_t freq)
{
  itr->idx = 0;
  itr->total = total;
  itr->last = now;
  itr->period = WR_TIM_1000_MS; // 1 second
  if (freq && itr->total) {
    itr->period /=(freq * itr->total); // for 30Hz it's 33312 us (30.0192 Hz)

    //itr->period /= 1000ULL; // granularity in 1 us
    //itr->period *= 1000ULL;
  }
}

// update iterator
void updateItr(timedItr_t* itr, uint64_t now)
{
  itr->last = now;

  ++itr->idx;
  if (itr->idx >= itr->total)
    itr->idx = 0;
}

// send MPS flags at specified period
void sendMpsFlag(timedItr_t* itr)
{
  uint64_t now = getSysTime();
  uint64_t deadline = itr->last + itr->period;
  if (!itr->last)
    deadline = now;       // initial transmission

  // send next MPS flag if deadline is over
  if (deadline <= now) {

    // send MPS flag with current timestamp, which varies around deadline
    fwlib_ebmWriteTM(now, mpsTimMsgFlagId, bufMpsFlag[itr->idx].param, 1);

    // update iterator with deadline
    updateItr(itr, deadline);
  }
}

/**
 ** \brief send MPS event
 **
 ** Upon flag change to NOK, there shall be 2 extra events within 50 us. [MPS_FS_530]
 ** If the read iterator is blocked by new cycle, then do not send any MPS event. [MPS_FS_630]
 **
 ** \param itr pointer to read iterator for MPS flag
 ** \param buf pointer to MPS event buffer
 ** \param n   number of extra events
 **
 **/
void sendMpsEvent(timedItr_t* itr, mpsTimParam_t* buf, uint8_t n)
{
  uint64_t now = getSysTime();

  if (itr->last >= now) // blocked by new cycle
    return;

  // send specified MPS event
  fwlib_ebmWriteTM(now, mpsTimMsgEvntId, buf->param, 1);

  // NOK flag shall be sent as extra events
  if (buf->prot.flag == MPS_FLAG_NOK) {
    for (uint8_t i = 0; i < n; ++i) {
      now = getSysTime();
      fwlib_ebmWriteTM(now, mpsTimMsgEvntId, buf->param, 1);
    }
  }
}

/**
 ** \brief reset MPS flag
 **
 ** It is used to reset the CMOS input virtually to high voltage in TX [MPS_FS_620] or
 ** reset effective logic input to HIGH bit in RX [MPS_FS_630].
 **
 ** \param buf pointer to MPS flag buffer
 **
 **/
void resetMpsFlag(size_t len, mpsTimParam_t* buf) {

  uint8_t flag = MPS_FLAG_OK;

  for (size_t i = 0; i < len; ++i) {
    (buf + i)->prot.pending = (buf + i)->prot.flag ^ flag;
    (buf + i)->prot.flag  = flag;
    (buf + i)->prot.ttl = 10; // time-out for 10 iterations
  }
}

/**
 ** \brief clear latched error
 **
 ** Errors caused by lost messages or NOK flag are being latched until new cycle.
 ** [MPS_FS_600]
 **
 ** \param buf pointer to MPS flag buffer
 **
 **/
void clearError(size_t len, mpsTimParam_t* buf) {

  for (size_t i = 0; i < len; ++i) {
    driveEffLogOut(buf + i);
  }
}

/**
 ** \brief update MPS flag with recieved MPS event
 **
 ** \param buf pointer to MPS flags buffer
 ** \param evt raw event data (bits 31-24 = flag, 23-16 = grpId, 15-0 = evtId)
 **
 ** \ret ptr pointer to the updated MPS flag
 **/
mpsTimParam_t* updateMpsFlag(mpsTimParam_t* buf, uint64_t evt)
{
  // evaluate MPS channel and its flag
  uint8_t flag = evt >> 24;
  uint8_t grpId = evt >> 16;
  uint16_t evtId = evt & 0xFFFF;

  if (evtId >= N_MPS_CHANNELS)
    return 0;

  // update MPS flag
  buf += evtId;
  buf->prot.flag = flag;
  return buf;
}

/**
 ** \brief store recieved MPS flag
 **
 ** \param buf pointer to MPS flags buffer
 ** \param raw raw protocol data (bits 63-56 = flag, 57-48 = grpId, 47-32 = evtId)
 **
 ** \ret ptr pointer to the stored MPS flag
 **/
mpsTimParam_t* storeMpsFlag(mpsTimParam_t* buf, uint64_t raw)
{
  // evaluate MPS channel and its flag
  uint8_t flag = raw >> 56;
  uint8_t grpId = raw >> 48;
  uint16_t evtId = raw >> 32;

  if (evtId >= N_MPS_CHANNELS)
    return 0;

  // store MPS flag
  buf += evtId;
  buf->prot.pending = buf->prot.flag ^ flag;
  buf->prot.flag = flag;
  buf->prot.ttl = 10; // die after 10 iterations
  return buf;
}

/**
 ** \brief drive the effective logic output [MPS_FS_640]
 **
 ** Drive internal signal based on MPS flag:
 ** - high if MPS flag is OK
 ** - low if MPS flag is NOK or TEST
 **
 ** Generate error (internal signal), if lifetime of MPS flag is expired.
 **
 ** \param buf pointer to MPS flag
 **
 ** \ret status
 **/
uint32_t driveEffLogOut(mpsTimParam_t* buf)
{
  uint8_t ioVal = MPS_SIGNAL_INVALID;

  // handle MPS flag if it's changed or expired
  if (buf->prot.pending) {
    buf->prot.pending = 0;
    DBPRINT3("pend: %x %x %x\n", buf->prot.grpId, buf->prot.evtId, buf->prot.flag);
    if (buf->prot.flag == MPS_FLAG_OK)
      ioVal = MPS_SIGNAL_HIGH;
    else
      ioVal = MPS_SIGNAL_LOW;
  } else if (!buf->prot.ttl) {
    ioVal = MPS_SIGNAL_LOW;
    DBPRINT3("ttl: %x %x %x\n", buf->prot.grpId, buf->prot.evtId, buf->prot.flag);
  }

  if (ioVal != MPS_SIGNAL_INVALID)
    driveIo(IO_CFG_CHANNEL_LVDS, 0, ioVal); // drive the IO1 port

  return COMMON_STATUS_OK;
}

/**
 ** \brief alter lifetime of MPS flags [MPS_FS_600]
 **
 ** \param itr iterator used to access MPS flags in pre-defined period
 **
 ** \ret ptr pointer to expired MPS flag
 **/
mpsTimParam_t* expireMpsFlag(timedItr_t* itr)
{
  uint64_t now = getSysTime();
  uint64_t deadline = itr->last + itr->period;

  if (!itr->last)
    deadline = now;       // initial check

  // check lifetime of next MPS flag
  if (deadline <= now) {

    // decrement TTL counter
    if (bufMpsFlag[itr->idx].prot.ttl) {
      --bufMpsFlag[itr->idx].prot.ttl;

      if (!bufMpsFlag[itr->idx].prot.ttl) {
        bufMpsFlag[itr->idx].prot.flag = MPS_FLAG_NOK;
        return &bufMpsFlag[itr->idx];  // expired MPS flag
      }
    }

    // update iterator with deadline
    updateItr(itr, deadline);
  }

  return 0;
}

/**
 ** \brief set operation mode
 **
 ** \param mode raw data with operation mode
 **
 **/
void setOpMode(uint64_t mode) {
  if (mode)
    opMode = FBAS_OPMODE_TEST;
  else
    opMode = FBAS_OPMODE_DEF;
}

void qualifyInput(size_t len, mpsTimParam_t* buf) {
}

void testOutput(size_t len, mpsTimParam_t* buf) {
}

// send MPS protocol
void sendMpsProtocol(uint64_t deadline, uint8_t mpsFlag)
{
  uint64_t evtParam = mpsFlag;
  evtParam <<= 56;
  evtParam |= mpsEventData.mac;

  // ignore the ahead interval of 500 us by setting 'flagForceLate'
  fwlib_ebmWriteTM(getSysTime(), mpsEventData.evtId, evtParam, 1);
}

/**
 ** \brief handle pending ECA event
 **
 ** On FBAS_GEN_EVT event the buffer for MPS flag is updated and \head returns
 ** pointer to it. Otherwise, \head is returned with null value.
 **
 ** On FBAS_WR_EVT or FBAS_WR_FLG event the effective logic output is driven.
 **
 ** \param usTimeout maximum interval in microseconds to poll ECA
 ** \param mpsTask   pointer to MPS-relevant task flag
 ** \param itr       pointer to the read iterator for MPS flags
 ** \param head      pointer to pointer of the MPS flags buffer
 **
 ** \return ECA action tag
 **/
uint32_t handleEcaEvent(uint32_t usTimeout, uint32_t* mpsTask, timedItr_t* itr, mpsTimParam_t** head)
{
  uint32_t nextAction;    // action triggered by received ECA event
  uint64_t ecaDeadline;   // deadline of received ECA event
  uint64_t ecaEvtId;      // ID of received ECA event
  uint64_t ecaParam;      // parameter value in received ECA event
  uint32_t ecaTef;        // TEF value in received ECA event
  uint32_t flagIsLate;    // flag indicates that received ECA event is 'late'
  uint64_t now;           // actual timestamp of the system time
  int64_t  poll;          // elapsed time to poll a pending ECA event

  nextAction = fwlib_wait4ECAEvent(usTimeout, &ecaDeadline, &ecaEvtId, &ecaParam, &ecaTef, &flagIsLate);

  if (nextAction) {
    now = getSysTime();
    storeSystemTime(FBAS_SHARED_GET_TS5, now);

    switch (nextAction) {
      case FBAS_AUX_NEWCYCLE:
        if (nodeType == FBAS_NODE_TX) { // it takes 1848/6328 ns for 2/32 MPS channels
          // reset MPS flags
          resetMpsFlag(N_MPS_CHANNELS, *head);

          // init the read iterator for MPS flags, so that iteration is delayed for 52 ms [MPS_FS_630]
          now += WR_TIM_52_MS;
          initItr(itr, N_MPS_CHANNELS, now, 1);

        } else if (nodeType == FBAS_NODE_RX) { // it takes 2480/31048 ns for 2/32 MPS channels
          // reset effective logic input to HIGH bit (delay for 52 ms) [MPS_FS_630]
          resetMpsFlag(N_MPS_CHANNELS, *head);
          // clear latched errors [MPS_FS_600]
          clearError(N_MPS_CHANNELS, *head);
        }
        now = getSysTime();
        DBPRINT2("%lli\n", measureElapsedTime(FBAS_SHARED_GET_TS5, now));
        break;

      case FBAS_AUX_OPMODE:
        setOpMode(ecaParam);

        if (nodeType == FBAS_NODE_TX) { // TODO: measure elapsed time
          // each gate shall be fully qualifed [MPS_FS_740]
          // no variable besides deliberate exceptions shall be unmasked [MPS_FS_740]
          // flag change suppressed 0,5 us after test begin or end [MPS_FS_550]
          qualifyInput(N_MPS_CHANNELS, *head);

        } else if (nodeType == FBAS_NODE_RX) {
          // invert output
          testOutput(N_MPS_CHANNELS, *head);
        }
        now = getSysTime();
        DBPRINT2("%lli\n", measureElapsedTime(FBAS_SHARED_GET_TS5, now));
        break;

      case FBAS_GEN_EVT:
        if (nodeType == FBAS_NODE_TX) {// only FBAS TX node handles the MPS events
          // update MPS flag
          *head = updateMpsFlag(*head, ecaEvtId);

          if (*head && (*mpsTask & TSK_TX_MPS_EVENTS)) {
            // send MPS event
            sendMpsEvent(itr, *head, N_EXTRA_MPS_EVENTS);
            // prepare performance measurements
            preparePerfMeasurement(now, ecaDeadline);
          }
        }
        break;
      case FBAS_TLU_EVT:
        if (nodeType == FBAS_NODE_TX) {// only FBAS TX node handles the TLU events
          // measure network delay (broadcast MPS events from TX to RX nodes) and
          // forwarding duration (from MPS event generation at TX to IO event detection at RX)
          measurePerformance(nextAction, flagIsLate, now, ecaDeadline);
        }
        break;
      case FBAS_WR_EVT:
      case FBAS_WR_FLG:
        if (nodeType == FBAS_NODE_RX) { // FBAS RX generates MPS class 2 signals

          // store and handle received MPS flag
          *head = storeMpsFlag(*head, ecaParam);
          if (*head) {
            driveEffLogOut(*head);
          }
        }
        break;
      default:
        break;
    }
  }

  if (nextAction != FBAS_GEN_EVT)
    *head = 0;

  return nextAction;
}

/**
 ** \brief store actual system time
 **
 ** \param offset offset to shared memory buffer for storing the actual system time
 ** \param now    actual system time
 **
 **/
void storeSystemTime(uint32_t offset, uint64_t now) {
  uint64_t* pSharedTs = (uint64_t *)(pSharedApp + (offset >> 2));

  *pSharedTs = now;
}

/**
 ** \brief measure elapsed time
 **
 ** \param offset offset to shared memory buffer with timestamp
 ** \param now    actual system time
 **
 ** \ret time     elapsed time in nanosecond since last timestamp
 **
 **/
int64_t measureElapsedTime(uint32_t offset, uint64_t now) {
  uint64_t* pSharedTs = (uint64_t *)(pSharedApp + (offset >> 2));

  return (now - *pSharedTs);
}

/**
 ** \brief prepare network performance measurement
 **
 ** Store actual system time of MPS event transmission and
 ** timestamp of MPS event detection for later performance measurement.
 **
 ** Timestamp of MPS event detection is pointed by pSharedApp + FBAS_SHARED_GET_TS1.
 ** Timestamp of MPS event transmission is pointed by pSharedApp + FBAS_SHARED_GET_TS2.
 **
 ** \param now       actual system time
 ** \param deadline  timestamp of ECA event
 **
 **/
void preparePerfMeasurement(uint64_t now, uint64_t deadline) {
  uint64_t *pSharedTs = (uint64_t *)(pSharedApp + (FBAS_SHARED_GET_TS1 >> 2));
  *pSharedTs = deadline;
  *(pSharedTs + (FBAS_SHARED_GET_TS2 >> 2)) = now;
}

/**
 ** \brief measure network performance
 **
 ** Network delay to transmit MPS events (broadcast frame from TX to RX) and
 ** time duration to forward MPS signals (from MPS event generation at TX
 ** to IO event detection at RX) are measured and output as debug msg.
 **
 ** Timestamp of MPS event detection is pointed by pSharedApp + FBAS_SHARED_GET_TS1.
 ** Timestamp of MPS event transmission is pointed by pSharedApp + FBAS_SHARED_GET_TS2.
 **
 ** \param tag       ECA condition tag
 ** \param flag      ECA late event flag
 ** \param now       actual system time
 ** \param deadline  timestamp of ECA event
 **
 **/
void measurePerformance(uint32_t tag, uint32_t flag, uint64_t now, uint64_t deadline) {
  uint64_t *pSharedTs = (uint64_t *)(pSharedApp + (FBAS_SHARED_GET_TS1 >> 2));
  uint64_t tmp64 = *(pSharedTs + (FBAS_SHARED_GET_TS2 >> 2));

  int64_t delayNw = deadline - tmp64;     // network delay
  int64_t durationFwd = now - *pSharedTs; // forward duration
  DBPRINT2("fbas%d: dly=%lli, fwd=%lli\n", nodeType, delayNw, durationFwd);

  int64_t poll = now - deadline;   // duration to detect TLU (IO) event (RX->TX)
  DBPRINT3("fbas%d: TLU evt (tag %x, flag %x, ts %llu, now %llu, poll %lli)\n",
      nodeType, tag, flag, deadline, now, poll);

  poll = tmp64 - *pSharedTs;       // duration to send MPS event (TX->RX)
  DBPRINT3("fbas%d: generator evt timestamps (detect %llu, send %llu, poll %lli)\n",
      nodeType, *pSharedTs, tmp64, poll);
}

// write a debug text to the WR console in given period (seconds)
void wrConsolePeriodic(uint32_t seconds)
{
  uint64_t period = seconds * WR_TIM_1000_MS; // period in system time
  uint64_t soon = tsLast + period;            // next time point for the action
  uint64_t now = getSysTime();                // get the current time

  if (now >= soon) {                          // if the given period is over, then proceed
    //sendMpsProtocol((uint8_t)cntMpsSignal);
    DBPRINT3("fbas%d: now %llu, elap %lli\n", nodeType, now, now - tsLast);
    tsLast = now;
  }
}

int main(void) {
  uint32_t status;                                            // (error) status
  uint32_t cmd;                                               // command via shared memory
  uint32_t actState;                                          // actual FSM state
  uint32_t pubState;                                          // value of published state
  uint32_t reqState;                                          // requested FSM state
  uint32_t *buildID;

  // init local variables
  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  buildID        = (uint32_t *)(INT_BASE_ADR + BUILDID_OFFS); // required for 'stack check'

  // init
  init();                                                              // initialize stuff for lm32
  initSharedMem();                                                     // initialize shared memory
  fwlib_init((uint32_t *)_startshared, pCpuRamExternal, SHARED_OFFS, "fbastx", FBASTX_FW_VERSION); // init common stuff
  fwlib_clearDiag();                                                   // clear common diagnostic data

  initAppData();                                                       // initialize everything specific to this application

  while (1) {
    check_stack_fwid(buildID);                                         // check for stack corruption
    fwlib_cmdHandler(&reqState, &cmd);                                 // check for common commands and possibly request state changes
    cmdHandler(&reqState, cmd);                                        // check for project relevant commands
    status = COMMON_STATUS_OK;                                         // reset status for each iteration
    status = fwlib_changeState(&actState, &reqState, status);          // handle requested state changes
    switch(actState) {                                                 // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&mpsTask, bufMpsFlag, &rdItr, status);
        if (status == COMMON_STATUS_WRBADSYNC) reqState = COMMON_STATE_ERROR;
        if (status == COMMON_STATUS_ERROR)     reqState = COMMON_STATE_ERROR;
        break;
      default :                                                        // avoid flooding WB bus with unnecessary activity
        status = fwlib_doActionState(&reqState, actState, status);     // handle do actions states
        break;
    } // switch

    // update sum status
    switch (status) {
      case COMMON_STATUS_OK :                                                     // status OK
        statusArray = statusArray |  (0x1 << COMMON_STATUS_OK);                   // set OK bit
        break;
      default :                                                                   // status not OK
        if ((statusArray >> COMMON_STATUS_OK) & 0x1) fwlib_incBadStatusCnt();     // changing status from OK to 'not OK': increase 'bad status count'
        statusArray = statusArray & ~(0x1 << COMMON_STATUS_OK);                   // clear OK bit
        statusArray = statusArray |  (0x1 << status);                             // set status bit and remember other bits set
        break;
    } // switch status

    // update shared memory
    if ((pubState == COMMON_STATE_OPREADY) && (actState  != COMMON_STATE_OPREADY)) fwlib_incBadStateCnt();
    fwlib_publishStatusArray(statusArray);
    pubState             = actState;
    fwlib_publishState(pubState);
    // ... insert code here
  } // while

  return(1);
} // main

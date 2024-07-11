/********************************************************************************************
 *  wr-f50.c
 *
 *  created : 2024
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 10-Jul-2024
 *
 *  firmware required for the 50 Hz mains -> WR gateway
 *  
 *  This firmware tries to lock the Injector Data Master to the 50 Hz mains frequency
 *  * receives a trigger signal from the 50 Hz mains,
 *  * receives a 50 Hz 'cycle start' signal via the WR network,
 *  * compares both signals,
 *  * calculates the the new set-value of the period (~ 20ms) for the Data Master
 *  * broadcasts this information to the timing network
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
 * Last update: 15-April-2019
 ********************************************************************************************/
#define WRF50_FW_VERSION      0x000011                                  // make this consistent with makefile

// standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// includes specific for bel_projects
#include "dbg.h"                                                        // debug outputs
#include <stack.h>                                                      // stack check
#include "pp-printf.h"                                                  // print
#include "mini_sdb.h"                                                   // sdb stuff
#include "ebm.h"                                                        // EB master
#include "aux.h"                                                        // cpu and IRQ
#include "uart.h"                                                       // WR console

// includes for this project 
#include <common-defs.h>                                                // common defs for firmware
#include <common-fwlib.h>                                               // common routines for firmware
#include <wr-f50.h>                                                     // specific defs for wr-f50
#include <wrf50_shared_mmap.h>                                          // autogenerated upon building firmware

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

// global variables 
volatile uint32_t *pShared;                // pointer to begin of shared memory region
volatile uint32_t *pSharedSetF50Offset;    // pointer to a "user defined" u32 register; here: offset to TLU signal         
volatile uint32_t *pSharedSetMode;         // pointer to a "user defined" u32 register; here: mode of 50 Hz synchronization
volatile uint32_t *pSharedGetTMainsAct;    // pointer to a "user defined" u32 register; here: period of mains cycle [ns], actual value
volatile uint32_t *pSharedGetTDMAct;       // pointer to a "user defined" u32 register; here: period of Data Master cycle [ns], actual value
volatile uint32_t *pSharedGetTDMSet;       // pointer to a "user defined" u32 register; here: period of Data Master cycle [ns], actual value
volatile uint32_t *pSharedGetOffsDMAct;    // pointer to a "user defined" u32 register; here: offset of cycle start: t_DM_act - t_mains_act; actual value
volatile uint32_t *pSharedGetOffsDMMin;    // pointer to a "user defined" u32 register; here: offset of cycle start: t_DM_act - t_mains_act; min value
volatile uint32_t *pSharedGetOffsDMMax;    // pointer to a "user defined" u32 register; here: offset of cycle start: t_DM_act - t_mains_act; max value
volatile uint32_t *pSharedGetDTDMAct;      // pointer to a "user defined" u32 register; here: change of period: DM_act - DM_previous; actual value
volatile uint32_t *pSharedGetDTDMMin;      // pointer to a "user defined" u32 register; here: change of period: DM_act - DM_previous; min value   
volatile uint32_t *pSharedGetDTDMMax;      // pointer to a "user defined" u32 register; here: change of period: DM_act - DM_previous; max value    
volatile uint32_t *pSharedGetOffsMainsAct; // pointer to a "user defined" u32 register; here: offset of cycle start: t_mains_act - t_mains_predict; actual value
volatile uint32_t *pSharedGetOffsMainsMin; // pointer to a "user defined" u32 register; here: offset of cycle start: t_mains_act - t_mains_predict; min value        
volatile uint32_t *pSharedGetOffsMainsMax; // pointer to a "user defined" u32 register; here: offset of cycle start: t_mains_act - t_mains_predict; max value        
volatile uint32_t *pSharedGetLockState;    // pointer to a "user defined" u32 register; here: lock state; how DM is locked to mains                              
volatile uint32_t *pSharedGetLockDateHi;   // pointer to a "user defined" u32 register; here: time when lock has been achieve [ns], high bits                    
volatile uint32_t *pSharedGetLockDateLo;   // pointer to a "user defined" u32 register; here: time when lock has been achieve [ns], low bits                     
volatile uint32_t *pSharedGetNLocked;      // pointer to a "user defined" u32 register; here: counts how many locks have been achieved                           
volatile uint32_t *pSharedGetNCycles;      // pointer to a "user defined" u32 register; here: number of UNILAC cycles
volatile uint32_t *pSharedGetNSent;        // pointer to a "user defined" u32 register; here: number of messages sent to the Data Master (as broadcast)

uint32_t *cpuRamExternal;                  // external address (seen from host bridge) of this CPU's RAM

// set and get values
uint32_t setF50Offset;  
uint32_t setMode;        
uint32_t getTMainsAct;   
uint32_t getTDMAct;      
uint32_t getTDMSet;      
int32_t  getOffsDMAct;   
int32_t  getOffsDMMin;   
int32_t  getOffsDMMax;   
int32_t  getDTDMAct;
int32_t  getDTDMMin;
int32_t  getDTDMMax;
int32_t  getOffsMainsAct;
int32_t  getOffsMainsMin;
int32_t  getOffsMainsMax;
uint32_t getLockState;   
uint64_t getLockDate;  
uint32_t getNLocked;     
uint32_t getNCycles;
uint32_t getNSent;
uint32_t getNEvtsLate;
uint32_t getOffsDone;
int32_t  getComLatency;

int32_t  maxComLatency;
uint32_t maxOffsDone;

uint64_t statusArray;                      // all status infos are ORed bit-wise into statusArray, statusArray is then published
uint32_t nEvtsLate;                        // # of late messages

// constants (as variables to have a defined type)
uint64_t  one_us_ns = 1000;

// debug 
uint64_t t1, t2;
int32_t  tmp1;

// important local variables
uint64_t stampsF50[WRF50_N_STAMPS];        // previous timestamps of 50 Hz signal   (most recent is n = WR50_N_STAMPS)
uint64_t stampsDM[WRF50_N_STAMPS];         // previous timestamps of DM cycle start (most recent is n = WR50_N_STAMPS)

uint64_t t0F50;                            // timestamp of actual cycle,                 50 Hz mains
uint64_t t1F50;                            // timestamp of actual cycle + 1 (predicted), 50 Hz mains
uint64_t t2F50;                            // timestamp of actual cycle + 2 (predicted), 50 Hz mains
uint64_t t0DM;                             // timestamp of actual cycle,                 Data Master
uint64_t t1DM;                             // timestamp of actual cycle + 1 (predicted), Data Master
uint64_t t2DM;                             // timestamp of actual cycle + 2 (predicted), Data Master

int      validF50;                         // mains timestamps are valid
int      validDM;                          // Data Master timestamps are valid


void init() // typical init for lm32
{
  discoverPeriphery();        // mini-sdb ...
  uart_init_hw();             // needed by WR console   
  cpuId = getCpuIdx();
} // init


// determine address and clear shared mem
void initSharedMem(uint32_t *reqState, uint32_t *sharedSize)
{
  uint32_t idx;
  uint32_t *pSharedTemp;
  int      i; 
  const uint32_t c_Max_Rams = 10;
  sdb_location   found_sdb[c_Max_Rams];
  sdb_location   found_clu;
  
  // get pointer to shared memory
  pShared                 = (uint32_t *)_startshared;

  // get address to data
  pSharedSetF50Offset     = (uint32_t *)(pShared + (WRF50_SHARED_SET_F50OFFSET       >> 2));
  pSharedSetMode          = (uint32_t *)(pShared + (WRF50_SHARED_SET_MODE            >> 2));
  pSharedGetTMainsAct     = (uint32_t *)(pShared + (WRF50_SHARED_GET_T_MAINS_ACT     >> 2));
  pSharedGetTDMAct        = (uint32_t *)(pShared + (WRF50_SHARED_GET_T_DM_ACT        >> 2));
  pSharedGetTDMSet        = (uint32_t *)(pShared + (WRF50_SHARED_GET_T_DM_SET        >> 2));
  pSharedGetOffsDMAct     = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_DM_ACT     >> 2));
  pSharedGetOffsDMMin     = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_DM_MIN     >> 2));
  pSharedGetOffsDMMax     = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_DM_MAX     >> 2));
  pSharedGetDTDMAct       = (uint32_t *)(pShared + (WRF50_SHARED_GET_DT_DM_ACT       >> 2));
  pSharedGetDTDMMin       = (uint32_t *)(pShared + (WRF50_SHARED_GET_DT_DM_MIN       >> 2));
  pSharedGetDTDMMax       = (uint32_t *)(pShared + (WRF50_SHARED_GET_DT_DM_MAX       >> 2));
  pSharedGetOffsMainsAct  = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_MAINS_ACT  >> 2));
  pSharedGetOffsMainsMin  = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_MAINS_MIN  >> 2));
  pSharedGetOffsMainsMax  = (uint32_t *)(pShared + (WRF50_SHARED_GET_OFFS_MAINS_MAX  >> 2));
  pSharedGetLockState     = (uint32_t *)(pShared + (WRF50_SHARED_GET_LOCK_STATE      >> 2));
  pSharedGetLockDateHi    = (uint32_t *)(pShared + (WRF50_SHARED_GET_LOCK_DATE_HIGH  >> 2));
  pSharedGetLockDateLo    = (uint32_t *)(pShared + (WRF50_SHARED_GET_LOCK_DATE_LOW   >> 2));
  pSharedGetNLocked       = (uint32_t *)(pShared + (WRF50_SHARED_GET_N_LOCKED        >> 2));
  pSharedGetNCycles       = (uint32_t *)(pShared + (WRF50_SHARED_GET_N_CYCLES        >> 2));
  pSharedGetNSent         = (uint32_t *)(pShared + (WRF50_SHARED_GET_N_SENT          >> 2));

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("wr-f50: fatal error - did not find LM32-CB-CLUSTER!\n");
  } // if idx
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("wr-f50: fatal error - did not find THIS CPU!\n");
  } // if idx
  else cpuRamExternal = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  DBPRINT2("wr-f50: CPU RAM external 0x%8x, shared offset 0x%08x\n", cpuRamExternal, SHARED_OFFS);
  DBPRINT2("wr-f50: fw common shared begin   0x%08x\n", pShared);
  DBPRINT2("wr-f50: fw common shared end     0x%08x\n", pShared + (COMMON_SHARED_END >> 2));

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_END >> 2 ) + 1);
  DBPRINT2("wr-f50: fw specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (WRF50_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("wr-f50: fw specific shared end   0x%08x\n", pSharedTemp);

  *sharedSize        = (uint32_t)(pSharedTemp - pShared) << 2;

  // basic info to wr console
  DBPRINT1("\n");
  DBPRINT1("wr-f50: initSharedMem, shared size [bytes]: %d\n", *sharedSize);
  DBPRINT1("\n");
} // initSharedMem 


// clear project specific diagnostics
void extern_clearDiag()
{
  getTMainsAct    = 0x0;    
  getTDMAct       = 0x0;       
  getTDMSet       = 0x0;       
  getOffsDMAct    = 0x0;    
  getOffsDMMin    = 0x7fffffff;
  getOffsDMMax    = 0x80000000;
  getDTDMAct      = 0x00000000;
  getDTDMMin      = 0x7fffffff;
  getDTDMMax      = 0x80000000;
  getOffsMainsAct = 0x0; 
  getOffsMainsMin = 0x7fffffff;
  getOffsMainsMax = 0x80000000;
  getLockState    = WRF50_SLOCK_UNKWN;
  getLockDate     = 0x0;
  getNLocked      = 0x0;
  if (getNCycles > WRF50_N_STAMPS) getNCycles = WRF50_N_STAMPS;
  getNSent        = 0x0;
  getNEvtsLate    = 0x0;
  getOffsDone     = 0x0;
  getComLatency   = 0x0;
  maxOffsDone     = 0x0;
  maxComLatency   = 0x0;


  statusArray     = 0x0;
  nEvtsLate       = 0x0;
} // extern_clearDiag 


// entry action 'configured' state
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // configure EB master (SRC and DST MAC/IP are set from host)
  if ((status = fwlib_ebmInit(2000, 0xffffffffffff, 0xffffffff, EBM_NOREPLY)) != COMMON_STATUS_OK) {
    DBPRINT1("wr-f50: ERROR - init of EB master failed! %u\n", (unsigned int)status);
    return status;
  } 

  // get and publish NIC data
  fwlib_publishNICData(); 

  // clear diagnostic data
  fwlib_clearDiag();
  getNCycles = 0x0;
  
  // if everything is ok, we must return with COMMON_STATUS_OK
  if (status == COMMON_STATUS_OK) status = COMMON_STATUS_OK;

  return status;
} // extern_entryActionConfigured


// entry action 'operation' state
uint32_t extern_entryActionOperation()
{
  int      i;
  uint64_t tDummy;
  uint64_t eDummy;
  uint64_t pDummy;
  uint32_t fDummy;
  uint32_t flagDummy1, flagDummy2, flagDummy3, flagDummy4;
  int      enable_fifo;

  // clear diagnostics
  fwlib_clearDiag();
  getNCycles = 0;

  // flush ECA queue for lm32
  i = 0;
  while (fwlib_wait4ECAEvent(1000, &tDummy, &eDummy, &pDummy, &fDummy, &flagDummy1, &flagDummy2, &flagDummy3, &flagDummy4) !=  COMMON_ECADO_TIMEOUT) {i++;}
  DBPRINT1("wr-f50: ECA queue flushed - removed %d pending entries from ECA queue\n", i);
    
  // init get values
  *pSharedGetTMainsAct      = 0x0;   
  *pSharedGetTDMAct         = 0x0;      
  *pSharedGetTDMSet         = 0x0;      
  *pSharedGetOffsDMAct      = 0x0;   
  *pSharedGetOffsDMMin      = 0x0;   
  *pSharedGetOffsDMMax      = 0x0;
  *pSharedGetDTDMAct        = 0x0;
  *pSharedGetDTDMMin        = 0x0;
  *pSharedGetDTDMMax        = 0x0;
  *pSharedGetOffsMainsAct   = 0x0;
  *pSharedGetOffsMainsMin   = 0x0;
  *pSharedGetOffsMainsMax   = 0x0;
  *pSharedGetLockState      = 0x0;   
  *pSharedGetLockDateHi     = 0x0;  
  *pSharedGetLockDateLo     = 0x0;  
  *pSharedGetNLocked        = 0x0;
  *pSharedGetNCycles        = 0x0;
  *pSharedGetNSent          = 0x0;

  // init set values
  setF50Offset              = *pSharedSetF50Offset;
  if (setF50Offset > WRF50_CYCLELEN_MIN) setF50Offset = WRF50_CYCLELEN_MIN;
  setMode                   = *pSharedSetMode;

  t0F50                     = 0;
  t1F50                     = 0;
  t2F50                     = 0;
  t0DM                      = 0;
  t1DM                      = 0;
  t2DM                      = 0;

  validF50                  = 0;
  validDM                   = 0;

  return COMMON_STATUS_OK;
} // extern_entryActionOperation


uint32_t extern_exitActionOperation()
{ 
  return COMMON_STATUS_OK;
} // extern_exitActionOperation


// insert (and shift) tstamps
// - pop oldest timeststamp
// - insert newest timetamp at index WRF50_N_STAMPS - 1
// - calculate actual cycle length
void updateStamps(uint64_t newStamp,               // new timestamp
                  uint64_t stamps[],               // all timestamps
                  int      *flagValid              // flags (or not) return values are valid
                  )
{
  int i;
  uint64_t cyclen;                                 // actual cycle length
  
  // timestamps: pop oldest, shift (not very clever, shame on me) and add new
  for (i=1; i<WRF50_N_STAMPS; i++) stamps[i-1] = stamps[i];
  stamps[WRF50_N_STAMPS-1] = newStamp;

  // calculate average cycle length
  cyclen    = 0;
  for (i=1; i<WRF50_N_STAMPS; i++)
    cyclen  = cyclen + stamps[i] - stamps[i-1];
  cyclen    = cyclen / (WRF50_N_STAMPS - 1);
  // check limits
  if ((cyclen <= (uint64_t)WRF50_CYCLELEN_MAX) && (cyclen >= (uint64_t)WRF50_CYCLELEN_MIN)) *flagValid = 1;
  else                                                                                      *flagValid = 0;

} // updateStamps


// calculates next timestamp
uint64_t nextStamp(uint64_t stamps[],              // all timestamps
                   uint64_t *t1,                   // time stamp of actual cycle + 1 (predicted) 
                   uint64_t *t2                    // time stamp of actual cycle + 2 (predicted) 
                   )
{
  // calc timestamp via linear regression t = a + b*x
  // a: value of oldest timestamp
  // b: slope
  // linear regression with 11 timestamps takes about 35us
  // important: the linear regression uses integer instead of floating point values
  //            thus, the number of data of data points must be an odd number
  //            to avoid rounding errors

  
  uint64_t   a;
  uint64_t   b;
  uint64_t   nxtStamp;
  uint64_t   actStamp;

  // linear regression 
  // x: index
  // y: timestamp
  int        i;
  int64_t    num;                                  // numerator 
  int64_t    denom;                                // denominator
  int64_t    x0;                                   // rescale offset to avoid overflow
  int64_t    x_mean;                               // mean of x values
  int64_t    y_mean;                               // mean of y values
  int64_t    n;                                    // number of measurements

  // basic check
  if (WRF50_N_STAMPS < 2) return getSysTime() + WRF50_CYCLELEN_MIN;

  x0        = stamps[0] - 1000000;
  x_mean    = 0;
  y_mean    = 0;
  num       = 0;
  denom     = 0;
  n         = WRF50_N_STAMPS;

  // calc mean values
  for (i=0; i<n; i++) {
    x_mean += i;
    y_mean += (stamps[i] - x0);
  } // for i
  x_mean = x_mean / n;
  y_mean = y_mean / n;

  //tmp = (uint32_t)x_mean;
  //pp_printf("wr-f50: x_mean %d\n", tmp);
  //tmp = (uint32_t)y_mean;
  //pp_printf("wr-f50: y_mean %d\n", tmp);

  // linear regression through coordinate system origin; use oldest timestamp as origin
  for (i=0; i<n; i++) {
    num    += i * (stamps[i] - x0);
    denom  += i * i;
  } // for i
  num    = num   - n * x_mean * y_mean;
  denom  = denom - n * x_mean * x_mean;

  b      = num / denom;
  a      = y_mean - b * x_mean;

  //tmp    = (uint32_t)b;
  //pp_printf("wr-f50: b %d\n", tmp);
  //tmp    = (uint32_t)a;
  //pp_printf("wr-f50: a %d\n", tmp);

  // timestamp of actual cycle + 1
  nxtStamp = x0 + a + b * (n - 1 + 1);

  // respect hard limits
  actStamp = stamps[WRF50_N_STAMPS - 1];
  if ((nxtStamp - actStamp) < WRF50_CYCLELEN_MIN) nxtStamp = actStamp + WRF50_CYCLELEN_MIN;
  if ((nxtStamp - actStamp) > WRF50_CYCLELEN_MAX) nxtStamp = actStamp + WRF50_CYCLELEN_MAX;

  *t1 = nxtStamp;

  // timestamp of actual cycle + 2
  nxtStamp = x0 + a + b * (n - 1 + 2);

  // respect hard limits
  actStamp = *t1;
  if ((nxtStamp - actStamp) < WRF50_CYCLELEN_MIN) nxtStamp = actStamp + WRF50_CYCLELEN_MIN;
  if ((nxtStamp - actStamp) > WRF50_CYCLELEN_MAX) nxtStamp = actStamp + WRF50_CYCLELEN_MAX;

  *t2 = nxtStamp;
} // calcNextStamp


// do action of state operation: This is THE central code of this firmware
uint32_t doActionOperation(uint64_t *tAct,                    // actual time
                           uint32_t actStatus)                // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t flagIsLate;                                        // flag indicating that we received a 'late' event from ECA
  uint32_t flagIsEarly;                                       // flag 'early'
  uint32_t flagIsConflict;                                    // flag 'conflict'
  uint32_t flagIsDelayed;                                     // flag 'delayed'
  uint32_t ecaAction;                                         // action triggered by event received from ECA
  uint64_t recDeadline;                                       // deadline received from ECA
  uint64_t reqDeadline;                                       // deadline requested by sender
  uint64_t recEvtId;                                          // evt ID received
  uint64_t recParam;                                          // param received
  uint32_t recTEF;                                            // TEF received
  uint32_t recGid;                                            // GID received
  uint32_t recEvtNo;                                          // event number received
  uint32_t recSid;                                            // SID received
  uint32_t recBpid;                                           // BPID received
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtid to send
  uint64_t sendParam;                                         // parameter to send
  uint32_t sendTEF;                                           // TEF to send
  
  uint64_t tluStamp;                                          // timestamp of TLU (50 Hz mains)
  uint64_t dmStamp;                                           // timestamp from DM
  uint64_t dmStampNxt;                                        // next timestamp for DM
  uint64_t tmpEvtNo;

  uint32_t tmpOffsDone;                                       // temporary variable
  
  
  status    = actStatus;

  ecaAction = fwlib_wait4ECAEvent(COMMON_ECATIMEOUT * 1000, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);

  switch (ecaAction) {
    // received WR timing message from Data Master (cycle start)
    case WRF50_ECADO_F50_DM:
      getComLatency = (int32_t)(getSysTime() - recDeadline);
      recGid        = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo      = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      
      if (recGid != PZU_F50) return WRF50_STATUS_BADSETTING;

      // update timestamps
      updateStamps(recDeadline, stampsDM, &validDM);

      if (validDM) {
        // in an ideal world, getTDMAct will equal getTDMSet
        t0DM       = stampsDM[WRF50_N_STAMPS - 1];
        getTDMAct  = t0DM - stampsDM[WRF50_N_STAMPS - 2];
 
        // calculate next cycle Start of Data Master
        // - we know getTDMSet from the previous cycle
        // - if not, set to a actual value
        if (!getTDMSet) getTDMSet = getTDMAct;
        t1DM = t0DM + getTDMSet;

        // statistics for Data Master
        // comparing length of actual cycle with previous cycle; in ideal world, the offset difference should be zero
        getDTDMAct = getTDMAct - (stampsDM[WRF50_N_STAMPS - 2] - stampsDM[WRF50_N_STAMPS - 3]);
        if (getDTDMAct > getDTDMMax) getDTDMMax = getDTDMAct;
        if (getDTDMAct < getDTDMMin) getDTDMMin = getDTDMAct;
      } // if validDM

      break;
      
    // received WR timing message from TLU (50 Hz signal)
    // as this happens with a posttrigger of 500us, this should always (chk ?) happen after receiving the WR timing message from Data Master  
    case WRF50_ECADO_F50_TLU:
      // basic checks
      getComLatency = (int32_t)(getSysTime() - recDeadline);
      recGid        = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo      = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      if (recGid != PZU_F50) return WRF50_STATUS_BADSETTING;

      // correct received deadline for post trigger
      tluStamp      = recDeadline - (uint64_t)WRF50_POSTTRIGGER_TLU;

      // update time stamps
      updateStamps(tluStamp, stampsF50, &validF50);

      if (validF50) {
        t0F50        = stampsF50[WRF50_N_STAMPS - 1];
        getTMainsAct = t0F50 - stampsF50[WRF50_N_STAMPS - 2];
        
        // statistics 50 Hz mains, comparing actual and predicted value (from previous cycle)
        getOffsMainsAct = t0F50 - t1F50;
        if (getOffsMainsAct > getOffsMainsMax) getOffsMainsMax = getOffsMainsAct;
        if (getOffsMainsAct < getOffsMainsMin) getOffsMainsMin = getOffsMainsAct;

        // statistics Data Master
        // comparing actual value of DM and 50 Hz mains; in an ideal world, the offset should be zero
        getOffsDMAct = t0DM - t0F50;
        if (getOffsDMAct > getOffsDMMax) getOffsDMMax = getOffsDMAct;
        if (getOffsDMAct < getOffsDMMin) getOffsDMMin = getOffsDMAct;
      } // if validF50

      // we don't know the current situation, set to lock state 'unknown'
      // getLockState = WRF50_SLOCK_UNKWN;
      
      // count number of received signals and continue only if we have sufficient data
      getNCycles++;
      if (getNCycles < WRF50_N_STAMPS) {
        getLockState = WRF50_SLOCK_UNKWN;
        break;
      } // if getNCycles
    
      // fix missing DM messages for simulation mode
      if (setMode & WRF50_MASK_LOCK_SIM) {
        // fix zero value upon startup
        if (!validDM) {
          t1DM       = t1F50;
          validDM    = 1;
        } // if validDM
      } // if MASK_LOCK_SIM

      // continue only if we have valid information from Data Master and 50 Hz mains
      if (!(validDM && validF50)) {
        getLockState = WRF50_SLOCK_UNKWN;
        break;
      } // if not valid

      // set lock state
      if ((abs(getOffsDMAct)    < WRF50_LOCK_DIFFDM)    &&
          (abs(getDTDMAct)      < WRF50_LOCK_DIFFDTDM)    ) {
        if (getLockState != WRF50_SLOCK_LOCKED) {
          getLockDate = tluStamp;
          getNLocked++;
        }
        getLockState = WRF50_SLOCK_LOCKED;
      } // if abs ...
      else
        getLockState =  WRF50_SLOCK_LOCKING;

      // recap:
      // - we have valid TLU (mains) and DM timestamps
      // - we have done the essential statistics
      // - we have set the lock state
      // todo:
      // - estimate start of next 50 Hz mains cycles and set new period of Data Master
      // - send messages to own ECA and WR network

      // estimate cycle start of next and following cycle 
      nextStamp(stampsF50, &t1F50, &t2F50);

      // calc set-value of period for next DM cycle and respect hard limits
      getTDMSet      = t2F50 - t1DM;
      getTDMSet     -= setF50Offset;                                    // add desired phase offset
      if (getTDMSet < WRF50_CYCLELEN_MIN) getTDMSet = WRF50_CYCLELEN_MIN;
      if (getTDMSet > WRF50_CYCLELEN_MAX) getTDMSet = WRF50_CYCLELEN_MAX;

      // timing message for Data Master
      sendEvtId      = fwlib_buildEvtidV1(PZU_F50, WRF50_ECADO_F50_TUNE, 0x0, 0x0, 0x0, 0x0);
      sendParam      = (uint64_t)getTDMSet & 0xffffffff;
      sendDeadline   = tluStamp + (uint64_t)WRF50_TUNE_MSG_DELAY;                                          // send message with a defined offset to 50 Hz mains signal
      
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0);                                        // write DM set-value of cycle length to local ECA; helpful for debugging

      if (setMode & WRF50_MASK_LOCK_DM) {
        fwlib_ebmWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0);                                      // broadcast set-value of cycle length to network (Data Master)
      } // if LOCK_DM
      else {
        // else: not locking to DM, thus we are in 'simulation mode'
        // mimic 'cycle start' message from DM
        sendEvtId    = fwlib_buildEvtidV1(PZU_F50, WRF50_ECADO_F50_DM, 0x0, 0x0, 0x0, 0x0);
        sendParam    = 0x0;
        sendDeadline = t2F50;
        fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0);                                      
      } // else LOCK_DM

      
      getNSent++;
      getOffsDone    = (uint32_t)(getSysTime() - recDeadline);
                                            
      break;
    default :                                                         // flush ECA Queue
      flagIsLate = 0;                                                 // ignore late events
  } // switch ecaAction

  // check for late event
  if ((status == COMMON_STATUS_OK) && flagIsLate) status = WRF50_STATUS_LATEMESSAGE;
  
  // check WR sync state
  if (fwlib_wrCheckSyncState() == COMMON_STATUS_WRBADSYNC) return COMMON_STATUS_WRBADSYNC;
  else                                                     return status;
} // doActionOperation


int main(void) {
  uint64_t tActMessage;                         // time of actual message
  uint32_t status;                              // (error) status
  uint32_t actState;                            // actual FSM state
  uint32_t pubState;                            // value of published state
  uint32_t reqState;                            // requested FSM state
  uint32_t dummy1;                              // dummy parameter
  uint32_t sharedSize;                          // size of shared memory
  uint32_t *buildID;                            // build ID of lm32 firmware

  // init local variables
  buildID        = (uint32_t *)(INT_BASE_ADR + BUILDID_OFFS);                 // required for 'stack check'  

  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  /* chk init some variables maybe ? */

  init();                                                                     // initialize stuff for lm32
  initSharedMem(&reqState, &sharedSize);                                      // initialize shared memory
  fwlib_init((uint32_t *)_startshared, cpuRamExternal, SHARED_OFFS, sharedSize, "wr-f50", WRF50_FW_VERSION); // init common stuff
  fwlib_clearDiag();                                                          // clear common diagnostics data
  
  while (1) {
    check_stack_fwid(buildID);                                                // check stack status
    fwlib_cmdHandler(&reqState, &dummy1);                                     // check for commands and possibly request state changes
    status = COMMON_STATUS_OK;                                                // reset status for each iteration

    // state machine
    status = fwlib_changeState(&actState, &reqState, status);                 // handle requested state changes
    switch(actState) {                                                        // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&tActMessage, status);
        if (status == COMMON_STATUS_WRBADSYNC)      reqState = COMMON_STATE_ERROR;
        if (status == COMMON_STATUS_ERROR)          reqState = COMMON_STATE_ERROR;
        break;
      default :                                                               // avoid flooding WB bus with unnecessary activity
        status = fwlib_doActionState(&reqState, actState, status);            // other 'do actions' are handled here
        break;
    } // switch
    
    switch (status) {
      case COMMON_STATUS_OK :                                                 // status OK
        statusArray = statusArray |  (0x1 << COMMON_STATUS_OK);               // set OK bit
        break;
      default :                                                               // status not OK
        if ((statusArray >> COMMON_STATUS_OK) & 0x1) fwlib_incBadStatusCnt(); // changing status from OK to 'not OK': increase 'bad status count'
        statusArray = statusArray & ~((uint64_t)0x1 << COMMON_STATUS_OK);     // clear OK bit
        statusArray = statusArray |  ((uint64_t)0x1 << status);               // set status bit and remember other bits set
        break;
    } // switch status
    
    if ((pubState == COMMON_STATE_OPREADY) && (actState  != COMMON_STATE_OPREADY)) fwlib_incBadStateCnt();
    fwlib_publishStatusArray(statusArray);
    pubState = actState;
    fwlib_publishState(pubState);

    if (getComLatency > maxComLatency) maxComLatency = getComLatency;
    if (getOffsDone   > maxOffsDone)   maxOffsDone   = getOffsDone;

    fwlib_publishTransferStatus(0, 0, 0, nEvtsLate, maxOffsDone, maxComLatency);
    
    *pSharedGetTMainsAct      = getTMainsAct;
    *pSharedGetTDMAct         = getTDMAct;
    *pSharedGetTDMSet         = getTDMSet;
    *pSharedGetOffsDMAct      = getOffsDMAct;
    *pSharedGetOffsDMMin      = getOffsDMMin;
    *pSharedGetOffsDMMax      = getOffsDMMax;
    *pSharedGetDTDMAct        = getDTDMAct;
    *pSharedGetDTDMMin        = getDTDMMin;
    *pSharedGetDTDMMax        = getDTDMMax;
    *pSharedGetOffsMainsAct   = getOffsMainsAct;
    *pSharedGetOffsMainsMin   = getOffsMainsMin;
    *pSharedGetOffsMainsMax   = getOffsMainsMax;
    *pSharedGetLockState      = getLockState;
    *pSharedGetLockDateHi     = (uint32_t)(getLockDate >> 32);
    *pSharedGetLockDateLo     = (uint32_t)(getLockDate && 0xffffffff);
    *pSharedGetNLocked        = getNLocked;
    *pSharedGetNCycles        = getNCycles;
    *pSharedGetNSent          = getNSent;
  } // while

  return(1); // this should never happen ...
} // main

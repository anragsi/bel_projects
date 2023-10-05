/********************************************************************************************
 *  b2b-pm-stub.c
 *
 *  created : 2021
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 24-Feb-2023
 *
 *  firmware required for measuring the h=1 phase for ring machine
 *  
 *  - when receiving B2B_ECADO_PHASEMEAS, the phase is measured as a timestamp for an 
 *    arbitraty period
 *  - the phase timestamp is then sent as a timing message to the network
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
#define B2BPMSTUB_FW_VERSION 0x000600                                   // make this consistent with makefile

//standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// includes specific for bel_projects
#include "dbg.h"                                                        // debug outputs
#include <stack.h>                                                      // stack check
#include "ebm.h"                                                        // EB master
#include "pp-printf.h"                                                  // print
#include "mini_sdb.h"                                                   // sdb stuff
#include "aux.h"                                                        // cpu and IRQ
#include "uart.h"                                                       // WR console

// includes for this project
#include <common-defs.h>                                                // common defs for firmware
#include <common-fwlib.h>                                               // common routines for firmware
#include <b2b.h>                                                        // specific defs for b2b
#include <b2bpmstub_shared_mmap.h>                                      // autogenerated upon building firmware

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

// global variables 
volatile uint32_t *pShared;             // pointer to begin of shared memory region
volatile uint32_t *pSharedGetGid;       // pointer to a "user defined" u32 register; here: group ID of extraction machine
volatile uint32_t *pSharedGetSid;       // pointer to a "user defined" u32 register; here: sequence ID of extraction machine
volatile uint32_t *pSharedGetTH1Hi;     // pointer to a "user defined" u32 register; here: period of h=1, high bits
volatile uint32_t *pSharedGetTH1Lo;     // pointer to a "user defined" u32 register; here: period of h=1, low bits
volatile uint32_t *pSharedGetNH;        // pointer to a "user defined" u32 register; here: harmonic number
volatile int32_t  *pSharedGetComLatency;// pointer to a "user defined" u32 register; here: latency for messages received via ECA

uint32_t *cpuRamExternal;               // external address (seen from host bridge) of this CPU's RAM            

uint64_t statusArray;                   // all status infos are ORed bit-wise into statusArray, statusArray is then published
uint32_t nTransfer;                     // # of transfers
uint32_t transStat;                     // status of transfer, here: meanDelta of 'poor mans fit'
int32_t  comLatency;                    // latency for messages received via ECA

// for phase measurement
#define NSAMPLES 11                     // # of timestamps for sampling h=1
uint64_t tStamp[NSAMPLES];              // timestamp samples

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
  sdb_location found_sdb[c_Max_Rams];
  sdb_location found_clu;
  
  // get pointer to shared memory
  pShared                 = (uint32_t *)_startshared;

  // get address to data
  pSharedGetGid           = (uint32_t *)(pShared + (B2B_SHARED_GET_GID        >> 2));
  pSharedGetSid           = (uint32_t *)(pShared + (B2B_SHARED_GET_SID        >> 2));
  pSharedGetTH1Hi         = (uint32_t *)(pShared + (B2B_SHARED_GET_TH1EXTHI   >> 2));   // for simplicity: use 'EXT' for data
  pSharedGetTH1Lo         = (uint32_t *)(pShared + (B2B_SHARED_GET_TH1EXTLO   >> 2));
  pSharedGetNH            = (uint32_t *)(pShared + (B2B_SHARED_GET_NHEXT      >> 2));
  pSharedGetComLatency    =  (int32_t *)(pShared + (B2B_SHARED_GET_COMLATENCY >> 2));

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("b2b-pmstub: fatal error - did not find LM32-CB-CLUSTER!\n");
  } // if idx
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("b2b-pmstub: fatal error - did not find THIS CPU!\n");
  } // if idx
  else cpuRamExternal           = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective
  
  DBPRINT2("b2b-pmstub: CPU RAM external 0x%8x, shared offset 0x%08x\n", cpuRamExternal, SHARED_OFFS);
  DBPRINT2("b2b-pmstub: fw common shared begin   0x%08x\n", pShared);
  DBPRINT2("b2b-pmstub: fw common shared end     0x%08x\n", pShared + (COMMON_SHARED_END >> 2));

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_END >> 2 ) + 1);
  DBPRINT2("b2b-pmstub: fw specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (B2B_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("b2b-pmstub: fw specific shared end   0x%08x\n", pSharedTemp);

  *sharedSize        = (uint32_t)(pSharedTemp - pShared) << 2;

  // basic info to wr console
  DBPRINT1("\n");
  DBPRINT1("b2b-pmstub: initSharedMem, shared size [bytes]: %d\n", *sharedSize);
  DBPRINT1("\n");
  
} // initSharedMem 


// clear project specific diagnostics
void extern_clearDiag()
{
  statusArray  = 0x0; 
  nTransfer    = 0;
  transStat    = 0;
  comLatency   = 0x0;
} // extern_clearDiag
  

// entry action 'configured' state
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // configure EB master (SRC and DST MAC/IP are set from host)
  if ((status = fwlib_ebmInit(2000, 0xffffffffffff, 0xffffffff, EBM_NOREPLY)) != COMMON_STATUS_OK) {
    DBPRINT1("b2b-pmstub: ERROR - init of EB master failed! %u\n", (unsigned int)status);
    return status;
  } 

  // get and publish NIC data
  fwlib_publishNICData();

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

  // clear diagnostics
  fwlib_clearDiag();             

  // flush ECA queue for lm32
  i = 0;
  while (fwlib_wait4ECAEvent(1000, &tDummy, &eDummy, &pDummy, &fDummy, &flagDummy1, &flagDummy2, &flagDummy3, &flagDummy4) !=  COMMON_ECADO_TIMEOUT) {i++;}
  DBPRINT1("b2b-pmstub: ECA queue flushed - removed %d pending entries from ECA queue\n", i);

  // init get values
  *pSharedGetTH1Hi       = 0x0;
  *pSharedGetTH1Lo       = 0x0;
  *pSharedGetNH          = 0x0;
  *pSharedGetGid         = 0x0; 
  *pSharedGetSid         = 0x0;
  *pSharedGetComLatency  = 0x0;

  return COMMON_STATUS_OK;
} // extern_entryActionOperation


uint32_t extern_exitActionOperation()
{
  return COMMON_STATUS_OK;
} // extern_exitActionOperation


// sort timestamps as they might be unordered
void insertionSort(uint64_t *stamps, int n) {
  int      i, j;
  uint64_t tmp;
  for (i=1; i<n; i++) {
    tmp = stamps[i];
    j   = i;
    while ((--j >= 0) && (stamps[j] > tmp)) stamps[j+1] = stamps[j];
    stamps[j+1] = tmp;
  } // for i
} // insertionSort


// 'fit' phase value
 uint32_t phaseFit(uint64_t period, uint32_t nSamples, uint64_t *phase_125ps, uint32_t *dt, uint64_t *confidence_as)
{
  int      i;
  int      usedIdx;      // index of used timestamp
  int32_t  diff;         // difference of two neighboring timestamps [ns]
  int32_t  delta;        // difference from expected period [ns]
  int32_t  periodNs;     // period [ns]
  uint32_t maxDelta;     // max deviation of measured period [ns]
  
  uint64_t phaseTmp;     // intermediate value;
  uint64_t tmp;          // helper variable

  int32_t  test;
  uint64_t t1,t2;

  // dummy implementation
  *phase_125ps   = tStamp[1] << 3;      // use 2nd stamp and convert to [125 ps]
  *dt            = (*phase_125ps) / 100;
  *confidence_as = 1000000000;
  
  return COMMON_STATUS_OK;
} //phaseFit


// aquires a series of timestamps from an IO; returns '0' on success
// this is only a stub doing nothing 
int acquireTimestamps(uint64_t *ts,                           // array of timestamps [ns]
                      uint32_t nReq,                          // number of requsted timestamps
                      uint32_t *nRec,                         // number of received timestamps
                      uint32_t interval,                      // interval for receiving timestamps [us]
                      uint32_t io,                            // number of IO 0..n
                      uint32_t tag                            // expected event tag
                      )
{
  uint32_t ecaAction;                                         // action triggered by ECA
  uint64_t recDeadline;                                       // received deadline
  uint64_t recEvtId;                                          // received EvtId
  uint64_t recParam;                                          // received Parameter
  uint32_t recTEF;                                            // received TEF
  uint32_t flagIsLate;                                        // is late?

  // dummy implementation 
  *nRec = 3;
  ts[0] = getSysTime();
  ts[1] = ts[0] + 1000;
  ts[2] = ts[1] + 1000;
  
  return COMMON_STATUS_OK;
} // acquireTimestamps                  


// do action of state operation: This is THE central code of this firmware
uint32_t doActionOperation(uint64_t *tAct,                    // actual time
                           uint32_t actStatus)                // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t flagIsLate;                                        // flag indicating that we received a 'late' event from ECA
  uint32_t flagEarly;                                         // flag indicating that a 'early event' was received from data master
  uint32_t flagConflict;                                      // flag indicating that a 'conflict event' was received from data master
  uint32_t flagDelayed;                                       // flag indicating that a 'delayed event' was received from data master
  uint32_t ecaAction;                                         // action triggered by event received from ECA
  uint64_t recDeadline;                                       // deadline received from ECA
  uint64_t reqDeadline;                                       // deadline requested by sender
  uint64_t recEvtId;                                          // evt ID received
  uint64_t recParam;                                          // param received
  uint32_t recTEF;                                            // TEF received
  uint32_t recGid;                                            // GID received
  uint32_t recSid;                                            // SID received
  uint32_t recBpid;                                           // BPID received
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtid to send
  uint64_t sendParam;                                         // parameter to send
  uint32_t sendEvtNo;                                         // EvtNo to send
  
  uint32_t nInput;                                            // # of timestamps
  static uint64_t TH1_as;                                     // h=1 period [as]
  static uint64_t tH1_125ps;                                  // h=1 timestamp of phase ( = 'phase') [125 ps]
  uint32_t dt;                                                // uncertainty of h=1 timestamp
  uint64_t confidence_as;                                     // measure for the confidence of the sub-ns part of the phase fit 
  static uint32_t flagPMError;                                // error flag phase measurement

  // diagnostic PM; phase (rf) and match (trigger)
  static uint32_t flagMatchDone;                              // flag: match measurement done
  static uint32_t flagPhaseDone;                              // flag: phase meausrement done
  uint64_t tH1Match_125ps;                                    // h=1 timestamp of match diagnostic [125 ps]
  uint64_t tH1Phase_125ps;                                    // h=1 timestamp of phase diagnostic [125 ps]
  int64_t  Dt;                                                // difference of the two timestamps
  uint64_t remainder;                                         // remainder
  static int64_t dtMatch_as;                                  // deviation of trigger from expected timestamp [as]
  int64_t  dtPhase_as;                                        // deviation of phase from expected timestamp [as]
  
  int      i;
  int      imin;
  static uint32_t nSamples;                                   // # of samples for measurement
  static uint64_t TMeas;                                      // measurement window for timestamps [ns]
  static uint32_t TMeas_us;                                   // measurement window [us]
  int64_t  TWait;                                             // time till measurement start [ns]
  int32_t  TWait_us;                                          // time till measuremetn start [us]

  fdat_t   tmp;                                               // for copying of data
  
  uint64_t t1,t2;

  status    = actStatus;
  sendEvtNo = 0x0;

  ecaAction = fwlib_wait4ECAEvent(COMMON_ECATIMEOUT * 1000, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate, &flagEarly, &flagConflict, &flagDelayed);

  switch (ecaAction) {
    // the following two cases handle H=1 group DDS phase measurement
    case B2B_ECADO_B2B_PMEXT :                                        // this is an OR, no 'break' on purpose
      sendEvtNo   = B2B_ECADO_B2B_PREXT;
    case B2B_ECADO_B2B_PMINJ :
      if (!sendEvtNo) sendEvtNo = B2B_ECADO_B2B_PRINJ;

      comLatency       = (int32_t)(getSysTime() - recDeadline);
      
      *pSharedGetTH1Hi = (uint32_t)((recParam >> 32) & 0x00ffffff);   // lower 56 bit used as period
      *pSharedGetTH1Lo = (uint32_t)( recParam        & 0xffffffff);
      *pSharedGetNH    = (uint32_t)((recParam>> 56)  & 0xff      );   // upper 8 bit used as harmonic number
      TH1_as           = recParam & 0x00ffffffffffffff;
      recGid           = (uint32_t)((recEvtId >> 48) & 0xfff     );
      recSid           = (uint32_t)((recEvtId >> 20) & 0xfff     );
      recBpid          = (uint32_t)((recEvtId >>  6) & 0x3fff    );
      *pSharedGetGid   = recGid;
      *pSharedGetSid   = recSid;
      flagMatchDone    = 0;
      flagPhaseDone    = 0;
      flagPMError      = 0x0;

      nSamples                             = NSAMPLES;
      if (TH1_as > 2000000000000) nSamples = NSAMPLES >> 1;          // use only half the sample for nue > 1MHz
      else                     nSamples = NSAMPLES;
      TMeas           = (uint64_t)(nSamples)*(TH1_as / 1000000000);  // window for acquiring timestamps [ns]
      TMeas_us        = (int32_t)(TMeas / 1000) + 1;                 // add 1 us to avoid a too short window
      
      nInput = 0;
      acquireTimestamps(tStamp, nSamples, &nInput, TMeas_us, 2, B2B_ECADO_TLUINPUT3);

      if (nInput > 2) insertionSort(tStamp, nInput);                  // for 11 timestamps, this is below 10us
      if ((nInput < 3) || (phaseFit(TH1_as, nInput, &tH1_125ps, &dt, &confidence_as) != COMMON_STATUS_OK)) {
        tH1_125ps = 0x7fffffffffffffff;
        if (sendEvtNo ==  B2B_ECADO_B2B_PREXT) flagPMError = B2B_ERRFLAG_PMEXT;
        else                                   flagPMError = B2B_ERRFLAG_PMINJ;
        if (nInput < 3) status = B2B_STATUS_NORF;
        else            status = B2B_STATUS_PHASEFAILED;
      } // if some error occured

      // send command: transmit measured phase value to the network
      sendEvtId    = fwlib_buildEvtidV1(recGid, sendEvtNo, 0, recSid, recBpid, flagPMError);
      sendParam    = tH1_125ps;
      sendDeadline = recDeadline + (uint64_t)COMMON_AHEADT;
      fwlib_ebmWriteTM(sendDeadline, sendEvtId, sendParam, 0, 0);

      // send the confidence value of the phase fit to ECA (for monitoring purposes)
      sendEvtId    = fwlib_buildEvtidV1(0xfff, ecaAction, 0, recSid, recBpid, 0x0);
      sendParam    = confidence_as;
      sendDeadline = getSysTime();                                    // produces a late action but allows explicit monitoring of processing time   
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0, 1);     // force late message

      transStat    = dt;
      nTransfer++;
      //flagIsLate = 0; /* chk */      
      break; // case  B2B_ECADO_B2B_PMEXT

    // the following two cases handle phase matching diagnostic and measure the skew between kicker trigger and H=1 group DDS signals
    case B2B_ECADO_B2B_TRIGGEREXT :                                   // this is an OR, no 'break' on purpose
    case B2B_ECADO_B2B_TRIGGERINJ :                                   // this case only makes sense if cases B2B_ECADO_B2B_PMEXT/INJ succeeded
      if (!flagPMError) {

        reqDeadline = recDeadline + (uint64_t)B2B_PRETRIGGERTR;       // ECA is configured to pre-trigger ahead of time!!!
        nInput   = 0;
        TWait    = (int64_t)((reqDeadline - (TMeas >> 1)) - getSysTime());  // time how long we should wait before starting the measurement
        TWait_us = (TWait / 1000 - 10);                               // the '-10' is a fudge thing
        if (TWait_us > 0) uwait(TWait_us);
        acquireTimestamps(tStamp, nSamples, &nInput, TMeas_us, 2, B2B_ECADO_TLUINPUT3);
        //pp_printf("TMeas %u, TMeasUs %u\n", (uint32_t)TMeas, (uint32_t)TMeasUs);

        // find closest timestamp
        if (nInput > 2) {
          insertionSort(tStamp, nInput);                              // need at least two timestamps
          if (phaseFit(TH1_as, nInput, &tH1Match_125ps, &dt, &confidence_as) == COMMON_STATUS_OK) {
            Dt          = (reqDeadline * 8 - tH1Match_125ps);         // difference to trigger [125 ps]
            // tmp1 = (int32_t)Dt / 8; pp_printf("match1 [ns] %08d\n", tmp1);            
            Dt          = Dt * 125000000;                             // difference [as]
            remainder   =  Dt % TH1_as;                               // remainder [as]
            if (remainder > (TH1_as >> 1)) dtMatch_as = remainder - TH1_as;
            else                           dtMatch_as = remainder;
            // hack
            dtMatch_as = Dt;
            // hack
            flagMatchDone = 1;
            // tmp1 = (int32_t)(dtMatch_as / 1000000); pp_printf("match2 %08d\n", tmp1);
          } // if phasefit
        } // if nInput
        
        // send the confidence value of the phase fit to ECA (for monitoring purposes)
        sendEvtId    = fwlib_buildEvtidV1(0xfff, ecaAction, 0, recSid, recBpid, 0x0);
        sendParam    = confidence_as;
        sendDeadline = getSysTime();                                  // produces a late action but allows explicit monitoring of processing time
        fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0, 1);   // force late message
      } // if not pm error
      //flagIsLate = 0; /* chk */
      
      break; // case  B2B_ECADO_B2B_TRIGGEREXT/INJ

    // the following two cases handle frequency diagnostic and measure the skew between expected and H=1 group DDS signals
    case B2B_ECADO_B2B_PDEXT :                                        // this is an OR, no 'break' on purpose
      sendEvtNo   = B2B_ECADO_B2B_DIAGEXT;
    case B2B_ECADO_B2B_PDINJ :
      if (!sendEvtNo) 
        sendEvtNo = B2B_ECADO_B2B_DIAGINJ;

        recGid          = (uint32_t)((recEvtId >> 48) & 0xfff     );
        recSid          = (uint32_t)((recEvtId >> 20) & 0xfff     );
        recBpid         = (uint32_t)((recEvtId >>  6) & 0x3fff    );

        nInput          = 0;

        acquireTimestamps(tStamp, nSamples, &nInput, TMeas_us, 2, B2B_ECADO_TLUINPUT3);
        // find closest timestamp
        if (nInput > 2) {
          insertionSort(tStamp, nInput);                              // need at least two timestamps
          if (phaseFit(TH1_as, nInput, &tH1Phase_125ps, &dt, &confidence_as) == COMMON_STATUS_OK) {
            Dt          = (tH1Phase_125ps - tH1_125ps) * 125000000;   // difference [as]
            remainder   =  Dt % TH1_as;                               // remainder [as]
            if (remainder > (TH1_as >> 1)) dtPhase_as = remainder - TH1_as;
            else                           dtPhase_as = remainder;
            flagPhaseDone = 1;
          } // if phasefit
        } // if nInput

        // send command: transmit diagnostic information to the network
        sendEvtId    = fwlib_buildEvtidV1(recGid, sendEvtNo, 0, recSid, recBpid, 0);
        if (flagPhaseDone) tmp.f = (float)dtPhase_as / 1000000000.0; // convert to float [ns]
        else               tmp.data = 0x7fffffff;                    // mark as invalid
        sendParam    = (uint64_t)(tmp.data & 0xffffffff) << 32;      // high word; phase diagnostic
        if (flagMatchDone) tmp.f = (float)dtMatch_as / 1000000000.0; // convert to float [ns]
        else               tmp.data = 0x7fffffff;                    // mark as invalid
        sendParam   |= (uint64_t)(tmp.data & 0xffffffff);            // low word; match diagnostic
        sendDeadline = recDeadline + (uint64_t)COMMON_AHEADT;
        fwlib_ebmWriteTM(sendDeadline, sendEvtId, sendParam, 0, 0);

        // send the confidence value of the phase fit to ECA (for monitoring purposes)
        sendEvtId    = fwlib_buildEvtidV1(0xfff, ecaAction, 0, recSid, recBpid, 0x0);
        sendParam    = confidence_as;
        sendDeadline = getSysTime();                                 // produces a late action but allows explicit monitoring of processing time
        fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0, 1);  // force late message
      //flagIsLate = 0; /* chk */
      break; // case  B2B_ECADO_B2B_PDEXT/INJ

    default :                                                         // flush ECA queue
      flagIsLate = 0;                                                 // ingore late events
  } // switch ecaAction
 
  // check for late event
  if ((status == COMMON_STATUS_OK) && flagIsLate) status = B2B_STATUS_LATEMESSAGE;
  
  // check WR sync state
  if (fwlib_wrCheckSyncState() == COMMON_STATUS_WRBADSYNC) return COMMON_STATUS_WRBADSYNC;
  else                                                     return status;
} // doActionOperation


int main(void) {
  uint64_t tActCycle;                           // time of actual UNILAC cycle
  uint32_t status;                              // (error) status
  uint32_t actState;                            // actual FSM state
  uint32_t pubState;                            // value of published state
  uint32_t reqState;                            // requested FSM state
  uint32_t sharedSize;                          // size of shared memory
  
  uint32_t dummy1;                              // dummy parameter
  uint32_t *buildID;                            // build ID of lm32 firmware
 
  // init local variables
  buildID        = (uint32_t *)(INT_BASE_ADR + BUILDID_OFFS);                 // required for 'stack check'  

  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  nTransfer      = 0;

  init();                                                                     // initialize stuff for lm32
  initSharedMem(&reqState, &sharedSize);                                      // initialize shared memory  
  fwlib_init((uint32_t *)_startshared, cpuRamExternal, SHARED_OFFS, sharedSize, "b2b-pmstub", B2BPMSTUB_FW_VERSION); // init common stuff
  fwlib_clearDiag();                                                          // clear common diagnostics data
  
  while (1) {
    check_stack_fwid(buildID);                                                // check stack status
    fwlib_cmdHandler(&reqState, &dummy1);                                     // check for commands and possibly request state changes
    status = COMMON_STATUS_OK;                                                // reset status for each iteration

    // state machine
    status = fwlib_changeState(&actState, &reqState, status);                 // handle requested state changes
    switch(actState) {                                                        // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&tActCycle, status);
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
    fwlib_publishTransferStatus(nTransfer, 0x0, transStat);
    *pSharedGetComLatency = comLatency;
  } // while

  return(1); // this should never happen ...
} // main

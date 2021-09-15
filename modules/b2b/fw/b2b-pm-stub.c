/********************************************************************************************
 *  b2b-pm-stub.c
 *
 *  created : 2021
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 16-Jul-2021
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
#define B2BPMSTUB_FW_VERSION 0x0003001                                  // make this consistent with makefile

/* standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

/* includes specific for bel_projects */
#include "dbg.h"                                                        // debug outputs
#include <stack.h>                                                      // stack check
#include "ebm.h"                                                        // EB master
#include "pp-printf.h"                                                  // print
#include "mini_sdb.h"                                                   // sdb stuff
#include "aux.h"                                                        // cpu and IRQ
#include "uart.h"                                                       // WR console

/* includes for this project */
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
void initSharedMem(uint32_t *reqState)
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

  DBPRINT2("b2b-pmstub: CPU RAM External 0x%08x, begin shared 0x%08x\n", (unsigned int)cpuRamExternal, (unsigned int)SHARED_OFFS);

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_END >> 2 ) + 1);
  while (pSharedTemp < (uint32_t *)(pShared + (B2B_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("b2b-pmstub: used size of shared mem is %d words (uint32_t), begin %x, end %x\n", i, (unsigned int)pShared, (unsigned int)pSharedTemp-1);
  fwlib_publishSharedSize((uint32_t)(pSharedTemp - pShared) << 2);
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
  uint32_t flagDummy;

  // clear diagnostics
  fwlib_clearDiag();             

  // flush ECA queue for lm32
  i = 0;
  while (fwlib_wait4ECAEvent(1000, &tDummy, &eDummy, &pDummy, &fDummy, &flagDummy) !=  COMMON_ECADO_TIMEOUT) {i++;}
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
uint32_t phaseFit(uint64_t period, uint32_t nSamples, uint64_t *phase, uint32_t *dt)
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
  *phase = tStamp[1];
  *dt    = (*phase) / 100;
  
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
  static uint64_t TH1Ns;                                      // h=1 period [ns]
  static uint64_t TH1;                                        // h=1 period [as]
  static uint64_t tH1;                                        // h=1 timestamp of phase ( = 'phase')
  uint32_t dt;                                                // uncertainty of h=1 timestamp
  static uint32_t flagPMError;                                // error flag phase measurement

  // diagnostic PM
  uint64_t tH1Diag;                                           // h=1 timestamp of phase
  uint64_t Dt;                                                // difference of the two timestamps
  uint64_t remainder;                                         // remainder
  int64_t  dtDiag;                                            // deviation from expected timestamp
  uint64_t periodNs;                                          // period [ns]

  // diagnostic match
  static int64_t dtMatch;                                     // deviation from expected timestamp
  int64_t  dtTmp;                                             // helper variable
  uint32_t min;                                               // minimum deviation
  

  int      i;
  int      imin;
  static uint32_t nSamples;                                   // # of samples for measurement
  static uint64_t TMeas;                                      // measurement window for timestamps [ns]
  static uint32_t TMeasUs;                                    // measurement window [us]
  int64_t  TWait;                                             // time till measurement start [ns]
  int32_t  TWaitUs;                                           // time till measuremetn start [us]
  uint64_t t1,t2;

  status    = actStatus;
  sendEvtNo = 0x0;

  ecaAction = fwlib_wait4ECAEvent(COMMON_ECATIMEOUT * 1000, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate);

  switch (ecaAction) {
    // the following two cases handle H=1 group DDS phase measurement
    case B2B_ECADO_B2B_PMEXT :                                        // this is an OR, no 'break' on purpose
      sendEvtNo   = B2B_ECADO_B2B_PREXT;
    case B2B_ECADO_B2B_PMINJ :
      if (!sendEvtNo) sendEvtNo = B2B_ECADO_B2B_PRINJ;

      reqDeadline      = recDeadline + (uint64_t)B2B_PRETRIGGERPM;    // ECA is configured to pre-trigger ahead of time!!!
      comLatency       = (int32_t)(getSysTime() - recDeadline);
      
      *pSharedGetTH1Hi = (uint32_t)((recParam >> 32) & 0x00ffffff);   // lower 56 bit used as period
      *pSharedGetTH1Lo = (uint32_t)( recParam        & 0xffffffff);
      *pSharedGetNH    = (uint32_t)((recParam>> 56)  & 0xff      );   // upper 8 bit used as harmonic number
      TH1              = recParam & 0x00ffffffffffffff;
      recGid           = (uint32_t)((recEvtId >> 48) & 0xfff     );
      recSid           = (uint32_t)((recEvtId >> 20) & 0xfff     );
      recBpid          = (uint32_t)((recEvtId >>  6) & 0x3fff    );
      *pSharedGetGid   = recGid;
      *pSharedGetSid   = recSid;
      dtMatch          = 0x7fffffff;
      dtDiag           = 0x7fffffff;
      flagPMError      = 0x0;

      if (TH1 > 2000000000000) nSamples = NSAMPLES >> 1;             // use only half the sample for nue > 1MHz
      else                     nSamples = NSAMPLES;
      TMeas           = (uint64_t)(nSamples)*(TH1 / 1000000000);     // window for acquiring timestamps [ns]
      TMeasUs         = (int32_t)(TMeas / 1000) + 1;                 // add 1 us to avoid a too short window
      
      nInput = 0;
      acquireTimestamps(tStamp, nSamples, &nInput, TMeasUs, 2, B2B_ECADO_TLUINPUT3);

      if (nInput > 2) insertionSort(tStamp, nInput);                 // for 11 timestamps, this is below 10us
      if ((nInput < 3) || (phaseFit(TH1, nInput, &tH1, &dt) != COMMON_STATUS_OK)) {
        tH1       = 0x7fffffffffffffff;
        if (sendEvtNo ==  B2B_ECADO_B2B_PREXT) flagPMError = B2B_ERRFLAG_PMEXT;
        else                                   flagPMError = B2B_ERRFLAG_PMINJ;
        if (nInput < 3) status = B2B_STATUS_NORF;
        else            status = B2B_STATUS_PHASEFAILED;
      } // if some error occured

      // send command: transmit measured phase value
      sendEvtId    = fwlib_buildEvtidV1(recGid, sendEvtNo, 0, recSid, recBpid, flagPMError);
      sendParam    = tH1;
      sendDeadline = reqDeadline + (uint64_t)COMMON_AHEADT;
      fwlib_ebmWriteTM(sendDeadline, sendEvtId, sendParam, 0);

      transStat    = dt;
      nTransfer++;
      //flagIsLate = 0; /* chk */      
      break; // case  B2B_ECADO_B2B_PMEXT

    // the following two cases handle phase matching diagnostic and measure the skew between kicker trigger and H=1 group DDS signals
    case B2B_ECADO_B2B_TRIGGEREXT :                                   // this is an OR, no 'break' on purpose
    case B2B_ECADO_B2B_TRIGGERINJ :                                   // this case only makes sense if cases B2B_ECADO_B2B_PMEXT/INJ succeeded
      if (!flagPMError) {

        reqDeadline = recDeadline + (uint64_t)B2B_PRETRIGGER;         // ECA is configured to pre-trigger ahead of time!!!
        nInput  = 0;
        TWait   = (int64_t)((reqDeadline - (TMeas >> 1)) - getSysTime());  // time how long we should wait before starting the measurement
        TWaitUs = (TWait / 1000 - 10);                                // the '-10' is a fudge thing
        if (TWaitUs > 0) uwait(TWaitUs);
        acquireTimestamps(tStamp, nSamples, &nInput, TMeasUs, 2, B2B_ECADO_TLUINPUT3);
        //pp_printf("TMeas %u, TMeasUs %u\n", (uint32_t)TMeas, (uint32_t)TMeasUs);

        // find closest timestamp
        if (nInput > 2) {
          insertionSort(tStamp, nInput);                              // need at least two timestamps
          min        = 0x7fffffff;
          imin       = -1;
          for (i=1; i<nInput; i++) {                                  // treat 1st TS as junk 
            dtTmp = reqDeadline - tStamp[i];                        
            if (abs(dtTmp) < min) {min = abs(dtTmp); dtMatch = dtTmp; imin = i;}
          } // for i
          //for (i=1; i<nInput; i++) pp_printf(" %d", (int32_t)(reqDeadline - tStamp[i])); pp_printf(" imin %d, TWait %d, TWaitUs %d\n", imin, (int32_t)TWait, TWaitUs);
        } // if nInput
        // this is ugly!!!! all but the 1st TS are late and thus no longer ordered
        // even worse: the 'fitting' TS might be delayed further and not even received
      } // if not pm error
      //flagIsLate = 0; /* chk */
      
      break; // case  B2B_ECADO_B2B_TRIGGEREXT/INJ

    // the following two cases handle frequency diagnostic and measure the skew between expected and H=1 group DDS signals
    case B2B_ECADO_B2B_PDEXT :                                        // this is an OR, no 'break' on purpose
      sendEvtNo   = B2B_ECADO_B2B_DIAGEXT;
    case B2B_ECADO_B2B_PDINJ :
      if (!sendEvtNo) 
        sendEvtNo = B2B_ECADO_B2B_DIAGINJ;
      if(!flagPMError) {                                              // this case only makes sense if cases  B2B_ECADO_B2B_PMEXT/INJ succeeded

        reqDeadline      = recDeadline + (uint64_t)COMMON_AHEADT;     // ECA is configured to pre-trigger ahead of time!!!
        recGid           = (uint32_t)((recEvtId >> 48) & 0xfff     );
        recSid           = (uint32_t)((recEvtId >> 20) & 0xfff     );
        recBpid          = (uint32_t)((recEvtId >>  6) & 0x3fff    );

        dtDiag    = 0x7fffffff;
        nInput    = 0;

        acquireTimestamps(tStamp, nSamples, &nInput, TMeasUs, 2, B2B_ECADO_TLUINPUT3);
        if (nInput > 2) {
          insertionSort(tStamp, nInput);                              // need at least two timestamps
          if (phaseFit(TH1, nInput, &tH1Diag, &dt) == COMMON_STATUS_OK) {
            Dt          = (tH1Diag - tH1);                            // difference [ns]
            Dt          = Dt * 1000000000;                            // difference [as]
            remainder   = Dt % TH1;                                   // remainder [as]
            remainder   = (uint64_t)((double)remainder / 1000000000.0); // remainder [ns]
            periodNs    = (uint64_t)((double)TH1 / 1000000000.0);     // period [ns]
            if (remainder > (periodNs >> 1)) dtDiag = remainder - periodNs;
            else                             dtDiag = remainder;
          } // if ok
        } // if nInput

        // send command: transmit diagnostic information
        sendEvtId    = fwlib_buildEvtidV1(recGid, sendEvtNo, 0, recSid, recBpid, 0);
        sendParam    = (uint64_t)((dtDiag  & 0xffffffff) << 32);      // high word; phase diagnostic
        sendParam   |= (uint64_t)( dtMatch & 0xffffffff);             // low word; match diagnostic
        sendDeadline = reqDeadline + (uint64_t)COMMON_AHEADT;
        fwlib_ebmWriteTM(sendDeadline, sendEvtId, sendParam, 0);
      } // if not pm error
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
  fwlib_init((uint32_t *)_startshared, cpuRamExternal, SHARED_OFFS, "b2b-pmstub", B2BPMSTUB_FW_VERSION); // init common stuff
  initSharedMem(&reqState);                                                   // initialize shared memory
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

/********************************************************************************************
 *  measurement.c
 *
 *  created : 2021
 *  author  : Enkhbold Ochirsuren, GSI-Darmstadt
 *  version : 04-June-2021
 *
 *  Functions to measure timing performance, packet loss etc
 *
 * -------------------------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2021  Enkhbold Ochirsuren
 * GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 * Planckstrasse 1
 * D-64291 Darmstadt
 * Germany
 *
 * Contact: e.ochirsuren@gsi.de
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
 * For all questions and ideas contact: e.ochirsuren@gsi.de
 * Last update: 04-June-2021
 ********************************************************************************************/

#include "measure.h"

msrSumStats_t sumStats[msr_all] = {0};  // buffer for summary statistics
msrCnt_t cnt[N_MSR_CNT] = {0};          // event and action counters

/**
 * \brief store a timestamp
 *
 * Given timestamp is stored in shared memory location pointed by base + offset.
 *
 * \param base   base address of the user-defined u32 register set
 * \param offset offset in the register set that will store given timestamp
 * \param ts     timestamp
 *
 * \ret none
 **/
void storeTimestamp(uint32_t* base, uint32_t offset, uint64_t ts)
{
  uint64_t* pSharedTs = (uint64_t *)(base + (offset >> 2));

  *pSharedTs = ts;
}

/**
 * \brief get the elapsed time
 *
 * Return an elapsed time, which is the difference of given system time and
 * timestamp stored in shared memory (pointed by base + offset).
 * The timestamp is updated then with the given system time.
 *
 * \param base   base address of the user-defined u32 register set
 * \param offset offset in the register set that stores timestamp needed for calculation
 * \param now    actual system time
 *
 * \ret time     elapsed time in nanosecond since last timestamp
 **/
int64_t getElapsedTime(uint32_t* base, uint32_t offset, uint64_t now)
{
  uint64_t* pSharedTs = (uint64_t *)(base + (offset >> 2));

  int64_t elapsed = now - *pSharedTs;

  *pSharedTs = now;

  return elapsed;
}

/**
 * \brief store timestamps to measure delays
 *
 * Store time points, at which an MPS event was detected and forwarded, in
 * shared memory:
 * - timestamp of MPS event detection is stored in a location pointed by base + offset
 * - timestamp of MPS event transmission is stored in next location.
 *
 * \param base   base address of the user-defined u32 register set
 * \param offset offset in the register set that will store given timestamps
 * \param now    timestamp of MPS event transmission (actual system time)
 * \param tsEca  timestamp of MPS event detection (ECA event deadline)
 *
 * \ret none
 **/
void storeTsMeasureDelays(uint32_t* base, uint32_t offset, uint64_t now, uint64_t tsEca)
{
  uint32_t next = offset + _64b_SIZE;
  uint64_t *pSharedTs = (uint64_t *)(base + (offset >> 2));
  *pSharedTs = now;
  *(pSharedTs + (next >> 2)) = tsEca;
}

/**
 * \brief measure network performance
 *
 * Transmission delay is defined by time-span that points the start of
 * transmission on TX node and completion of reception on RX node.
 * Signalling latency is determined by time-span that points the detection of
 * the MPS event on TX node and completion of signalling it back to the same TX node.
 *
 * The timestamps required to calculate time-spans are stored in shared memory:
 * - timestamp of MPS event transmission is stored in a location pointed by base + offset
 * - timestamp of MPS event detection is stored in next location
 *
 * \param base   base address of the user-defined u32 register set
 * \param offset offset in register set that stores timestamps
 * \param tag    ECA condition tag
 * \param flag   ECA late event flag
 * \param now    actual system time
 * \param tsEca  timestamp of TLU event detection (signalling MPS event back to generator)
 *
 * \ret none
 **/
void measureNwPerf(uint32_t* base, uint32_t offset, uint32_t tag, uint32_t flag, uint64_t now, uint64_t tsEca, bool verbose)
{
  uint64_t *pSharedTs = (uint64_t *)(base + (offset >> 2));    // timestamp of MPS event transmission
  uint64_t tmp64 = *(pSharedTs + ((offset + _64b_SIZE) >> 2)); // timestamp of MPS event detection

  int64_t txDelay = tsEca - *pSharedTs;  // transmission delay
  int64_t sgLatency = now - tmp64;       // signal latency
  msrSumStats_t* pStats;

  if (verbose)
    DBPRINT2("txDly=%lli, sgLty=%lli\n", txDelay, sgLatency);

  pStats = &sumStats[msr_tx_dly];
  calculateSumStats(txDelay, pStats);

  pStats = &sumStats[msr_sg_lty];
  calculateSumStats(sgLatency, pStats);

  // for details, elapsed time of other actions are also calculated
  int64_t poll = now - tsEca;      // elapsed time to detect IO (TLU) event (RX->TX)
  DBPRINT3("IO evt (tag %x, flag %x, ts %llu, now %llu, poll %lli)\n",
      tag, flag, tsEca, now, poll);

  poll = tmp64 - *pSharedTs;       // elapsed time to send MPS event (TX->RX)
  DBPRINT3("MSP evt (detect %llu, send %llu, poll %lli)\n",
      *pSharedTs, tmp64, poll);
}

/**
 * \brief print result of network performance measurement - transmission delay
 *
 * Average, minimum and maximum values of transmission delay are
 * printed to debug output (invoke eb-console $dev to get the debug output) and
 * written to a given location of the shared memory.
 *
 * \param base   base address of the shared memory
 * \param offset offset to the given location
 * \ret none
 **/
void printMeasureTxDelay(uint32_t* base, uint32_t offset) {

  uint64_t *pSharedReg64 = (uint64_t *)(base + (offset >> 2));
  msrSumStats_t* pStats = &sumStats[msr_tx_dly];

  DBPRINT2("txd @0x%08x avg=%llu min=%lli max=%llu cnt=%d/%d\n",
      pSharedReg64,
      pStats->avg, pStats->min, pStats->max,pStats->cntValid, pStats->cntTotal);

  wrSumStats(pStats, pSharedReg64);
}

/**
 * \brief print result of network performance measurement - signalling latency
 *
 * Average, minimum and maximum of signalling latency are
 * printed to debug output (invoke eb-console $dev to get the debug output)
 *
 * \param none
 * \ret none
 **/
void printMeasureSgLatency(uint32_t* base, uint32_t offset) {

  uint64_t *pSharedReg64 = (uint64_t *)(base + (offset >> 2));
  msrSumStats_t* pStats = &sumStats[msr_sg_lty];

  DBPRINT2("sgl @0x%08x avg=%llu min=%lli max=%llu cnt=%d/%d\n",
      pSharedReg64,
      pStats->avg, pStats->min, pStats->max, pStats->cntValid, pStats->cntTotal);

  wrSumStats(pStats, pSharedReg64);
}

/**
 * \brief Count events
 *
 * \param name   Counter name (listed in MSR_CNT)
 * \param value  Used to increment/initialize the counter
 *
 * \ret counter  Value
 **/
uint32_t msrCnt(unsigned name, uint32_t value)
{
  cnt[name].val += value;

  return cnt[name].val;
}

/**
 * \brief Set event counter
 *
 * \param name   Counter name (listed in MSR_CNT)
 * \param value  Used to increment/initialize the counter
 *
 * \ret counter  Value
 **/
uint32_t msrSetCnt(unsigned name, uint32_t value)
{
  cnt[name].val = value;

  return cnt[name].val;
}

/**
 * \brief measure one-way delay
 *
 * The one-way delay (or end-to-end) is the time taken for a timing message
 * (with a MPS flag) to be transmitted across a network (a WRS switch) from
 * a TX node to a RX node.
 *
 * \param now   actual system time
 * \param ts    timestamp of MPS flag
 *
 * \ret none
 **/
void measureOwDelay(uint64_t now, uint64_t ts, bool verbose)
{
  msrSumStats_t* pStats = &sumStats[msr_ow_dly];
  int64_t owd = now - ts;  // one-way (end-to-end) delay
  if (verbose)
    DBPRINT2("owd=%lli\n", owd);

  calculateSumStats(owd, pStats);
}

/**
 * \brief measure the TTL period
 *
 * The TTL period is 101 ms, which corresponds for two lost timing messages.
 *
 * \param buf   pointer to MPS protocol data
 *
 * \ret none
 **/
void measureTtlInterval(mpsTimParam_t* buf)
{
  int64_t interval;
  uint64_t now = getSysTime();
  msrSumStats_t* pStats = &sumStats[msr_ttl];

  // measure time interval
  if (!buf->prot.ttl) {
    interval = now - buf->prot.ts;

    calculateSumStats(interval, pStats);
  }
}

/**
 * \brief print result of the one-way delay measurement
 *
 * \param base   base address of the shared memory
 * \param offset offset to the memory location
 * \ret none
 **/
void printMeasureOwDelay(uint32_t* base, uint32_t offset) {

  uint64_t *pSharedReg64 = (uint64_t *)(base + (offset >> 2));
  msrSumStats_t* pStats = &sumStats[msr_ow_dly];

  DBPRINT2("owd @0x%08x avg=%llu min=%lli max=%llu cnt=%d/%d\n",
      pSharedReg64,
      pStats->avg, pStats->min, pStats->max, pStats->cntValid, pStats->cntTotal);

  wrSumStats(pStats, pSharedReg64);
}

/**
 * \brief print result of the TTL measurement
 *
 * \param base   base address of the shared memory
 * \param offset offset to the memory location
 * \ret none
 **/
void printMeasureTtl(uint32_t* base, uint32_t offset) {

  uint64_t *pSharedReg64 = (uint64_t *)(base + (offset >> 2));
  msrSumStats_t* pStats = &sumStats[msr_ttl];

  DBPRINT2("ttl @0x%08x avg=%llu min=%lli max=%llu cnt=%d/%d\n",
      pSharedReg64,
      pStats->avg, pStats->min, pStats->max, pStats->cntValid, pStats->cntTotal);

  wrSumStats(pStats, pSharedReg64);
}

/**
 * \brief calculate summary statistics
 *
 * \param value  measured value for calculation
 * \param pStats pointer to summary statistics buffer
 * \ret   count  total number of measurements
 **/
uint32_t calculateSumStats(int64_t value, msrSumStats_t* pStats) {

    if (value > 0) {
      pStats->avg = (value + (pStats->cntValid * pStats->avg)) / (pStats->cntValid + 1);
      ++pStats->cntValid;

      if (value > pStats->max)
        pStats->max = value;

      if (value < pStats->min || !pStats->min)
        pStats->min = value;
    }

    return ++pStats->cntTotal;
}

/**
 * \brief write a specified summary statistics to a given memory location
 *
 * \param pStats       pointer to summary statistics (avg, min, max) buffer
 * \param pSharedReg64 address of the shared memory location (64-bit)
 * \ret none
 **/
void wrSumStats(msrSumStats_t* pStats, uint64_t* pSharedReg64) {

  uint32_t *pSharedReg32;

  *pSharedReg64 = pStats->avg;
  *(++pSharedReg64) = pStats->min;
  *(++pSharedReg64) = pStats->max;
  ++pSharedReg64;

  pSharedReg32 = (uint32_t *)pSharedReg64;
  *pSharedReg32 = pStats->cntValid;
  *(++pSharedReg32) = pStats->cntTotal;
}

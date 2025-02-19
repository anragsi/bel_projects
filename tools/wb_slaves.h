//////////////////////////////////////////////////////////////////////////////////////////////
// wb_slaves.h
//
//
//  created : 11-Nov-2016
//  author  : Dietrich Beck, GSI-Darmstadt
//  version : 22-Jun-2023
//
#define WB_SLAVES_VERSION "0.09.1"
//
//  defines wishbone vendor IDs
//  defines wishbone device IDs and registers
//
//  can't include mini-sdb.h due to conflicts with etherbone.h
//
// wherever possible, it was tried to include ...regs.h of the specfici Wishbone
// device. Unfortunately, I did not succeed in handling the autogenerated files using WB_GEN
//
////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef WB_SLAVES_H_
#define WB_SLAVES_H_

/////////////////////////////////////////// vendor IDs /////////////////////////////////////////
#define WB_CERN         0xce42
#define WB_GSI          0x0651

/////////////////////////////////////////// devices ////////////////////////////////////////////

//-- WR-PPS-Generator --
// see ip_cores/wrpc-sw/include/hw/pps_gen_regs.h
// device ID
#define WR_PPS_GEN_VENDOR       WB_CERN     // vendor ID
#define WR_PPS_GEN_PRODUCT      0xde0d8ced  // product ID
#define WR_PPS_GEN_VMAJOR      	1           // major revision
#define WR_PPS_GEN_VMINOR      	1           // minor revision

// register offsets
#define WR_PPS_GEN_CNTR_UTCLO   0x8         // UTC seconds low bytes
#define WR_PPS_GEN_CNTR_UTCHI   0xc         // UTC seconds high bytes
#define WR_PPS_GEN_CNTR_NSEC    0x4         // UTC nanoseconds
#define WR_PPS_GEN_ESCR         0x1c        // External Sync Control Register

// masks
#define WR_PPS_GEN_ESCR_MASKPPS 0x4         // PPS valid bit
#define WR_PPS_GEN_ESCR_MASKTS  0x8         // timestamp valid bit
#define WR_PPS_GEN_ESCR_MASK    0xC         // bit 2: PPS valid, bit 3: timestamp valid



//-- WR-Endpoint --
// see ip_cores/wrpc-sw/include/hw/pps_gen_regs.h
// device ID
#define WR_ENDPOINT_VENDOR      WB_CERN     // vendor ID
#define WR_ENDPOINT_PRODUCT     0x650c2d4f  // device ID
#define WR_ENDPOINT_VMAJOR      1           // major revision
#define WR_ENDPOINT_VMINOR      1           // minor revision

// register offsets
#define WR_ENDPOINT_MACHI       0x24        // MAC high bytes
#define WR_ENDPOINT_MACLO       0x28        // MAC low bytes
#define WR_ENDPOINT_LINK        0x30        // link status

// masks
#define WR_ENDPOINT_MACHI_MASK  0x0000ffff  // only two bytes are of interest
#define WR_ENDPOINT_LINK_MASK   0x000000a0  // only this bit



//-- Etherbone-Config --
#ifdef META_TIMING
#include "hw/etherbone-config.h"
#else
#include "../ip_cores/wrpc-sw/include/hw/etherbone-config.h"
#endif
// device ID
#define ETHERBONE_CONFIG_VENDOR       	 WB_GSI      // vendor ID
#define ETHERBONE_CONFIG_PRODUCT       	 0x68202b22  // product ID
#define ETHERBONE_CONFIG_VMAJOR      	 1           // major revision
#define ETHERBONE_CONFIG_VMINOR      	 1           // minor revision

// register offsets
#define ETHERBONE_CONFIG_IP              EB_IPV4     // IP address

// masks



//-- LM32-RAM-User --
// device ID
#define LM32_RAM_USER_VENDOR      WB_GSI             // vendor ID
#define LM32_RAM_USER_PRODUCT     0x54111351         // product ID
#define LM32_RAM_USER_VMAJOR      1                  // major revision
#define LM32_RAM_USER_VMINOR      0                  // minor revision

// register offsets

// masks



//-- WB4-BlockRAM --
// see ip_cores/wrpc-sw/arch/lm32/crt0.h
// device ID
#define WB4_BLOCKRAM_VENDOR         WB_CERN             // vendor ID
#define WB4_BLOCKRAM_PRODUCT        0x66cfeb52          // product ID
#define WB4_BLOCKRAM_VMAJOR         1                   // major revision
#define WB4_BLOCKRAM_VMINOR         0                   // minor revision

// register offsets
#define WB4_BLOCKRAM_WR_UPTIME      0xa0                // uptime of WR
#define WB4_BLOCKRAM_IPSTATE_050004 0x1a5b4             // ip state for gw v5.0.4
#define WB4_BLOCKRAM_IPSTATE_060001 0x1b544             // ip state for gw v6.0.1
#define WB4_BLOCKRAM_IPSTATE_060102 0x1b544             // ip state for gw v6.1.2
#define WB4_BLOCKRAM_IPSTATE_060201 0x1b544             // ip state for gw v6.2.1

// masks



//-- WR-Periph-1Wire --
// device ID
#define WB4_PERIPH_1WIRE_VENDOR      WB_CERN             // vendor ID
#define WB4_PERIPH_1WIRE_PRODUCT     0x779c5443          // product ID
#define WB4_PERIPH_1WIRE_VMAJOR      1                   // major revision
#define WB4_PERIPH_1WIRE_VMINOR      1                   // minor revision

// register offsets

// masks



//-- User-1Wire --
// device ID
#define USER_1WIRE_VENDOR            WB_GSI              // vendor ID
#define USER_1WIRE_PRODUCT           0x4c8a0635          // product ID
#define USER_1WIRE_VMAJOR            1                   // major revision
#define USER_1WIRE_VMINOR            1                   // minor revision

// register offsets

// masks



//-- ECA  --
#ifdef META_TIMING
#include "eca_regs.h"
#else
#include "../ip_cores/wr-cores/modules/wr_eca/eca_regs.h"
#endif
// device ID
#define ECA_CTRL_VENDOR              WB_GSI              // vendor ID
#define ECA_CTRL_PRODUCT             ECA_SDB_DEVICE_ID   // product ID
#define ECA_CTRL_VMAJOR              1                   // major revision
#define ECA_CTRL_VMINOR              0                   // minor revision

// register offsets
#define ECA_CTRL_TIME_HI_GET         ECA_TIME_HI_GET     // UTC high 32 bit
#define ECA_CTRL_TIME_LO_GET         ECA_TIME_LO_GET     // UTC low  32 bit

// masks


//-- RESET  --
// device ID
#define FPGA_RESET_VENDOR            WB_GSI              // vendor ID
#define FPGA_RESET_PRODUCT           0x3a362063          // product ID
#define FPGA_RESET_VMAJOR            1                   // major revision
#define FPGA_RESET_VMINOR            3                   // minor revision

// register offsets
#define FPGA_RESET_RESET             0x0000              // reset register of FPGA (write), write 'deadbeef' to reset
#define FPGA_RESET_USERLM32_GET      0x0004              // get reset status of user lm32, one bit per CPU, bit 0 is CPU 0 (read)
#define FPGA_RESET_USERLM32_SET      0x0008              // puts user lm32 into RESET, one bit per CPU, bit 0 is CPU 0 (write)
#define FPGA_RESET_USERLM32_CLEAR    0x000c              // clears RESET of user lm32, one bit per CPU, bit 0 is CPU 0 (write)
#define FPGA_RESET_WATCHDOG_DISABLE  0x0004              // disables watchdog (write),    write 'cafebabe' to disable watchdog
                                                         //                               write 'cafebab0' to reenable watchdog
#define FPGA_RESET_WATCHDOG_STAT     0x000c              // reads watchdog stauts (read), read '1': watchdog enabled, '0': watchdog disabled
#define FPGA_RESET_WATCHDOG_TRG      0x0010              // retrigger watchdog (write),   write 'cafebabe' regularly to prevent auto-reset
                                                         //
#define FPGA_RESET_PHY_RESET         0x0014              // reset register of PHY and SFP (write/read)
#define FPGA_RESET_PHY_DROP_LINK_WR  0x0001              // drop link: main (White Rabbit) port
#define FPGA_RESET_PHY_DROP_LINK_AUX 0x0002              // drop link: auxiliary port
#define FPGA_RESET_PHY_SFP_DIS_WR    0x0004              // disable SFP: main (White Rabbit) port
#define FPGA_RESET_PHY_SFP_DIS_AUX   0x0008              // disable SFP: auxiliary port
// masks


//-- BUILD ROM  --
// device ID
#define FPGA_BUILDROM_VENDOR         WB_GSI              // vendor ID
#define FPGA_BUILDROM_PRODUCT        0x2d39fa8b          // product ID
#define FPGA_BUILDROM_VMAJOR         1                   // major revision
#define FPGA_BUILDROM_VMINOR         1                   // minor revision

// register offsets

// masks



//-- Data Master (and slave) diagnosis  --
#include "../modules/dm_diag/dm_diag_regs.h"
// device ID
#define DM_DIAG_VENDOR               DM_DIAG_SDB_VENDOR_ID   // vendor ID
#define DM_DIAG_PRODUCT              DM_DIAG_SDB_DEVICE_ID   // product ID
#define DM_DIAG_VMAJOR               1                       // major revision
#define DM_DIAG_VMINOR               0                       // minor revision

// register offsets
// see dm_diag_regs.h
// masks
// see dm_diag_regs.h


//-- ECA TAP diagnosis  --
#include "../modules/eca_tap/eca_tap_regs.h"  /* chk: sept-2019: missing ECA_TAP_CAPTURE: manually patched */
// device ID
#define ECA_TAP_VENDOR               ECA_TAP_SDB_VENDOR_ID   // vendor ID
#define ECA_TAP_PRODUCT              ECA_TAP_SDB_DEVICE_ID   // product ID
#define ECA_TAP_VMAJOR               1                       // major revision
#define ECA_TAP_VMINOR               0                       // minor revision

// register offsets
// see eca_tap_regs.h
// masks
// see eca_tap_regs.h


//-- IO CONTROL  --
// device ID
#define IO_CTRL_VENDOR               WB_GSI            // vendor ID
#define IO_CTRL_PRODUCT              0x10c05791        // product ID
#define IO_CTRL_VMAJOR               0                 // major revision
#define IO_CTRL_VMINOR               0                 // minor revision

// register offsets
#define IO_CTRL_LVDSINGATESETLOW     0x2000            // LVDS input gate set low, one bit per input   /* chk, subject to change */
#define IO_CTRL_LVDSINGATERESETLOW   0x2008            // LVDS input gate reset low, one bit per input /* chk, subject to change */

// masks




//-- TLU --
//device ID
#define GSI_TM_LATCH_VENDOR          WB_GSI      //vendor ID
#define GSI_TM_LATCH_PRODUCT         0x10051981  //product ID
#define GSI_TM_LATCH_VMAJOR          1           //major revision
#define GSI_TM_LATCH_VMINOR          1           //minor revision

//clock
#define GSI_TM_LATCH_CLOCK           8           //clock period [ns]

//register offsets
#define GSI_TM_LATCH_FIFO_READY      0x000       //n..0 channel(n) timestamp(s) ready       (ro)
#define GSI_TM_LATCH_FIFO_CLEAR      0x004       //n..0 channel(n) FIFO clear               (wo)
#define GSI_TM_LATCH_TEST_CHANNELS   0x008       //Generate a test Event                    (wo)
#define GSI_TM_LATCH_TRIG_ARMSTAT    0x00C       //n..0 channel(n) trigger armed status     (ro)
#define GSI_TM_LATCH_TRIG_ARMSET     0x010       //n..0 channel(n) trigger set armed        (wo)
#define GSI_TM_LATCH_TRIG_ARMCLR     0x014       //n..0 channel(n) trigger clr armed        (wo)
#define GSI_TM_LATCH_TRIG_EDGESTAT   0x018       //n..0 channel(n) trigger edge status      (ro)
#define GSI_TM_LATCH_TRIG_EDGEPOS    0x01C       //n..0 channel(n) trigger edge set pos     (wo)
#define GSI_TM_LATCH_TRIG_EDGENEG    0x020       //n..0 channel(n) trigger edge set neg     (wo)

//IRQ
#define GSI_TM_LATCH_IRQ_ENABLE      0x024       // Enable/Disable Global IRQ               (rw)
#define GSI_TM_LATCH_IRQ_MASKSTAT    0x028       // Status of   IRQ Channel Mask
#define GSI_TM_LATCH_IRQ_MASKSET     0x02C       // n...0 channel(n) IRQ Mask Set           (wo)
#define GSI_TM_LATCH_IRQ_MASKCLR     0x030       // n...0 channel(n) IRQ Mask Clear         (wo)

// Channels Related Parameters
#define GSI_TM_LATCH_CHNS_TOTAL      0x034       // Total Number of Channels in Device      (ro)
#define GSI_TM_LATCH_CHNS_FIFOSIZE   0x038       // Total size of FIFOs                     (ro)

//Timestamp Read Addresses
//one must read ATSHI prior to reading ATSLO
#define GSI_TM_LATCH_ATSHI           0x050       //actual time stamp HIGH words in cycles   (ro)
#define GSI_TM_LATCH_ATSLO           0x054       //actual time stamp LOW words in cycles    (ro)

// Channel to be selected n....0 and the other operations depend on selected channel
#define GSI_TM_LATCH_CH_SELECT       0x058       //Channel Select                           (rw)

// *IMP* All operations below depend on the Channel Selection
#define GSI_TM_LATCH_FIFO_POP        0x05C       //pop the topmost FIFO Q Element           (wo)
#define GSI_TM_LATCH_FIFO_TEST       0x060       // Generate a test Event Pulse             (wo)
//pop just adjusts the pointer to the FIFO, it does not re-write a default value
#define GSI_TM_LATCH_FIFO_CNT        0x064       //FIFO Queue   fill count                  (ro)
#define GSI_TM_LATCH_FIFO_FTSHI      0x068       //timestamp HIGH words in cycles           (ro)
#define GSI_TM_LATCH_FIFO_FTSLO      0x06c       //timestamp LOW words in cycles            (ro)
#define GSI_TM_LATCH_FIFO_FTSSUB     0x070       //timestamp sub-cycle                      (ro)

//masks


#endif  // wb_slaves.h

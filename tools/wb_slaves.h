//////////////////////////////////////////////////////////////////////////////////////////////
// wb_slaves.h
//
//
//  created : 11-Nov-2016
//  author  : Dietrich Beck, GSI-Darmstadt
//  version : 20-Apr-2018
//
#define WB_SLAVES_VERSION "0.06.0"
//
//  defines wishbone vendor IDs
//  defines wishbone device IDs and registers
//
//
////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef WB_SLAVES_H_
#define WB_SLAVES_H_

#include "../ip_cores/wr-cores/modules/wr_eca/eca_regs.h"

/////////////////////////////////////////// vendor IDs /////////////////////////////////////////
#define WB_CERN         0xce42
#define WB_GSI          0x0651

/////////////////////////////////////////// devices ////////////////////////////////////////////

//-- WR-PPS-Generator --
// device ID
#define WR_PPS_GEN_VENDOR       WB_CERN     // vendor ID
#define WR_PPS_GEN_PRODUCT      0xde0d8ced  // product ID
#define WR_PPS_GEN_VMAJOR      	1           // major revision
#define WR_PPS_GEN_VMINOR      	1           // minor revision

// register offsets, see ip_cores/wrpc-sw/include/hw/pps_gen_regs.h
#define WR_PPS_GEN_CNTR_UTCLO   0x8         // UTC seconds low bytes
#define WR_PPS_GEN_CNTR_UTCHI   0xc         // UTC seconds high bytes
#define WR_PPS_GEN_CNTR_NSEC    0x4         // UTC nanoseconds
#define WR_PPS_GEN_ESCR         0x1c        // External Sync Control Register

// masks
#define WR_PPS_GEN_ESCR_MASKPPS 0x4         // PPS valid bit
#define WR_PPS_GEN_ESCR_MASKTS  0x8         // timestamp valid bit
#define WR_PPS_GEN_ESCR_MASK    0xC         // bit 2: PPS valid, bit 3: timestamp valid


//-- WR-Endpoint --
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
// device ID
#define ETHERBONE_CONFIG_VENDOR       	 WB_GSI      // vendor ID
#define ETHERBONE_CONFIG_PRODUCT       	 0x68202b22  // product ID
#define ETHERBONE_CONFIG_VMAJOR      	 1           // major revision
#define ETHERBONE_CONFIG_VMINOR      	 1           // minor revision

// register offsets
#define ETHERBONE_CONFIG_IP              0x18        // IP address in hex

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
// device ID
#define WB4_BLOCKRAM_VENDOR      WB_CERN             // vendor ID
#define WB4_BLOCKRAM_PRODUCT     0x66cfeb52          // product ID
#define WB4_BLOCKRAM_VMAJOR      1                   // major revision
#define WB4_BLOCKRAM_VMINOR      1                   // minor revision

// register offsets
#define WB4_BLOCKRAM_WR_UPTIME   0xa0                // uptime of WR

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
#define FPGA_RESET_VMINOR            1                   // minor revision

// register offsets
#define FPGA_RESET_RESET             0x0000              // reset register of FPGA
#define FPGA_RESET_USERLM32_GET      0x0004              // get reset status of user lm32, one bit per CPU, bit 0 is CPU 0
#define FPGA_RESET_USERLM32_SET      0x0008              // puts user lm32 into RESET, one bit per CPU, bit 0 is CPU 0
#define FPGA_RESET_USERLM32_CLEAR    0x000c              // clears RESET of user lm32, one bit per CPU, bit 0 is CPU 0

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
#define GSI_TM_LATCH_TRIG_EDGESTAT   0x018           //n..0 channel(n) trigger edge status  (ro)
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


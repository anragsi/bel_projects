# Simulation settings
TB_NAME="blinky_testbench"
GHDL_BIN="ghdl"
GHDL_FLAGS="--ieee=synopsys --warn-no-vital-generic"
STOP_TIME="1600ns"
VCD_NAME="$TB_NAME.vcd"
GHW_NAME="$TB_NAME.ghw"
VCD_VIEWER="gtkwave"
GTKW_NAME="$TB_NAME.gtkw"

# Files
BLINKY_PATH="../src/"
VHD_PACK="../../../ip_cores/general-cores/modules/genrams/genram_pkg.vhd \
          ../../../ip_cores/general-cores/modules/wishbone/wishbone_pkg.vhd"
VHD_FILES="$VHD_PACK \
           ../../../ip_cores/general-cores/modules/wishbone/wb_slave_adapter/wb_slave_adapter.vhd \
           $BLINKY_PATH/blinky_pkg.vhd \
           $BLINKY_PATH/blinky.vhd \
           blinky_testbench.vhd"

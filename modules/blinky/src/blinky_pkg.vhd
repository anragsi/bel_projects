library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;

package blinky_pkg is

 constant c_blinky_sdb : t_sdb_device := (
    abi_class     => x"0000", -- undocumented device
    abi_ver_major => x"00",
    abi_ver_minor => x"01",
    wbd_endian    => c_sdb_endian_big,
    wbd_width     => x"7", -- 8/16/32-bit port granularity
    sdb_component => (
    addr_first    => x"0000000000000000",
    addr_last     => x"000000000000000f",
    product       => (
    vendor_id     => x"0000000000000651",
    device_id     => x"434E5452",
    version       => x"00000001",
    date          => x"20241025",
    name          => "GSI:BLINKBLINKBLINK")));

  component blinky is

    port(
    s_clk_sys_i     : in std_logic;
    s_rst_sys_n_i   : in std_logic;

    t_wb_out        : out t_wishbone_slave_out;
    t_wb_in         : in  t_wishbone_slave_in;

    s_led_o         : out std_logic
    );

  end component;

end package;
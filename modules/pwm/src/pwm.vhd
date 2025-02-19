library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;
use work.spwm_wbgen2_pkg.all; 

entity pwm is


    generic (
        g_simulation                : in boolean := false;
        g_pwm_channel_num           : integer range 1 to 8 := 8;
        g_pwm_interface_mode        : t_wishbone_interface_mode := PIPELINED
    );

    port(
    
    -- these two sys signals come from SysCon
    s_clk_sys_i       : in std_logic;
    s_rst_sys_n_i     : in std_logic;


    t_wb_out          : out t_wishbone_slave_out;
        -- type t_wishbone_slave_out is record
        -- ack   : std_logic;
        -- err   : std_logic;
        -- rty   : std_logic;
        -- stall : std_logic;
        -- dat   : t_wishbone_data;
        -- end record t_wishbone_slave_out;
        -- equal to t_wishbone_master_in

    t_wb_in            : in  t_wishbone_slave_in;
        --type t_wishbone_slave_in is record
        --cyc : std_logic;
        --stb : std_logic;
        --adr : t_wishbone_address;
        --sel : t_wishbone_byte_select;
        --we  : std_logic;
        --dat : t_wishbone_data;
        --end record t_wishbone_slave_in;
        -- equal to t_wishbone_master_out

    s_pwm_o           : out std_logic_vector(g_pwm_channel_num-1 downto 0)
    -- start with only one channel
    );

end entity;

architecture pwm_arch of pwm is

    signal s_tb_pwm_o : std_logic;

begin

    PWM_WB : wb_simple_pwm
    generic map(
        g_num_channels          =>  g_pwm_channel_num,
        g_interface_mode        =>  g_pwm_interface_mode,
        g_default_val           =>  2
    )
    port map (
        rst_n_i     =>  s_rst_sys_n_i,
        clk_sys_i   =>  s_clk_sys_i,

        wb_adr_i    => t_wb_in.adr(5 downto 0),
        wb_dat_i    => t_wb_in.dat,    
        wb_dat_o    => t_wb_out.dat,
        wb_cyc_i    => t_wb_in.cyc,
        wb_sel_i    => t_wb_in.sel,
        wb_stb_i    => t_wb_in.stb,
        wb_we_i     => t_wb_in.we,
        wb_ack_o    => t_wb_out.ack,
        wb_stall_o  => t_wb_out.stall,

        pwm_o       =>  s_pwm_o

    );

end pwm_arch;

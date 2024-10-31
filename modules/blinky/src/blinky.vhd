library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;


entity blinky is

    port(
    
    -- these two sys signal come from SysCon
    s_clk_sys_i       : in std_logic;
    s_rst_sys_i       : in std_logic;


    t_wb_out          : out t_wishbone_slave_out;
      -- type t_wishbone_slave_out is record
      -- ack   : std_logic;
      -- err   : std_logic;
      -- rty   : std_logic;
      -- stall : std_logic;
      -- dat   : t_wishbone_data;
      -- end record t_wishbone_slave_out;

    t_wb_in            : in  t_wishbone_slave_in;
      --type t_wishbone_slave_in is record
      --cyc : std_logic;
      --stb : std_logic;
      --adr : t_wishbone_address;
      --sel : t_wishbone_byte_select;
      --we  : std_logic;
      --dat : t_wishbone_data;
      --end record t_wishbone_slave_in;

    s_led_o           : out std_logic
    );

end entity;

architecture blinky_arch of blinky is

  signal s_rst_sys_n        : std_logic := '0';

  signal s_led_state        : std_logic := '0';

  signal s_stall_state        : std_logic := '0';

begin
  
  --  for now no errors, no stalling
  t_wb_out.err    <= '0';
  t_wb_out.stall  <= s_stall_state;
  t_wb_out.rty    <= '0';

  s_led_o <= s_led_state;

  -- single read write cycle
  p_wb_read_write: process(s_clk_sys_i, s_rst_sys_i)

  begin

    -- reset is active low
    if (s_rst_sys_i = '0') then
      -- all wishbone interfaces must initialize on the rising clock after reset was

      -- RESET
      t_wb_out.ack    <= '0';
      t_wb_out.dat    <= (others => '0');

      s_led_state <= '0';
      s_stall_state <= '0';

    -- otherwise listen for rising edge
    elsif rising_edge(s_clk_sys_i) then

      -- DO THE HANDSHAKE
      -- be quick no stalling
      --t_wb_out.ack <= '1';
      t_wb_out.ack    <= '0';
      t_wb_out.dat(0) <= s_led_state;
        
      -- are we selected?
      -- STB on 1 (strobe is kind of chip select)
      -- CYC on 1 (bus cycle, active high)
      if t_wb_in.stb = '1' and t_wb_in.cyc = '1' and s_stall_state = '0'  then

        -- DO THE HANDSHAKE
        -- be quick no stalling
        t_wb_out.ack <= '1';
        
        -- WRITE
        -- WE on 1 (write enable, active high)
        if t_wb_in.we = '1' then

            s_led_state <= t_wb_in.dat(0);


        end if;

      end if; -- WRITE: wishbone_i.stb = '1' and wishbone_i.cyc = '1' and wishbone_i.we = '1'

    end if; --rstn_sys_i

  end process;
            
end blinky_arch;
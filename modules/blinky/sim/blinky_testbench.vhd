--wishbone secondary testbecnh
--bare bones testbench to test slave communication
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library std;
use std.textio.all;

library work;
use work.wishbone_pkg.all;


--Entity
entity blinky_testbench is
end;

architecture blinky_testbench_architecture of blinky_testbench is

  --Testbench settings
  constant c_reset_time   : time  := 200 ns;
  constant c_clock_cycle  : time  := 16 ns;
  constant c_light_time   : time  := 32 ns;
  constant c_sim_time     : time  := 20 ms;

  constant c_test_write   : integer  := 1;
  constant c_test_read    : integer  := 1;

  -- Other constants
  constant c_reg_all_zero                : std_logic_vector(31 downto 0) := ( others => '0');
  constant c_cyc_on                      : std_logic := '1';
  constant c_cyc_off                     : std_logic := '0';
  constant c_str_on                      : std_logic := '1';
  constant c_str_off                     : std_logic := '0';
  constant c_we_on                       : std_logic := '1';
  constant c_we_off                      : std_logic := '0';

  constant c_blinky_on    : std_logic_vector(31 downto 0) := ( 0 => '1', others => '0');
  constant c_blinky_off   : std_logic_vector(31 downto 0) := ( others => '0');
  constant c_blinky_B_on  : std_logic_vector(31 downto 0) := ( 0 => '1', 1 => '0', 2 => '0', 3 => '1', others => '0');
  constant c_blinky_B_off : std_logic_vector(31 downto 0) := ( 0 => '0', 1 => '0', 2 => '0', 3 => '1', others => '0');
  constant c_blinky_C_on  : std_logic_vector(31 downto 0) := ( 0 => '1', 1 => '1', 2 => '1', 3 => '1', others => '0');
  constant c_blinky_C_off : std_logic_vector(31 downto 0) := ( 0 => '0', 1 => '1', 2 => '1', 3 => '1', others => '0');
  
  -- Basic device signals
  signal s_clk        : std_ulogic := '0';
  signal s_rst_n      : std_ulogic := '1';
  
  -- Wishbone connections
  signal s_wb_master_in  : t_wishbone_slave_out;  -- equal to t_wishbone_master_in
  signal s_wb_master_out : t_wishbone_slave_in;   -- equal to t_wishbone_master_out

  -- blinky specific signals
  signal s_led_state : std_ulogic := '0';
  
  -- Function wb_stim -> Helper function to create a human-readable testbench
  function wb_stim(cyc : std_logic; stb : std_logic; we : std_logic;
                   dat : t_wishbone_data) return t_wishbone_slave_in is
  variable v_setup : t_wishbone_slave_in;
  begin
    v_setup.cyc := cyc;
    v_setup.stb := stb;
    v_setup.we  := we;
    v_setup.adr := (others => '0'); -- Don't care
    v_setup.dat := dat;
    v_setup.sel := (others => '0'); -- Don't care
    return v_setup;
  end function wb_stim;

function to_integer( s : std_logic ) return natural is
begin
      if s = '1' then
      return 1;
   else
      return 0;
   end if;
end function;

  --! @brief Return a intrepresentation of a logic value
function to_logic_to_int(x : std_logic) return natural is
  begin
    return to_integer(x);
  end function;


  component blinky is

    generic (
      g_simulation    : in boolean
  );

    port(
    s_clk_sys_i       : in std_logic;
    s_rst_sys_n_i     : in std_logic;

    t_wb_out          : out t_wishbone_slave_out;
    t_wb_in           : in  t_wishbone_slave_in;

    s_led_o           : out std_logic
    );

  end component;

  begin

    dut : blinky
    generic map
      (
        g_simulation  => true
      )
      port map (
        s_clk_sys_i     => s_clk,
        s_rst_sys_n_i   => s_rst_n,

        t_wb_out        => s_wb_master_in,
        t_wb_in         => s_wb_master_out,

        s_led_o         => s_led_state);


    -- generate clock
    p_clock : process
      begin
        s_clk <= '0';
        wait for c_clock_cycle/2;
        s_clk <= '1';
        wait for c_clock_cycle/2;
      end process;

    --Reset controller
    p_reset : process
    begin
       s_rst_n <= '0';
       wait for c_reset_time;
       s_rst_n <= '1';
       wait for c_sim_time;
    end process;

    p_test: process
        begin
            -- RESET active
            --
            s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            report("Testing Reset");
            wait until rising_edge(s_rst_n);
            -- RESET inactive
            --
            -- test SINGLE WRITE TURN LED ON THEN OFF
            --
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_on);
            report("WRITE: mode A on");
            s_wb_master_out  <= wb_stim(c_cyc_on, c_str_off, c_we_on, c_blinky_on);
            wait until rising_edge(s_clk);
            while (s_wb_master_in.ack = '0') loop
              wait until rising_edge(s_clk);
            end loop;
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off,  c_blinky_on);
            for i in 0 to 30 loop
              wait until rising_edge(s_clk);
            end loop; -- Waiter
              wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_off);
            report("WRITE: mode A off");
            while (s_wb_master_in.ack = '0') loop
              wait until rising_edge(s_clk);
            end loop;
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            --
            -- test SINGLE WRITE TURN LED ON THEN OFF END
            --
            for i in 0 to 5 loop
              wait until rising_edge(s_clk);
            end loop; -- Waiter
            --
            -- test SINGLE READ
            --
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_off, c_blinky_off);
            report("READ");
            while (s_wb_master_in.ack = '0') loop
              wait until rising_edge(s_clk);
            end loop;
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off,  c_blinky_off);
            --
            -- test SINGLE READ END
            --
            for i in 0 to 5 loop
              wait until rising_edge(s_clk);
            end loop; -- Waiter
            --
            -- test SINGLE WRITE MODE B ON THEN OFF
            --
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_C_on);
            report("WRITE: mode C on");
            while (s_wb_master_in.ack = '0') loop
              wait until rising_edge(s_clk);
            end loop;
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off,  c_blinky_C_on);
            for i in 0 to 30 loop
              wait until rising_edge(s_clk);
            end loop; -- Waiter
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_C_off);
            report("WRITE: mode C off");
            while (s_wb_master_in.ack = '0') loop
              wait until rising_edge(s_clk);
            end loop;
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            --
            -- test SINGLE WRITE MODE B ON THEN OFF END
            --
            --
            -- test SINGLE READ
            --
            wait until rising_edge(s_clk);
            s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_off, c_blinky_off);
            report("READ");
            while (s_wb_master_in.ack = '0') loop
              wait until rising_edge(s_clk);
            end loop;
            s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off,  c_reg_all_zero);
            --
            -- test SINGLE READ END
            --
            --
            -- test SINGLE READ
            --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_off, c_blinky_off);
            -- report("Testing single read");
            -- while (s_wb_master_in.ack = '0') loop
            --   wait until rising_edge(s_clk);
            -- end loop;
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off,  c_reg_all_zero);
            -- --
            -- -- test SINGLE READ END
            -- --
            -- --
            -- -- test SINGLE WRITE MODE B
            -- --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_off);
            -- report("Testing single write LED off");
            -- wait until rising_edge(s_wb_master_in.ack);
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            -- wait until rising_edge(s_clk);
            -- wait until rising_edge(s_clk);
            -- wait until rising_edge(s_clk);
            -- wait until rising_edge(s_clk);
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_on);
            -- report("Testing single write LED on");
            -- wait until rising_edge(s_wb_master_in.ack);
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            -- --
            -- -- test SINGLE WRITE END
            -- --
            -- -- test SINGLE READ
            -- --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_off, c_reg_all_zero);
            -- report("Testing single read");
            -- wait until rising_edge(s_wb_master_in.ack);
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            -- --
            -- -- test SINGLE READ END
            -- --
            -- wait until rising_edge(s_clk);
            -- wait until rising_edge(s_clk);
            -- --
            -- -- test SINGLE WRITE TURN PATTERN B 1 ON
            -- --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_B_on);
            -- wait until rising_edge(s_wb_master_in.ack);
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            -- for i in 0 to 30 loop
            --   wait until rising_edge(s_clk);
            --   report("waited");
            -- end loop; -- Waiter
            -- wait until rising_edge(s_clk);
            -- --
            -- -- test SINGLE WRITE END
            -- --
            -- -- test SINGLE WRITE TURN PATTERN C ON
            -- --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_C_on);
            -- wait until rising_edge(s_wb_master_in.ack);
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            -- for i in 0 to 60 loop
            --   wait until rising_edge(s_clk);
            --   report("waited");
            -- end loop; -- Waiter
            -- wait until rising_edge(s_clk);
            -- --
            -- -- test SINGLE WRITE END
            -- --
            -- -- test SINGLE READ
            -- --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_reg_all_zero);
            -- report("set c_reg_all_zero");
            -- wait until rising_edge(s_wb_master_in.ack);
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_blinky_off);
            -- s_wb_master_out  <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_reg_all_zero);
            -- for i in 0 to 30 loop 
            --   wait until rising_edge(s_clk);
            --   report("waited");
            -- end loop; -- Waiter
            -- --
            -- -- test SINGLE READ END
            -- --
            -- -- test SINGLE WRITE TURN PATTERN B OFF to C ON
            -- --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_C_off);
            -- report("set c_blinky_C_off");
            -- for i in 0 to 30 loop 
            --   wait until rising_edge(s_clk);
            --   report("waited");
            -- end loop; -- Waiter
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_C_on);
            -- report("set c_blinky_C_on");
            -- for i in 0 to 30 loop 
            --   wait until rising_edge(s_clk);
            --   report("waited");
            -- end loop; -- Waiter
            -- wait until rising_edge(s_clk);
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_on);
            -- report("set c_blinky_on");
            -- for i in 0 to 30 loop
            --   wait until rising_edge(s_clk);
            --   report("waited");
            -- end loop; -- Waiter
            -- wait until rising_edge(s_clk);
            -- --
            -- -- test SINGLE WRITE END
            -- --
            -- -- test SINGLE WRITE TURN LED OFF
            -- --
            -- wait until rising_edge(s_clk);
            -- s_wb_master_out  <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_off);
            -- wait until rising_edge(s_clk);
            -- --
            -- -- test SINGLE WRITE END
            -- --
      end process;

end architecture;





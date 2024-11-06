--wishbone slave testbench
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
  constant c_light_time   : time  := 320 ns;

  constant c_blinky_adr   : std_logic_vector(31 downto 0) := x"00000000";
  constant c_blinky_on    : std_logic_vector(31 downto 0) := x"00000001";
  constant c_blinky_off   : std_logic_vector(31 downto 0) := x"00000000";
  constant c_blinky_B_on  : std_logic_vector(31 downto 0) := x"00000011";
  constant c_blinky_C_on  : std_logic_vector(31 downto 0) := x"00000101";

  -- Other constants
  constant c_reg_all_zero                : std_logic_vector(31 downto 0) := x"00000000";
  constant c_cyc_on                      : std_logic := '1';
  constant c_cyc_off                     : std_logic := '0';
  constant c_str_on                      : std_logic := '1';
  constant c_str_off                     : std_logic := '0';
  constant c_we_on                       : std_logic := '1';
  constant c_we_off                      : std_logic := '0';
  
  -- Basic device signals
  signal s_clk        : std_logic := '0';
  signal s_clk_enable : boolean := false;
  signal s_rst_n      : std_logic := '0';
  signal s_rst        : std_logic := '0';
  
  -- Wishbone connections
  signal s_wb_slave_in  : t_wishbone_slave_in;
  signal s_wb_slave_out : t_wishbone_slave_out;

  -- Testbench logic
  signal s_ack            : std_logic             := '0';
  signal s_led_state      : std_logic             := '0';
  
    -- Function wb_stim -> Helper function to create a human-readable testbench
  function wb_stim(cyc : std_logic; stb : std_logic; we : std_logic;
                   adr : t_wishbone_address; dat : t_wishbone_data) return t_wishbone_slave_in is
  variable v_setup : t_wishbone_slave_in;
  begin
    v_setup.cyc := cyc;
    v_setup.stb := stb;
    v_setup.we  := we;
    v_setup.adr := adr;
    v_setup.dat := dat;
    v_setup.sel := (others => '0'); -- Don't care
    return v_setup;
  end function wb_stim;

  -- Procedures
  -- Procedure wb_expect -> Check WB slave answer
  procedure wb_expect(msg : string; dat_from_slave : t_wishbone_data; compare_value : t_wishbone_data) is
  begin
    if (to_integer(unsigned(dat_from_slave)) = to_integer(unsigned(compare_value))) then
      report "Test passed: " & msg;
    else
      report "Test failed: " & msg;
      report "-> Info:  Answer from slave:          " & integer'image(to_integer(unsigned(dat_from_slave)));
      report "-> Error: Expected answer from slave: " & integer'image(to_integer(unsigned(compare_value)));
    end if;
  end procedure wb_expect;

  component blinky is

    port(
    s_clk_sys_i       : in std_logic;
    s_rst_sys_i       : in std_logic;

    t_wb_out          : out t_wishbone_slave_out;
    t_wb_in           : in  t_wishbone_slave_in;

    s_led_o           : out std_logic
    );

  end component;

  begin

    dut : blinky
      port map (
        s_clk_sys_i     => s_clk,
        s_rst_sys_i     => s_rst_n,

        t_wb_out        => s_wb_slave_out,
        t_wb_in         => s_wb_slave_in,

        s_led_o         => s_led_state);


    -- generate clock
    p_clock : process
      begin
        s_clk <= '0';
        wait for c_clock_cycle/2;
        s_clk <= '1' and s_rst_n;
        wait for c_clock_cycle/2;
      end process;

    -- Reset controller
    p_reset : process
    begin
        wait for c_reset_time;
        s_rst_n <= '1';
    end process;
    s_rst <= not(s_rst_n);
    -- test process

    p_test_led : process
      begin
        report("Test started!");
        -- Reset
        report("Reset");
        s_wb_slave_in <= wb_stim(c_cyc_off, c_str_off, c_we_on, c_reg_all_zero, c_reg_all_zero);
        wait until rising_edge(s_rst_n);
        report("Reset End");
        -- Try read
        wait until rising_edge(s_clk); 
        s_wb_slave_in <= wb_stim(c_cyc_on,  c_str_on,  c_we_off, c_blinky_adr, c_blinky_on);
        report("Sent READ: LED_ON");
        wait for c_light_time / 2;
        if (s_led_state = '1') then
          report("LED is on");
        end if;
        wait for c_light_time / 2;
        -- Try read
        wait until rising_edge(s_clk); 
        s_wb_slave_in <= wb_stim(c_cyc_on,  c_str_on,  c_we_on, c_blinky_adr, c_blinky_off);
        report("Sent READ: LED_OFF");
        wait for c_light_time / 2;
        if (s_led_state = '0') then
          report("LED is off");
        end if;
        wait for c_light_time / 2;
        -- Try read
        wait until rising_edge(s_clk); 
        s_wb_slave_in <= wb_stim(c_cyc_on,  c_str_on,  c_we_on, c_blinky_adr, c_blinky_on);
        report("Sent READ: LED_ON");
        wait for c_light_time / 2;
        if (s_led_state = '1') then
          report("LED is on");
        end if;
        wait for c_light_time / 2;
        -- Try write
        wait until rising_edge(s_clk);
        s_wb_slave_in <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_adr, c_blinky_B_on);
        report("Sent WRITE: MODE_B_ON");
        wait for c_light_time;
        wait until rising_edge(s_clk);
        -- Try write
        wait until rising_edge(s_clk);
        s_wb_slave_in <= wb_stim(c_cyc_on, c_str_on, c_we_on, c_blinky_adr, c_blinky_C_on);
        report("Sent WRITE: MODE_C_ON");
        wait for c_light_time;
        wait until rising_edge(s_clk);
        -- turn it of again
        s_wb_slave_in <= wb_stim(c_cyc_on,  c_str_on,  c_we_on, c_blinky_adr, c_reg_all_zero);
        wait for c_light_time;
        wait until rising_edge(s_clk);

      end process;

end architecture;





library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;


entity blinky is


    generic (
        g_simulation    : in boolean := false
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

    s_led_o           : out std_logic
    );

end entity;

architecture blinky_arch of blinky is

    signal s_ack_state        : std_logic := '0';
    signal s_led_state        : std_logic := '0';

    signal s_stall_state      : std_logic := '0';
    signal s_retry_state      : std_logic := '0';
    signal s_error_state      : std_logic := '0';

    signal s_state_machine_vector : std_logic_vector(3 downto 0) := "0000";

    signal s_led_freq_ctl   : std_logic := '0';

    signal  s_blinky_mode_v   : std_logic_vector(2 downto 0) := (others => '0');

    -- LED static on
    constant c_blinky_mode_A        : std_logic_vector(2 downto 0) := "000";
    constant s_blinky_mode_A_toggle : std_logic := '1';

    -- LED blinking
    constant c_blinky_mode_B        : std_logic_vector(2 downto 0) := "100";
    signal   i_blinky_mode_B        : integer   :=  40000000;
    signal   s_counter_B            : integer   :=  1;
    signal   s_blinky_mode_B_toggle : std_logic := '0';

    -- LED blinking
    constant c_blinky_mode_C        : std_logic_vector(2 downto 0) := "111";
    signal   i_blinky_mode_C        : integer   :=  10000000;
    signal   s_counter_C            : integer   :=  1;
    signal   s_blinky_mode_C_toggle : std_logic := '0';

    -- all vectors are downto-range positions are 3210
    constant mode_write   : std_logic_vector(3 downto 0) := "1111";
    constant mode_read    : std_logic_vector(3 downto 0) := "0111";

begin

    -- change values if in a simulation
    g_set_sim_values : if g_simulation generate
        i_blinky_mode_B <=  10;
        i_blinky_mode_C <=  20;
    end generate g_set_sim_values;
 

    --  for now no errors, no stalling
    t_wb_out.err    <= s_error_state;
    t_wb_out.stall  <= s_stall_state;
    t_wb_out.rty    <= s_retry_state;

    t_wb_out.ack <= s_ack_state;
    s_led_o <= s_led_state and s_led_freq_ctl;


    -- fill the pseudo state machine
    s_state_machine_vector(0) <= s_rst_sys_n_i;
    s_state_machine_vector(1) <= t_wb_in.cyc;
    s_state_machine_vector(2) <= t_wb_in.stb;
    s_state_machine_vector(3) <= t_wb_in.we;

    p_wb_write: process(s_clk_sys_i)
    begin
        if rising_edge(s_clk_sys_i) then
            if s_rst_sys_n_i = '0' then
                s_stall_state   <= '0';
                s_error_state   <= '0';
                s_retry_state   <= '0';
            elsif s_state_machine_vector = mode_write then
                s_led_state <= t_wb_in.dat(0);
                s_blinky_mode_v <= t_wb_in.dat(3 downto 1);
                case s_blinky_mode_v is
                    when c_blinky_mode_A =>
                        s_led_freq_ctl <= s_blinky_mode_A_toggle;
                        report("s_led_freq_ctl <= s_blinky_mode_A_toggle");
                    when c_blinky_mode_B =>
                        s_led_freq_ctl <= s_blinky_mode_B_toggle;
                        report("s_led_freq_ctl <= s_blinky_mode_B_toggle");
                    when c_blinky_mode_C =>
                        s_led_freq_ctl <= s_blinky_mode_C_toggle;
                        report("s_led_freq_ctl <= s_blinky_mode_C_toggle");
                    when others => -- unrecognised turns LED off
                        s_led_freq_ctl <= '0';
                        s_error_state <= '1';
                end case;
            end if;
        end if;
    end process;

    p_wb_read: process(s_clk_sys_i)
    begin
        if rising_edge(s_clk_sys_i) then
            if s_rst_sys_n_i = '0' then
                t_wb_out.dat    <= (others => '0');
            elsif s_state_machine_vector = mode_read then
                t_wb_out.dat(0) <= s_led_state;
                t_wb_out.dat(3 downto 1) <= s_blinky_mode_v;
            end if;
        end if;
    end process;

    p_wb_ack: process(s_clk_sys_i)
    begin
        if rising_edge(s_clk_sys_i) then
            if s_rst_sys_n_i = '0' then
                s_ack_state <= '0';
            else
                if t_wb_in.stb = '1' and t_wb_in.cyc = '1' then
                    s_ack_state <= '1';
                else
                    s_ack_state <= '0';
                end if;
            end if;
        end if;
    end process;

    p_blinky_counter_B: process(s_clk_sys_i)
    begin
        if rising_edge(s_clk_sys_i) then
            if s_rst_sys_n_i = '0' then
                s_counter_B <= 0;
            elsif s_counter_B = i_blinky_mode_B then
                s_counter_B <= 0;
                s_blinky_mode_B_toggle <= not s_blinky_mode_B_toggle;
            else
                s_counter_B <= s_counter_B + 1;
            end if;
        end if;
    end process;

    p_blinky_counter_C: process(s_clk_sys_i)
    begin
        if rising_edge(s_clk_sys_i) then
            if s_rst_sys_n_i = '0' then
                s_counter_C <= 0;
            elsif s_counter_C = i_blinky_mode_C then
                s_counter_C <= 0;
                s_blinky_mode_C_toggle <= not s_blinky_mode_C_toggle;
            else
                s_counter_C <= s_counter_C + 1;
            end if;
        end if;
    end process;

end blinky_arch;
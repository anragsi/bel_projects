library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;


entity blinky is

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

    signal s_led_freq_ctl     : std_logic := '0';
    signal s_count            : integer   :=  1;
    signal s_compare_to       : integer   :=  4;
    signal s_freq_change_flag : std_logic := '0';

    signal  s_blinky_mode_v   : std_logic_vector(2 downto 0) := (others => '0');

    constant c_blinky_mode_A  : std_logic_vector(2 downto 0) := "000";
    constant i_blinky_mode_A  : integer   :=  1;
    constant c_blinky_mode_B  : std_logic_vector(2 downto 0) := "111";
    constant i_blinky_mode_B  : integer   :=  15000000;
    constant c_blinky_mode_C  : std_logic_vector(2 downto 0) := "101";
    constant i_blinky_mode_C  : integer   :=  30000000;

    -- all vectors are downto-range positions are 3210
    constant mode_write   : std_logic_vector(3 downto 0) := "1111";
    constant mode_read    : std_logic_vector(3 downto 0) := "0111";

    constant mode_reset_0 : std_logic_vector(3 downto 0) := "0000";
    constant mode_reset_1 : std_logic_vector(3 downto 0) := "0010";
    constant mode_reset_2 : std_logic_vector(3 downto 0) := "0100";
    constant mode_reset_3 : std_logic_vector(3 downto 0) := "0110";
    constant mode_reset_4 : std_logic_vector(3 downto 0) := "1000";
    constant mode_reset_5 : std_logic_vector(3 downto 0) := "1010";
    constant mode_reset_6 : std_logic_vector(3 downto 0) := "1110";

begin
  
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
                --s_blinky_mode_v(0) <= t_wb_in.dat(4 downto 1);
                s_blinky_mode_v(0) <= t_wb_in.dat(1);
                s_blinky_mode_v(1) <= t_wb_in.dat(2);
                s_blinky_mode_v(2) <= t_wb_in.dat(3);
                    case s_blinky_mode_v is
                        when c_blinky_mode_A =>
                            s_compare_to <= i_blinky_mode_A;
                        when c_blinky_mode_B =>
                            s_compare_to <= i_blinky_mode_B;
                        when c_blinky_mode_C =>
                            s_compare_to <= i_blinky_mode_C;
                        when others =>
                            s_compare_to <= i_blinky_mode_A;
                    end case;
                --report("inside write: 1111");
                --report std_logic'image(t_wb_in.dat(0));
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
                --report("inside read: 0111");
                --report std_logic'image(s_led_state);
            end if;
        end if;
    end process;

    p_wb_ack: process(s_clk_sys_i)
    begin
        if rising_edge(s_clk_sys_i) then
            if s_rst_sys_n_i = '0' then
                s_ack_state <= '0';
            else
                --report("inside ack else");
                --report std_logic'image(t_wb_in.stb);
                --report std_logic'image(t_wb_in.cyc);
                if t_wb_in.stb = '1' and t_wb_in.cyc = '1' then
                    --report("inside ack -> 1");
                    s_ack_state <= '1';
                else
                    s_ack_state <= '0';
                end if;
            end if;
        end if;
    end process;

    p_blinky_counter: process(s_clk_sys_i)
    begin
        if rising_edge(s_clk_sys_i) then
            if s_rst_sys_n_i = '0' then
                s_count <= 0;
            elsif s_count = s_compare_to then
                s_count <= 1;
                s_led_freq_ctl <= not s_led_freq_ctl;
            else
                s_count <= s_count + 1;
            end if;
        end if;
    end process;
            
end blinky_arch;
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.wishbone_pkg.all;


entity blinky is

    port(
    
    -- these two sys signal come from SysCon
    s_clk_sys_i       : in std_ulogic;
    s_rst_sys_n_i     : in std_ulogic;


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

    --signal s_rst_sys_n        : std_logic := '0';
    signal s_led_state        : std_logic := '0';
    signal s_stall_state      : std_logic := '0';

    signal s_state_machine_vector : std_logic_vector(3 downto 0) := "0000";

    --signal s_led_freq_ctl     : std_logic := '0';
    --signal s_count            : integer   :=  1;
    --signal s_compare_to       : integer   :=  1;

    --signal  s_blinky_mode_v   : std_logic_vector(2 downto 0);

    --constant s_blinky_mode_A  : std_logic_vector(2 downto 0) := "000";
    --constant s_blinky_mode_B  : std_logic_vector(2 downto 0) := "001";
    --constant s_blinky_mode_C  : std_logic_vector(2 downto 0) := "100";

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
    t_wb_out.err    <= '0';
    t_wb_out.stall  <= s_stall_state;
    t_wb_out.rty    <= '0';

    t_wb_out.ack <= t_wb_in.stb and t_wb_in.cyc;

    s_led_o <= s_led_state;
    --s_led_o <= s_led_state and s_led_freq_ctl;


    -- fill the stupid pseudo state machine
    s_state_machine_vector(0) <= s_rst_sys_n_i;
    s_state_machine_vector(1) <= t_wb_in.cyc;
    s_state_machine_vector(2) <= t_wb_in.stb;
    s_state_machine_vector(3) <= t_wb_in.we;

    -- single read write cycle
    p_wb_read_write: process(s_clk_sys_i)

    begin

        if rising_edge(s_clk_sys_i) then --WHY IS THIS BREAKING EVERYTHING???

        case s_state_machine_vector is

                when mode_reset_0 | mode_reset_1 | mode_reset_2 | mode_reset_3 | mode_reset_4 | mode_reset_5 | mode_reset_6 =>

                    -- RESET

                    t_wb_out.dat    <= (others => '0');

                    s_led_state     <= '0';
                    s_stall_state   <= '0';

                    report("reset");
                    --report std_logic'image(s_rst_sys_n_i);
                    --report std_logic'image(s_state_machine_vector(0));
                    --report std_logic'image(mode_reset_0(0));

                    --s_blinky_mode_v   <= (others => '0');
                    --s_led_freq_ctl  <= '1';
                    --s_count         <=  1;
                    --s_compare_to    <=  15; --15000
                    --end if; --rising_edge(s_clk_sys_i)

                when mode_write =>

                    
                    s_led_state <= t_wb_in.dat(0);
                    report("inside write: 1111");
                    report std_logic'image(t_wb_in.dat(0));
                    report std_logic'image(t_wb_in.dat(31));
                    
      
                -- END WRITE

                when mode_read =>

                    t_wb_out.dat(0) <= s_led_state;
                    report("inside read: 0111");
          
                -- END READ

                when others => 
                    --report("Fuck VHDL");
                    --report std_logic'image(t_wb_in.cyc);
                    --report std_logic'image(t_wb_in.dat(0));

            end case;

        end if;

    end process;
            
end blinky_arch;
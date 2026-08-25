-----------------------------------------------------------------------------
--  LEON3 Demonstration design test bench
--  Copyright (C) 2016 Cobham Gaisler
------------------------------------------------------------------------------
--  This file is a part of the GRLIB VHDL IP LIBRARY
--  Copyright (C) 2003 - 2008, Gaisler Research
--  Copyright (C) 2008 - 2014, Aeroflex Gaisler
--  Copyright (C) 2015 - 2018, Cobham Gaisler
--
--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation; either version 2 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with this program; if not, write to the Free Software
--  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
library gaisler;
use gaisler.sim.all;
library techmap;
use techmap.gencomp.all;
use work.debug.all;

use work.config.all;

use ieee.std_logic_textio.all;
use std.textio.all;

use work.config.all;

library grlib;
use grlib.amba.all;
use grlib.stdlib.all;
use grlib.devices.all;
entity testbench is
  generic (
    fabtech   : integer := CFG_FABTECH;
    memtech   : integer := CFG_MEMTECH;
    padtech   : integer := CFG_PADTECH;
    clktech   : integer := CFG_CLKTECH;
    disas     : integer := CFG_DISAS;   -- Enable disassembly to console
    dbguart   : integer := CFG_DUART;   -- Print UART on console
    pclow     : integer := CFG_PCLOW;
    USE_MIG_INTERFACE_MODEL : boolean := true; --false;
    clkperiod : integer := 20           -- system clock period
    );
end;

architecture behav of testbench is
  constant promfile  : string  := "prom.srec";      -- rom contents
  constant sdramfile : string  := "ram.srec";       -- sdram contents

  constant ct       : integer := clkperiod/2;

  -- MIG Simulation parameters
  constant SIM_BYPASS_INIT_CAL : string := "FAST";
          -- # = "OFF" -  Complete memory init &
          --               calibration sequence
          -- # = "SKIP" - Not supported
          -- # = "FAST" - Complete memory init & use
          --              abbreviated calib sequence

  constant SIMULATION          : string := "TRUE";
          -- Should be TRUE during design simulations and
          -- FALSE during implementations

  signal CLK50MHZ          : std_ulogic := '0';
  -- LEDs
  signal led                : std_logic_vector(1 downto 0);
  -- Buttons
  signal ck_rst             : std_ulogic;
  signal btn                : std_logic_vector(1 downto 0);
  signal cpu_resetn         : std_ulogic;
  -- Switches
  signal sw                 : std_logic_vector(3 downto 0);    
  -- PMOD
  signal jabcd              : std_logic_vector(31 downto 0);
  -- Arduino/ChipKit SPI
  signal ck_miso            : std_ulogic;
  signal ck_mosi            : std_ulogic;
  -- USB-RS232 interface
  signal uart_tx_in         : std_logic;
  signal uart_rx_out        : std_logic;
  -- DDR3
  signal ddr3_dq            : std_logic_vector(15 downto 0);
  signal ddr3_dqs_p         : std_logic_vector(1 downto 0);
  signal ddr3_dqs_n         : std_logic_vector(1 downto 0);
  signal ddr3_addr          : std_logic_vector(14 downto 0);
  signal ddr3_ba            : std_logic_vector(2 downto 0);
  signal ddr3_ras_n         : std_logic;
  signal ddr3_cas_n         : std_logic;
  signal ddr3_we_n          : std_logic;
  signal ddr3_reset_n       : std_logic;
  signal ddr3_ck_p          : std_logic_vector(0 downto 0);
  signal ddr3_ck_n          : std_logic_vector(0 downto 0);
  signal ddr3_cke           : std_logic_vector(0 downto 0);
  signal ddr3_dm            : std_logic_vector(1 downto 0);
  signal ddr3_odt           : std_logic_vector(0 downto 0);
  -- Fan PWM
  signal fan_pwm            : std_ulogic;    
  -- SPI
  signal qspi_sck           : std_ulogic;
  signal qspi_cs            : std_logic;
  signal qspi_dq            : std_logic_vector(3 downto 0);

  signal gnd                : std_ulogic;

  signal eref_clk   : std_ulogic;
  signal etx_clk    : std_ulogic;
  signal erx_clk    : std_ulogic;
  signal erxdt      : std_logic_vector(3 downto 0);
  signal erx_dv     : std_ulogic;
  signal erx_er     : std_ulogic;
  signal erx_col    : std_ulogic;
  signal erx_crs    : std_ulogic;
  signal etxdt      : std_logic_vector(3 downto 0);
  signal etx_en     : std_ulogic;
  signal etx_er     : std_ulogic;
  signal emdc       : std_ulogic;
  signal emdio      : std_logic;
  signal erstn      : std_logic;
  
  signal ck_rsth    : std_logic;
  signal baud_clk_posedge_wire: std_logic;
  signal uart_REC_dataH: std_logic;
  signal rec_readyH: std_logic;
  signal rec_dataH: std_logic_vector(7 downto 0);

begin

  gnd <= '0';

  -- clock and reset
  CLK50MHZ     <= not CLK50MHZ after ct * 1 ns;
  -- reset
  ck_rst        <= '0', '1' after 100 ns;
  ck_rsth       <= not ck_rst;
  -- dsui.break
  btn(1)        <= '0';
  btn(0)        <= ck_rst;
  -- dsui.enable
  sw(3)         <= '0';

  jabcd <= (others => 'H');
  ck_miso <= ck_mosi;

  d3 : entity work.leon3mp
    generic map (fabtech, memtech, padtech, clktech, disas, dbguart, pclow,
                 SIM_BYPASS_INIT_CAL, SIMULATION, USE_MIG_INTERFACE_MODEL, TRUE)
    port map (
      CLK50MHZ => CLK50MHZ, led => led,
      btn => btn,
      uart_txd_in => '1',
      uart_rxd_out => uart_REC_dataH,
      sddat0 => '1'
     );

        rxd1: entity work.u_rec_of_verifla(u_rec_of_verifla_arch)
                port map(clk_i => CLK50MHZ, rst_i => ck_rsth, baud_clk_posedge => baud_clk_posedge_wire, 
                        rxd_i => uart_REC_dataH, rdy_o => rec_readyH, data_o => rec_dataH);

        baud1: entity work.baud_of_verifla(baud_of_verifla_arch)
                port map(sys_clk => CLK50MHZ, sys_rst_l => ck_rst, baud_clk_posedge => baud_clk_posedge_wire);


  spif : if CFG_SPIMCTRL /= 0 generate
    spi0: spi_flash
      generic map (
        ftype      => 4,
        debug      => 1,
        fname      => promfile,
        readcmd    => CFG_SPIMCTRL_READCMD,
        dummybyte  => CFG_SPIMCTRL_DUMMYBYTE,
        dualoutput => CFG_SPIMCTRL_DUALOUTPUT,
        memoffset  => CFG_SPIMCTRL_OFFSET)
      port map (
        sck             => qspi_sck,
        di              => qspi_dq(0),
        do              => qspi_dq(1),
        csn             => qspi_cs,
        sd_cmd_timeout  => open,
        sd_data_timeout => open);
  end generate;

  iuerr : process
  begin
    wait for 1000 us;
    report "***" & tost(USE_MIG_INTERFACE_MODEL);
    --assert (to_X01(led(1)) = '0')
      --report "*** IU in error mode, simulation halted ***"
      --severity failure;  
  end process;

end;


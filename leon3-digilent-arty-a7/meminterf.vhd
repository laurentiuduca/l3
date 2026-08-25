-- laurentiu cristian duca
-- gnu gpl

library ieee;
use ieee.std_logic_1164.all;
use IEEE.NUMERIC_STD.ALL;

use work.config.all;

--pragma translate_off
use ieee.std_logic_textio.all;
use std.textio.all;
--pragma translate_on

library grlib;
use grlib.amba.all;
use grlib.stdlib.all;
use grlib.devices.all;

entity meminterf is
    generic(
        ramaddr                   : integer := 16#400#;
        romaddr                   : integer := 16#000#;
        rammask                   : integer := 16#f00#;
        SIMULATION                : string := "FALSE"
    );
    port (
        clk         : in std_logic;
        rstn        : in std_logic;
        w_init_done : out std_logic := '0';
        ahbsi : out ahb_slv_in_type;
        ahbso : in ahb_slv_out_type;
        ahbsiproc : in ahb_slv_in_type;
        ahbsorom : in ahb_slv_out_type;
        sdmig_rd_en : in std_logic;
        sdmig_wr_en : in std_logic;
        sdmig_init_done: in std_logic;
        sdmig_obusy: out std_logic := '0';
        sdmig_addr : in std_logic_vector(31 downto 0);
        sdmig_data : in std_logic_vector(31 downto 0);
        sdmig_odata : out std_logic_vector(31 downto 0)
    );
end;

architecture miarh of meminterf is

  constant notwait0 : boolean := true;

  procedure printahbsi(ahbsi : in ahb_slv_in_type) is
  begin
    report "ahbsi=" &
        " hsel " & tost(ahbsi.hsel) & " haddr " & tost(ahbsi.haddr) & " hwrite " & tost(ahbsi.hwrite) & 
        " htrans " & tost(ahbsi.htrans) & " hsize " & tost(ahbsi.hsize) & " hburst " & tost(ahbsi.hburst) & 
        " hwdata " & tost(ahbsi.hwdata) & " hprot " & tost(ahbsi.hprot) & " hready " & tost(ahbsi.hready) & 
        " hmaster " & tost(ahbsi.hmaster) & " hmastlock " & tost(ahbsi.hmastlock) & " hmbsel " & tost(ahbsi.hmbsel) &
        " endian "  & tost(ahbsi.endian);
  end;
  procedure printahbso(ahbso : in ahb_slv_out_type) is
  begin
    report "ahbso=" &
        " hready " & tost(ahbso.hready) & " hresp " & tost(ahbso.hresp) & " hrdata " & tost(ahbso.hrdata) &
        " hsplit " & tost(ahbso.hsplit) & " hirq " & tost(ahbso.hirq) & --" hconfig " & tost(ahbso.hconfig) &
        " hindex " & tost(ahbso.hindex);
  end;
  signal state, retstate : std_logic_vector(7 downto 0) := x"00";
  signal wrdata : std_logic_vector(31 downto 0) := X"88100000";
  signal rddata : std_logic_vector(31 downto 0) := (others => '0');
  signal ahbsip : ahb_slv_in_type;
begin

  miwrtest: process (clk)
  begin
    if rising_edge(clk) then
    if ((rstn = '0')) then
        state <= x"00";
        retstate <= x"00";
        ahbsi <= ahbs_in_none;
        w_init_done <= '0';
        sdmig_obusy <= '0';
        ahbsi.hsel <= (5 => '1', others => '0');
        ahbsi.haddr <= (others => '0');
        ahbsi.hwdata <= (others => '0');
        ahbsi.htrans <= HTRANS_NONSEQ;
        ahbsi.hsize <= HSIZE_WORD;
        ahbsi.hburst <= HBURST_SINGLE;
        ahbsi.hprot <= "0011"; -- privileged data access, non-cacheable, non-bufferable
        ahbsi.hmaster <= (others => '0');
        ahbsi.endian <= '0';
        ahbsi.hready <= '0';
    else
    case state is
    when x"00" =>
        if sdmig_init_done = '1' then
            state <= x"37";
            report "sdmig_init_done = '1'";
        elsif sdmig_rd_en = '1' then
            sdmig_obusy <= '1';
            ahbsi.haddr <= (std_logic_vector(to_unsigned(ramaddr, 12)) & x"00000") or sdmig_addr;
            ahbsi.hwrite <= '0';
            ahbsi.hready <= '1';
            state <= x"03";
        elsif sdmig_wr_en = '1' then
            sdmig_obusy <= '1';
            ahbsi.haddr <= (std_logic_vector(to_unsigned(ramaddr, 12)) & x"00000") or sdmig_addr;
            ahbsi.hwdata <= sdmig_data;
            ahbsi.hwrite <= '1';
            ahbsi.hready <= '1';
            state <= x"11";            
        end if;
    when x"11" =>
        -- write
        report "state 11";
        ahbsi.hready <= '1';
        ahbsi.hwrite <= '0';
        state <= x"12";
    when x"12" =>
        ahbsi.hready <= '0';
        ahbsi.hwrite <= '0';
        report "state 12";
        if(notwait0 = TRUE) then
            state <= x"13";
        else
            state <= x"26";
            retstate <= x"13";
        end if;
    when x"13" =>
            report "state 13";
            if ahbso.hready = '1' then
                sdmig_obusy <= '0';
                state <= x"00";
            end if;
    when x"03" => 
        -- read
        report "state 3";
        ahbsi.hwrite <= '0';
        ahbsi.hready <= '0';
        if(notwait0 = TRUE) then
            state <= x"05";
        else
            state <= x"26";
            retstate <= x"05";
        end if;
    when x"26" =>
        if ahbso.hready = '0' then
            state <= retstate;
        end if;        
    when x"05" =>
        ahbsi.hready <= '0';
        if ahbso.hready = '1' then
            sdmig_odata <= ahbso.hrdata;
            sdmig_obusy <= '0';
            report "out=" & tost(ahbso.hrdata);
            state <= x"00";
        end if;
   when x"37" =>
            -- give processor right
            ahbsi <= ahbsip;
            state <= x"38";
    when x"38" =>
            ahbsi.hready <= '0';
            state <= x"39";
            w_init_done <= '1';
    when x"39" =>
            --printahbso(ahbso);
            --printahbsi(ahbsiproc);
            state <= x"3a";
    when others => null;
    end case;
    end if;
    end if;
  end process;


  miproc: process (clk)
  begin 
    if rising_edge(clk) then
    if ahbsiproc.haddr = (std_logic_vector(to_unsigned(ramaddr, 12)) & x"00000") and ahbsiproc.hready = '1' then
       ahbsip <= ahbsiproc;
       report "ahbsiproc.haddr = " & tost(ahbsiproc.haddr);
       printahbsi(ahbsiproc);
    end if;    
    end if;
  end process;
end;


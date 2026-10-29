
----------------------------------------------------------------------------
----------------------------------------------------------------------------
----------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
library grlib;
use grlib.amba.all;
use grlib.stdlib.all;
use grlib.devices.all;
use grlib.config_types.all;
use grlib.config.all;

entity winitdone is
  generic (
    hindex  : integer := 0;
    haddr   : integer := 0;
    hmask   : integer := 16#fff#;
    pipe    : integer := 0;
    tech    : integer := 0;
    kbytes  : integer := 1); 
  port (
    w_init_done : in std_logic;
    rst     : in  std_ulogic;
    clk     : in  std_ulogic;
    ahbsi   : in  ahb_slv_in_type;
    ahbso   : out ahb_slv_out_type
  );
end;

architecture rtl of winitdone is
  signal romdata : std_logic_vector(31 downto 0);
constant hconfig : ahb_config_type := (
  0 => ahb_device_reg ( VENDOR_GAISLER, GAISLER_GRTESTMOD, 0, 0, 0),
  4 => ahb_membar(haddr, '0', '0', hmask),
  others => zero32);

begin

  ahbso.hresp   <= "00";
  ahbso.hsplit  <= (others => '0');
  ahbso.hirq    <= (others => '0');
  ahbso.hconfig <= hconfig;
  ahbso.hindex  <= hindex;

    romdata <= (others => w_init_done);
    ahbso.hrdata  <= ahbdrivedata(romdata);
    ahbso.hready  <= '1';

  -- pragma translate_off
  bootmsg : report_version
  generic map ("winitdone " & tost(hindex) );
  -- pragma translate_on
end;



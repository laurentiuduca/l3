#
# System signal
#
set_property PACKAGE_PIN G20 [get_ports w_led[1]]
set_property IOSTANDARD LVCMOS33 [get_ports w_led[1]]
set_property PACKAGE_PIN G21 [get_ports w_led[0]]
set_property IOSTANDARD LVCMOS33 [get_ports w_led[0]]
set_property PACKAGE_PIN M21 [get_ports clk]
set_property IOSTANDARD LVCMOS33 [get_ports clk]
create_clock -add -name sys_clk_pin -period 20.00 [get_ports { clk }];
#create_generated_clock -add -name sys_clk -period 20 [get_pins mmcmclock/clk_sys]
set_property IOSTANDARD LVCMOS33 [get_ports w_btnl]
set_property PACKAGE_PIN H7 [get_ports w_btnl]
set_property IOSTANDARD LVCMOS33 [get_ports w_btnr]
set_property PACKAGE_PIN M6 [get_ports w_btnr]

set_property PACKAGE_PIN F3 [get_ports i_rx]
set_property IOSTANDARD LVCMOS33 [get_ports i_rx]
set_property PACKAGE_PIN E3 [get_ports o_tx]
set_property IOSTANDARD LVCMOS33 [get_ports o_tx]


## DDR3
# PadFunction: IO_L18N_T2_16

set_property SLEW FAST [get_ports {ddr3_dq[0]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[0]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[0]}]

set_property PACKAGE_PIN D21 [get_ports {ddr3_dq[0]}]



# PadFunction: IO_L16P_T2_16

set_property SLEW FAST [get_ports {ddr3_dq[1]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[1]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[1]}]

set_property PACKAGE_PIN C21 [get_ports {ddr3_dq[1]}]


# PadFunction: IO_L17P_T2_16

set_property SLEW FAST [get_ports {ddr3_dq[2]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[2]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[2]}]

set_property PACKAGE_PIN B22 [get_ports {ddr3_dq[2]}]



# PadFunction: IO_L16N_T2_16

set_property SLEW FAST [get_ports {ddr3_dq[3]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[3]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[3]}]

set_property PACKAGE_PIN B21 [get_ports {ddr3_dq[3]}]


# PadFunction: IO_L13P_T2_MRCC_16

set_property SLEW FAST [get_ports {ddr3_dq[4]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[4]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[4]}]

set_property PACKAGE_PIN D19 [get_ports {ddr3_dq[4]}]



# PadFunction: IO_L14P_T2_SRCC_16

set_property SLEW FAST [get_ports {ddr3_dq[5]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[5]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[5]}]

set_property PACKAGE_PIN E20 [get_ports {ddr3_dq[5]}]


# PadFunction: IO_L13N_T2_MRCC_16

set_property SLEW FAST [get_ports {ddr3_dq[6]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[6]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[6]}]

set_property PACKAGE_PIN C19 [get_ports {ddr3_dq[6]}]



# PadFunction: IO_L14N_T2_SRCC_16

set_property SLEW FAST [get_ports {ddr3_dq[7]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[7]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[7]}]

set_property PACKAGE_PIN D20 [get_ports {ddr3_dq[7]}]


# PadFunction: IO_L19N_T3_VREF_16

set_property SLEW FAST [get_ports {ddr3_dq[8]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[8]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[8]}]

set_property PACKAGE_PIN C23 [get_ports {ddr3_dq[8]}]



# PadFunction: IO_L24P_T3_16

set_property SLEW FAST [get_ports {ddr3_dq[9]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[9]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[9]}]

set_property PACKAGE_PIN D23 [get_ports {ddr3_dq[9]}]


# PadFunction: IO_L23N_T3_16

set_property SLEW FAST [get_ports {ddr3_dq[10]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[10]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[10]}]

set_property PACKAGE_PIN B24 [get_ports {ddr3_dq[10]}]



# PadFunction: IO_L20P_T3_16

set_property SLEW FAST [get_ports {ddr3_dq[11]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[11]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[11]}]

set_property PACKAGE_PIN B25 [get_ports {ddr3_dq[11]}]


# PadFunction: IO_L23P_T3_16

set_property SLEW FAST [get_ports {ddr3_dq[12]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[12]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[12]}]

set_property PACKAGE_PIN C24 [get_ports {ddr3_dq[12]}]



# PadFunction: IO_L22P_T3_16

set_property SLEW FAST [get_ports {ddr3_dq[13]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[13]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[13]}]

set_property PACKAGE_PIN C26 [get_ports {ddr3_dq[13]}]


# PadFunction: IO_L20N_T3_16

set_property SLEW FAST [get_ports {ddr3_dq[14]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[14]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[14]}]

set_property PACKAGE_PIN A25 [get_ports {ddr3_dq[14]}]



# PadFunction: IO_L22N_T3_16

set_property SLEW FAST [get_ports {ddr3_dq[15]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dq[15]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dq[15]}]

set_property PACKAGE_PIN B26 [get_ports {ddr3_dq[15]}]


# PadFunction: IO_L4P_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[13]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[13]}]

set_property PACKAGE_PIN G15 [get_ports {ddr3_addr[13]}]



# PadFunction: IO_L12N_T1_MRCC_16

set_property SLEW FAST [get_ports {ddr3_addr[12]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[12]}]

set_property PACKAGE_PIN C18 [get_ports {ddr3_addr[12]}]



# PadFunction: IO_L1N_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[11]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[11]}]

set_property PACKAGE_PIN H15 [get_ports {ddr3_addr[11]}]



# PadFunction: IO_L5N_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[10]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[10]}]

set_property PACKAGE_PIN F20 [get_ports {ddr3_addr[10]}]



# PadFunction: IO_L4N_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[9]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[9]}]

set_property PACKAGE_PIN F15 [get_ports {ddr3_addr[9]}]



# PadFunction: IO_L1P_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[8]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[8]}]

set_property PACKAGE_PIN H14 [get_ports {ddr3_addr[8]}]



# PadFunction: IO_L8P_T1_16

set_property SLEW FAST [get_ports {ddr3_addr[7]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[7]}]

set_property PACKAGE_PIN E16 [get_ports {ddr3_addr[7]}]



# PadFunction: IO_L6P_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[6]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[6]}]

set_property PACKAGE_PIN H16 [get_ports {ddr3_addr[6]}]



# PadFunction: IO_L8N_T1_16

set_property SLEW FAST [get_ports {ddr3_addr[5]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[5]}]

set_property PACKAGE_PIN D16 [get_ports {ddr3_addr[5]}]



# PadFunction: IO_L6N_T0_VREF_16

set_property SLEW FAST [get_ports {ddr3_addr[4]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[4]}]

set_property PACKAGE_PIN G16 [get_ports {ddr3_addr[4]}]



# PadFunction: IO_L7P_T1_16

set_property SLEW FAST [get_ports {ddr3_addr[3]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[3]}]

set_property PACKAGE_PIN C17 [get_ports {ddr3_addr[3]}]



# PadFunction: IO_L2N_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[2]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[2]}]

set_property PACKAGE_PIN F17 [get_ports {ddr3_addr[2]}]



# PadFunction: IO_L2P_T0_16

set_property SLEW FAST [get_ports {ddr3_addr[1]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[1]}]

set_property PACKAGE_PIN G17 [get_ports {ddr3_addr[1]}]



# PadFunction: IO_L11P_T1_SRCC_16

set_property SLEW FAST [get_ports {ddr3_addr[0]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_addr[0]}]

set_property PACKAGE_PIN E17 [get_ports {ddr3_addr[0]}]



# PadFunction: IO_L9P_T1_DQS_16

set_property SLEW FAST [get_ports {ddr3_ba[2]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_ba[2]}]

set_property PACKAGE_PIN A17 [get_ports {ddr3_ba[2]}]



# PadFunction: IO_L12P_T1_MRCC_16

set_property SLEW FAST [get_ports {ddr3_ba[1]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_ba[1]}]

set_property PACKAGE_PIN D18 [get_ports {ddr3_ba[1]}]



# PadFunction: IO_L7N_T1_16

set_property SLEW FAST [get_ports {ddr3_ba[0]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_ba[0]}]

set_property PACKAGE_PIN B17 [get_ports {ddr3_ba[0]}]



# PadFunction: IO_L10N_T1_16

set_property SLEW FAST [get_ports ddr3_ras_n]

set_property IOSTANDARD SSTL135 [get_ports ddr3_ras_n]

set_property PACKAGE_PIN A19 [get_ports ddr3_ras_n]



# PadFunction: IO_L10P_T1_16

set_property SLEW FAST [get_ports ddr3_cas_n]

set_property IOSTANDARD SSTL135 [get_ports ddr3_cas_n]

set_property PACKAGE_PIN B19 [get_ports ddr3_cas_n]



# PadFunction: IO_L9N_T1_DQS_16

set_property SLEW FAST [get_ports ddr3_we_n]

set_property IOSTANDARD SSTL135 [get_ports ddr3_we_n]

set_property PACKAGE_PIN A18 [get_ports ddr3_we_n]



# PadFunction: IO_0_16

set_property SLEW FAST [get_ports ddr3_reset_n]

set_property IOSTANDARD SSTL135 [get_ports ddr3_reset_n]

set_property PACKAGE_PIN H17 [get_ports ddr3_reset_n]



# PadFunction: IO_L11N_T1_SRCC_16

set_property SLEW FAST [get_ports ddr3_cke]

set_property IOSTANDARD SSTL135 [get_ports ddr3_cke]

set_property PACKAGE_PIN E18 [get_ports ddr3_cke]



# PadFunction: IO_L5P_T0_16

set_property SLEW FAST [get_ports ddr3_odt]

set_property IOSTANDARD SSTL135 [get_ports ddr3_odt]

set_property PACKAGE_PIN G19 [get_ports ddr3_odt]



# PadFunction: IO_L17N_T2_16

set_property SLEW FAST [get_ports {ddr3_dm[0]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dm[0]}]

set_property PACKAGE_PIN A22 [get_ports {ddr3_dm[0]}]



# PadFunction: IO_L19P_T3_16

set_property SLEW FAST [get_ports {ddr3_dm[1]}]

set_property IOSTANDARD SSTL135 [get_ports {ddr3_dm[1]}]

set_property PACKAGE_PIN C22 [get_ports {ddr3_dm[1]}]



# PadFunction: IO_L15P_T2_DQS_16

set_property SLEW FAST [get_ports {ddr3_dqs_p[0]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dqs_p[0]}]

set_property IOSTANDARD DIFF_SSTL135 [get_ports {ddr3_dqs_p[0]}]




# PadFunction: IO_L15N_T2_DQS_16

set_property SLEW FAST [get_ports {ddr3_dqs_n[0]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dqs_n[0]}]

set_property IOSTANDARD DIFF_SSTL135 [get_ports {ddr3_dqs_n[0]}]

set_property PACKAGE_PIN B20 [get_ports {ddr3_dqs_p[0]}]
set_property PACKAGE_PIN A20 [get_ports {ddr3_dqs_n[0]}]



# PadFunction: IO_L21P_T3_DQS_16

set_property SLEW FAST [get_ports {ddr3_dqs_p[1]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dqs_p[1]}]

set_property IOSTANDARD DIFF_SSTL135 [get_ports {ddr3_dqs_p[1]}]




# PadFunction: IO_L21N_T3_DQS_16

set_property SLEW FAST [get_ports {ddr3_dqs_n[1]}]

set_property IN_TERM UNTUNED_SPLIT_50 [get_ports {ddr3_dqs_n[1]}]

set_property IOSTANDARD DIFF_SSTL135 [get_ports {ddr3_dqs_n[1]}]

set_property PACKAGE_PIN A23 [get_ports {ddr3_dqs_p[1]}]
set_property PACKAGE_PIN A24 [get_ports {ddr3_dqs_n[1]}]



# PadFunction: IO_L3P_T0_DQS_16

set_property SLEW FAST [get_ports ddr3_clk_p]

set_property IOSTANDARD DIFF_SSTL135 [get_ports ddr3_clk_p]




# PadFunction: IO_L3N_T0_DQS_16

set_property SLEW FAST [get_ports ddr3_clk_n]

set_property IOSTANDARD DIFF_SSTL135 [get_ports ddr3_clk_n]

set_property PACKAGE_PIN F18 [get_ports ddr3_clk_p]
set_property PACKAGE_PIN F19 [get_ports ddr3_clk_n]


set_property INTERNAL_VREF 0.675 [get_iobanks 16]
# set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]


# ## Place the IOSERDES_train manually (else the tool will place this blocks which can block the route for CLKB0 (OBUFDS for ddr3_clk_p))
# set_property LOC OLOGIC_X0Y91 [get_cells {ddr3_top/ddr3_phy_inst/genblk5[1].OSERDESE2_train}]
# set_property LOC ILOGIC_X0Y94 [get_cells {ddr3_top/ddr3_phy_inst/genblk5[0].ISERDESE2_train}]




#pmod J10, BANK35_

## ---- MAX7219 DIN (serial data input) ----
set_property PACKAGE_PIN G8 [get_ports {MAX7219_DATA}]
set_property IOSTANDARD LVCMOS33 [get_ports {MAX7219_DATA}]
set_property PULLDOWN FALSE [get_ports {MAX7219_DATA}]
set_property DRIVE 8 [get_ports {MAX7219_DATA}]
set_property SLEW SLOW [get_ports {MAX7219_DATA}]

## ---- MAX7219 CLK (serial clock) ----
set_property PACKAGE_PIN G5 [get_ports {MAX7219_CLK}]
set_property IOSTANDARD LVCMOS33 [get_ports {MAX7219_CLK}]
set_property PULLDOWN FALSE [get_ports {MAX7219_CLK}]
set_property DRIVE 8 [get_ports {MAX7219_CLK}]
set_property SLEW SLOW [get_ports {MAX7219_CLK}]

## ---- MAX7219 CS / LOAD (chip select) ----
set_property PACKAGE_PIN G7 [get_ports {MAX7219_LOAD}]
set_property IOSTANDARD LVCMOS33 [get_ports {MAX7219_LOAD}]
set_property PULLDOWN FALSE [get_ports {MAX7219_LOAD}]
set_property DRIVE 8 [get_ports {MAX7219_LOAD}]
set_property SLEW SLOW [get_ports {MAX7219_LOAD}]


# SDcard
#set_property -dict { PACKAGE_PIN E2    IOSTANDARD LVCMOS33 } [get_ports { sdcard_pwr_n }];   #IO_L14P_T2_SRCC_35 Sch=sd_resetn
#set_property -dict { PACKAGE_PIN N6    IOSTANDARD LVCMOS33 } [get_ports { sd_cd }];          #IO_L9N_T1_DQS_AD7N_35 Sch=sd_cd
set_property -dict { PACKAGE_PIN L4    IOSTANDARD LVCMOS33 } [get_ports { sdclk }];          #IO_L9P_T1_DQS_AD7P_35 Sch=sdclk
set_property -dict { PACKAGE_PIN J8    IOSTANDARD LVCMOS33 } [get_ports { sdcmd }];          #IO_L16N_T2_35 Sch=sdcmd mosi
set_property -dict { PACKAGE_PIN M5    IOSTANDARD LVCMOS33 } [get_ports { sddat0 }];         #IO_L16P_T2_35 Sch=sd_dat[0] miso
set_property -dict { PACKAGE_PIN M7    IOSTANDARD LVCMOS33 } [get_ports { sddat1 }];         #IO_L18N_T2_35 Sch=sd_dat[1]
set_property -dict { PACKAGE_PIN H6    IOSTANDARD LVCMOS33 } [get_ports { sddat2 }];         #IO_L18P_T2_35 Sch=sd_dat[2]
set_property -dict { PACKAGE_PIN J6    IOSTANDARD LVCMOS33 } [get_ports { sddat3 }];         #IO_L14N_T2_SRCC_35 Sch=sd_dat[3] cs

set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property CFGBVS VCCO [current_design]



## This file is a general .xdc for the Arty A7-100 Rev. D
## To use it in a project:
## - uncomment the lines corresponding to used pins
## - rename the used ports (in each line, after get_ports) according to the top level signal names in the project

## Clock signal
set_property -dict {PACKAGE_PIN M21 IOSTANDARD LVCMOS33} [get_ports CLK50MHZ]
create_clock -add -name sys_clk_pin -period 20.00 -waveform {0 10} [get_ports { CLK50MHZ }];
#create_clock -period 100.000 -name ahbjtaggen0.ahbjtag0/tap0/ac7v.u0/lltck -waveform {0.000 50.000} [get_pins ahbjtaggen0.ahbjtag0/tap0/ac7v.u0/u0/TCK]
create_clock -period 40.000 -name eth_rx_clk -waveform {0.000 20.000} [get_ports eth_rx_clk]
create_clock -period 40.000 -name eth_tx_clk -waveform {0.000 20.000} [get_ports eth_tx_clk]
#set_clock_groups -asynchronous -group [get_clocks clkm_clockers] -group [get_clocks ahbjtaggen0.ahbjtag0/tap0/ac7v.u0/lltck]
set_clock_groups -asynchronous -group [get_clocks eth_rx_clk] -group [get_clocks clkm_clockers]
set_clock_groups -asynchronous -group [get_clocks eth_tx_clk] -group [get_clocks clkm_clockers]
set_clock_groups -asynchronous -group [get_clocks clkm_clockers] -group [get_clocks eth_rx_clk]
set_clock_groups -asynchronous -group [get_clocks clkm_clockers] -group [get_clocks eth_tx_clk]
set_clock_groups -asynchronous -group [get_clocks clkm_clockers] -group [get_clocks clk_pll_i]
set_clock_groups -asynchronous -group [get_clocks clk_pll_i] -group [get_clocks clkm_clockers]
set_clock_groups -asynchronous -group [get_clocks clk_pll_i] -group [get_clocks eth_rx_clk]
set_clock_groups -asynchronous -group [get_clocks eth_rx_clk] -group [get_clocks clk_pll_i]
set_clock_groups -asynchronous -group [get_clocks clk_pll_i] -group [get_clocks eth_tx_clk]
set_clock_groups -asynchronous -group [get_clocks eth_tx_clk] -group [get_clocks clk_pll_i]
#set_clock_groups -asynchronous -group [get_clocks clk_pll_i] -group [get_clocks ahbjtaggen0.ahbjtag0/tap0/ac7v.u0/lltck]

#set_false_path -from [get_clocks ahbjtaggen0.ahbjtag0/tap0/ac7v.u0/lltck] -to [get_clocks clkm_clockers]

# ETH CDC
#set_property ASYNC_REG true [get_cells {ahbjtaggen0.ahbjtag0/newcom.jtagcom0/tnr1_reg[done_sync1]}]
#set_property ASYNC_REG true [get_cells {ahbjtaggen0.ahbjtag0/newcom.jtagcom0/tpr1_reg[done_sync]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[rxstart][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[rxstart][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[rxwrite][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[rxwrite][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[rxdone][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[rxdone][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[txread][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[txread][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[txrestart][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[txrestart][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[txdone][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/r_reg[txdone][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/rx_rmii0.rx0/gmiimode0.r_reg[write_ack][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/rx_rmii0.rx0/gmiimode0.r_reg[write_ack][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/rx_rmii0.rx0/gmiimode0.r_reg[done_ack][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/rx_rmii0.rx0/gmiimode0.r_reg[done_ack][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/rx_rmii0.rx0/rx_rst/r_reg[0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/rx_rmii0.rx0/rx_rst/r_reg[2]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/gmiimode0.r_reg[fullduplex][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/gmiimode0.r_reg[fullduplex][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/tx_rst/r_reg[0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/tx_rst/r_reg[2]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/gmiimode0.r_reg[start][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/gmiimode0.r_reg[start][1]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/gmiimode0.r_reg[read_ack][0]}]
set_property ASYNC_REG true [get_cells {eth0.e1/m100.u0/ethc0/tx_rmii0.tx0/gmiimode0.r_reg[read_ack][1]}]

## LEDs
set_property -dict {PACKAGE_PIN G21 IOSTANDARD LVCMOS33} [get_ports {led[0]}]
set_property -dict {PACKAGE_PIN G20 IOSTANDARD LVCMOS33} [get_ports {led[1]}]

## Buttons
set_property IOSTANDARD LVCMOS33 [get_ports btn[0]]
set_property PACKAGE_PIN H7 [get_ports btn[0]]
set_property IOSTANDARD LVCMOS33 [get_ports btn[1]]
set_property PACKAGE_PIN M6 [get_ports btn[1]]

## USB-UART Interface
set_property PACKAGE_PIN F3 [get_ports uart_txd_in]
set_property IOSTANDARD LVCMOS33 [get_ports uart_txd_in]
set_property PACKAGE_PIN E3 [get_ports uart_rxd_out]
set_property IOSTANDARD LVCMOS33 [get_ports uart_rxd_out]

## ---- TM1638 ----
set_property PACKAGE_PIN G8 [get_ports {tm_cs}] 
set_property IOSTANDARD LVCMOS33 [get_ports {tm_cs} ]
set_property PULLDOWN FALSE [get_ports {tm_cs}]
set_property DRIVE 8 [get_ports {tm_cs}]
set_property SLEW SLOW [get_ports {tm_cs}]

set_property PACKAGE_PIN G5 [get_ports {tm_dio}] 
set_property IOSTANDARD LVCMOS33 [get_ports {tm_dio}]
set_property PULLDOWN FALSE [get_ports {tm_dio}]
set_property DRIVE 8 [get_ports {tm_dio}]
set_property SLEW SLOW [get_ports {tm_dio}]

set_property PACKAGE_PIN G7 [get_ports {tm_clk}] 
set_property IOSTANDARD LVCMOS33 [get_ports {tm_clk}]
set_property PULLDOWN FALSE [get_ports {tm_clk}]
set_property DRIVE 8 [get_ports {tm_clk}]
set_property SLEW SLOW [get_ports {tm_clk}]

# SDcard
#set_property -dict { PACKAGE_PIN E2    IOSTANDARD LVCMOS33 } [get_ports { sdcard_pwr_n }];   #IO_L14P_T2_SRCC_35 Sch=sd_resetn
#set_property -dict { PACKAGE_PIN N6    IOSTANDARD LVCMOS33 } [get_ports { sd_cd }];          #IO_L9N_T1_DQS_AD7N_35 Sch=sd_cd
set_property -dict { PACKAGE_PIN L4    IOSTANDARD LVCMOS33 } [get_ports { sdclk }];          #IO_L9P_T1_DQS_AD7P_35 Sch=sdclk
set_property -dict { PACKAGE_PIN J8    IOSTANDARD LVCMOS33 } [get_ports { sdcmd }];          #IO_L16N_T2_35 Sch=sdcmd mosi
set_property -dict { PACKAGE_PIN M5    IOSTANDARD LVCMOS33 } [get_ports { sddat0 }];         #IO_L16P_T2_35 Sch=sd_dat[0] miso
set_property -dict { PACKAGE_PIN M7    IOSTANDARD LVCMOS33 } [get_ports { sddat1 }];         #IO_L18N_T2_35 Sch=sd_dat[1]
set_property -dict { PACKAGE_PIN H6    IOSTANDARD LVCMOS33 } [get_ports { sddat2 }];         #IO_L18P_T2_35 Sch=sd_dat[2]
set_property -dict { PACKAGE_PIN J6    IOSTANDARD LVCMOS33 } [get_ports { sddat3 }];         #IO_L14N_T2_SRCC_35 Sch=sd_dat[3] cs

## realtek Ethernet PHY
#set_property -dict { PACKAGE_PIN D17   IOSTANDARD LVCMOS33 } [get_ports { eth_col }]; 
#set_property -dict { PACKAGE_PIN G14   IOSTANDARD LVCMOS33 } [get_ports { eth_crs }]; 
set_property -dict { PACKAGE_PIN H2   IOSTANDARD LVCMOS33 } [get_ports { eth_mdc }]; 
set_property -dict { PACKAGE_PIN H1   IOSTANDARD LVCMOS33 } [get_ports { eth_mdio }]; 
set_property -dict { PACKAGE_PIN U1   IOSTANDARD LVCMOS33 } [get_ports { eth_ref_clk }]; 
set_property -dict { PACKAGE_PIN R1   IOSTANDARD LVCMOS33 } [get_ports { eth_rstn }]; 
set_property -dict { PACKAGE_PIN P4   IOSTANDARD LVCMOS33 } [get_ports { eth_rx_clk }]; 
set_property -dict { PACKAGE_PIN L3   IOSTANDARD LVCMOS33 } [get_ports { eth_rx_dv }]; 
set_property -dict { PACKAGE_PIN M4   IOSTANDARD LVCMOS33 } [get_ports { eth_rxd[0] }]; 
set_property -dict { PACKAGE_PIN N3   IOSTANDARD LVCMOS33 } [get_ports { eth_rxd[1] }]; 
set_property -dict { PACKAGE_PIN N4   IOSTANDARD LVCMOS33 } [get_ports { eth_rxd[2] }]; 
set_property -dict { PACKAGE_PIN P3   IOSTANDARD LVCMOS33 } [get_ports { eth_rxd[3] }]; 
set_property -dict { PACKAGE_PIN U5   IOSTANDARD LVCMOS33 } [get_ports { eth_rxerr }]; 
set_property -dict { PACKAGE_PIN M2   IOSTANDARD LVCMOS33 } [get_ports { eth_tx_clk }]; 
set_property -dict { PACKAGE_PIN T2   IOSTANDARD LVCMOS33 } [get_ports { eth_tx_en }]; 
set_property -dict { PACKAGE_PIN R2   IOSTANDARD LVCMOS33 } [get_ports { eth_txd[0] }]; 
set_property -dict { PACKAGE_PIN P1   IOSTANDARD LVCMOS33 } [get_ports { eth_txd[1] }]; 
set_property -dict { PACKAGE_PIN N2   IOSTANDARD LVCMOS33 } [get_ports { eth_txd[2] }]; 
set_property -dict { PACKAGE_PIN N1   IOSTANDARD LVCMOS33 } [get_ports { eth_txd[3] }]; 



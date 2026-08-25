// laur
//`define SIM_MODE

//`define DUMP_VCD
// dbgstart may be defined in hazard3_config.vh, not here
//`define dbgsclr
//`define dbgdhrystone
//`define dbghexcl
//`define dbgcache

//`define TN_DRAM_REFRESH // for tang nano
`define SIM_TNSRAM // tang nano not only sim ram
//`define BRAM_IMP
//`define frdiv 12
`define FREQ 83333333 //(1200 / `frdiv) * 1_000_000
`define WUKONGDDR3
//`define WUKONGSDRAM
/*
CLK
mmcm
uart define.vh:`define SERIAL_WCNT (FREQ/115200)
sd v and vhdl
eth
example_soc #( 
        .CLK_MHZ   (40)        // For timer timebase
sdram clk
ddr3 clk
    ) 
define.vh `define SDCARD_CLK_DIV 3 // clk is 50mhz
*/

//`define ethirqon
`define ETHERNET_DEVADDR 16'hc000
`define ETHERNET_MTU 16'd1500

//`define SDSPI
`define SDSPI_DEVADDR 16'h8000
`define SDSPI_BLOCKSIZE 16'd512
`define SDSPI_BLOCKADDR (`SDSPI_DEVADDR + `SDSPI_BLOCKSIZE)
`define SDSPI_ADDRUH 16'h4000
//`define simsdspi
`define SDCARD_CLK_DIV 3 
//`define FAT32_SD

`define CACHE_SIZE (32*1024)

`define SERIAL_WCNT (`FREQ / 38400)

`define XLEN    32
`define LATENCY 0

	`define LAUR_MEM_RB // mem read-back after writing it with BBL
	`define LAUR_MEM_RB_ONLY_CHECK

`define MEM_SIZE (10*1024*1024)
`define BBL_SIZE `MEM_SIZE // initmem.bin
`define BIN_BBL_SIZE   `BBL_SIZE
`define BIN_DISK_SIZE 0
`define BIN_SIZE       (`BIN_BBL_SIZE + `BIN_DISK_SIZE)

// simulate sdram winbond
`define winbaddrlen 24
`define winbdatalen 16
`define winbmasklen 2

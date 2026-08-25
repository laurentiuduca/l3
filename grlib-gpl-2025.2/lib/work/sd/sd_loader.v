// modified by Laurentiu-Cristian Duca, 2023-09-12
// used to copy linux from microsd to ram
// adapted from https://github.com/WangXuan95/FPGA-SDcard-Reader
//--------------------------------------------------------------------------------------------------------

`include "define.vh"

module sd_loader #(parameter SD_CLK_DIV = 3, parameter PRELOAD_FILE = "init_kernel.txt") (
    input  wire         clk,
    // rstn active-low, You can re-read SDcard by pushing the reset button.
    input  wire         resetn,
    input  wire [2:0]   w_main_init_state,
    input  wire [7:0]   w_ctrl_state,
    output wire [11:0]  tangled,
    // when sdcard_pwr_n = 0, SDcard power on
    output wire         sdcard_pwr_n,
    // signals connect to SD bus
    output wire         sdclk,
    inout  wire         sdcmd,
    input  wire         sddat0,
    output wire         sddat1, sddat2, sddat3,
    
    output reg  [31:0]  DATA,
    output reg          WE,
    output reg          DONE
    );

assign sdcard_pwr_n = 1'b0;                  // keep SDcard power-on

assign {sddat1, sddat2, sddat3} = 3'b111;    // Must set sddat1~3 to 1 to avoid SD card from entering SPI mode


reg [31:0] waddr=0;
wire       rdone;
reg        rstart = 0;
reg [31:0] rsector = 0;

wire       outen;
wire [7:0] outbyte;

wire [3:0] card_stat;
wire [1:0] card_type;

`define SD_SECTOR_SIZE 512
reg [7:0] state=0;
assign tangled = {state, card_type};
reg [7:0] mem[0:`SD_SECTOR_SIZE - 1];
reg [$clog2(`SD_SECTOR_SIZE):0] i=0;

    always @(posedge clk) begin
        if(!resetn) begin
            rstart <= 0;
            rsector <= 0;
            state <= 0;
            DATA <= 0;
            WE <= 0;
            DONE <= 0;
            i <= 0;
        end else begin
            if(state == 0) begin
                if (DONE==0) begin
                    if(rdone) begin // sector was read
                        $display("rdone, state0 state 20");
                        rstart <= 0;
                        state <= 20;
                    end else if(w_main_init_state == 3) begin
                        rstart <= 1;
                    end else
                        rstart <= 0;
                end else
                    rstart <= 0;
            end else if(state == 20) begin
                if(w_ctrl_state == 0)
                    if((i < `SD_SECTOR_SIZE) && ((rsector << $clog2(`SD_SECTOR_SIZE))+i) < `BIN_SIZE) begin
                        DATA <= {mem[i+3], mem[i+2], mem[i+1], mem[i]};
                        WE <= 1;
                        i <= i + 4;
                        state <= 21;
                        $display("rdone, state20 21");
                        `ifdef SIM_MODE
                        waddr <= waddr + 4;
                        `endif
                    end else begin
                        $display("rdone, state20 0, %d", waddr);
                        i <= 0;
                        state <= 0;
                        rsector <= rsector + 1;
                        if(waddr>=`BIN_SIZE) begin
                            DONE <= 1;
                            $display("sd loader done");
                        end
                    end
            end else if(state == 21) begin
                if(w_ctrl_state != 0) begin
                    WE <= 0;
                    state <= 20;
                    $display("rdone, state21 20");
                end
            end
        end
    end

`ifndef SIM_MODE
    always @(posedge clk) begin
        if(!resetn) begin
            waddr <= 0;
        end else begin
            if(DONE==0 && outen && w_main_init_state == 3) begin
                mem[waddr & (`SD_SECTOR_SIZE -1)] <= outbyte;
                waddr <= waddr + 1;
            end
        end
    end
`endif
/**********************************************************************************************/

`ifdef SIM_MODE
    // LOAD linux
    integer j;
    //integer k;
    reg  [7:0] mem_bbl [0:`BBL_SIZE-1];
    initial begin
`ifndef VERILATOR
    $display("VERILATOR NOT DEFINED");
    #1
`endif
        $write("Running %s `BBL_SIZE=%d `SD_SECTOR_SIZE\n", PRELOAD_FILE, `BBL_SIZE);
        $readmemh(PRELOAD_FILE, mem_bbl);
        j=0;
        for(j=0;j<`BBL_SIZE;j=j+1) begin
            mem[j] = mem_bbl[j];
        end
        $write("mem[3]=%x mem[2]=%x mem[1]=%x mem[0]=%x \n", mem[3], mem[2], mem[1], mem[0]);
        $write("-------------------------------------------------------------------\n");
    end

    assign rdone = 1;
`else

//----------------------------------------------------------------------------------------------------
// sd_reader
//----------------------------------------------------------------------------------------------------
sd_reader #(
    .CLK_DIV          ( SD_CLK_DIV )   // because clk=100MHz, CLK_DIV must ≥3
) u_sd_reader (
    .rstn             ( resetn         ),
    .clk              ( clk      ),
    .sdclk            ( sdclk          ),
    .sdcmd            ( sdcmd          ),
    .sddat0           ( sddat0         ),
    .card_stat        ( card_stat      ),  // show the sdcard initialize status
    .card_type        ( card_type       ),  // 0=UNKNOWN    , 1=SDv1    , 2=SDv2  , 3=SDHCv2
    .rstart           ( rstart         ),
    .rsector          ( rsector        ),  // read No. 0 sector (the first sector) in SDcard
    .rbusy            (                ),
    .rdone            ( rdone          ),
    .outen            ( outen          ),
    .outaddr          (                ),
    .outbyte          ( outbyte        )
);
`endif

endmodule

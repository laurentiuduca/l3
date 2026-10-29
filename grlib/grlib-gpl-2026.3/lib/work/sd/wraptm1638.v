module wraptm1638(
    input wire clk,
    input wire rst,
    output reg tm_cs,
    output wire tm_clk,
    inout  wire tm_dio,
    input wire [31:0] val
    );
    parameter CLK_DIV1638=5;

    localparam 
        HIGH    = 1'b1,
        LOW     = 1'b0;

    //reg [31:0] val=32'hfedcba98;

    wire [6:0] m[0:15]; 
                // +----+----+----+----+----+----+----+----+
                // | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
                // +----+----+----+----+----+----+----+----+
                // | DP | G  | F  | E  | D  | C  | B  | A  |
                // +----+----+----+----+----+----+----+----+
                //   A
                // F   B
                //   G  
                // E   C
                //   D
    assign  m[4'b0000] = 7'b0111111; //7'b1111110; // 0
    assign  m[4'b0001] = 7'b0000110; //7'b0110000; // 1
    assign  m[4'b0010] = 7'b1011011; //7'b1101101; // 2
    assign  m[4'b0011] = 7'b1001111; //7'b1111001; // 3
    assign  m[4'b0100] = 7'b1100110; //7'b0110011; // 4
    assign  m[4'b0101] = 7'b1101101; //7'b1011011; // 5
    assign  m[4'b0110] = 7'b1111101; //7'b1011111; // 6
    assign  m[4'b0111] = 7'b0000111; //7'b1110000; // 7
    assign  m[4'b1000] = 7'b1111111; //7'b1111111; // 8
    assign  m[4'b1001] = 7'b1101111; //7'b1111011; // 9
    assign  m[4'b1010] = 7'b1110111; //7'b1111101; // a
    assign  m[4'b1011] = 7'b1111100; //7'b0011111; // b
    assign  m[4'b1100] = 7'b1011000; //7'b0001101; // c
    assign  m[4'b1101] = 7'b1011110; //7'b0111101; // d
    assign  m[4'b1110] = 7'b1111001; //7'b1001111; // E
    assign  m[4'b1111] = 7'b1110001; //7'b1000111; // F

    wire [6:0]
        S_1     = m[val[31:28]],
        S_2     = m[val[27:24]],
        S_3     = m[val[23:20]],
        S_4     = m[val[19:16]],
        S_5     = m[val[15:12]],
        S_6     = m[val[11:08]],
        S_7     = m[val[07:04]],
        S_8     = m[val[03:00]],
        S_BLK   = 7'b0000000;

    localparam [7:0]
        C_READ  = 8'b01000010,
        C_WRITE = 8'b01000000,
        C_DISP  = 8'b10001111,
        C_ADDR  = 8'b11000000;

    localparam CLK_DIV = 19; // speed of scanner

    reg [5:0] instruction_step;
    reg [7:0] keys;

    reg [0:7] leds=8'h08; // leds
    reg leds_dir;
    reg [CLK_DIV:0] counter;

    // set up tristate IO pin for display
    //   tm_dio     is physical pin
    //   dio_in     for reading from display
    //   dio_out    for sending to display
    //   tm_rw      selects input or output
    reg tm_rw;
    wire dio_in, dio_out;
    assign tm_dio = tm_rw ? dio_out : 1'bz;
    assign dio_in = tm_dio;
	 
    // setup tm1638 module with it's tristate IO
    //   tm_in      is read from module
    //   tm_out     is written to module
    //   tm_latch   triggers u_tm1638 to read or write
    //   tm_rw      selects read or write mode to display
    //   busy       indicates when module is busy
    //                (another latch will interrupt)
    //   tm_clk     is the data clk
    //   dio_in     for reading from display
    //   dio_out    for sending to display
    //
    //   tm_data    the tristate io pin to module
    wire busy;
	 reg tm_latch;
    wire [7:0] tm_data, tm_in;
    reg [7:0] tm_out;

    // data input from device
    assign tm_in = tm_data;
	 // tm_rw == 1 => we send tm_data to u_tm1638, else we receive tm_data from u_tm1638
    assign tm_data = tm_rw ? tm_out : 8'hZZ;

    tm1638 #(.CLK_DIV(CLK_DIV1638)) u_tm1638 (
        .clk(clk),
        .rst(rst),
        .data_latch(tm_latch),
        .data(tm_data),
        .rw(tm_rw),
        .busy(busy),
        .sclk(tm_clk),
        .dio_in(dio_in),
        .dio_out(dio_out)
    );

    // handles displaying 1-8 on a display location
    // and animating the decimal point
    task display_digit;
        input [2:0] key;
        input [6:0] segs;

        begin
            tm_latch <= HIGH;

            if (keys[key])
                tm_out <= {1'b1, S_BLK[6:0]}; // decimal on
            else
                tm_out <= {1'b0, segs}; // decimal off
        end
    endtask

    // handles animating the LEDs 1-8
    task display_led;
        input [2:0] dot;

        begin
            tm_latch <= HIGH;
            tm_out <= {7'b0, leds[dot]};
        end
    endtask

    always @(posedge clk) begin
        if (rst) begin
            instruction_step <= 6'b0;
            tm_cs <= HIGH;
            tm_rw <= HIGH;
            counter <= 0;
            keys <= 8'b0;

        end else begin
            if (counter[0] && ~busy) begin
                case (instruction_step)
                    // *** KEYS ***
                    1:  {tm_cs, tm_rw}     <= {LOW, HIGH}; // tm_cs is active LOW
                    2:  {tm_latch, tm_out} <= {HIGH, C_READ}; // read mode
                    3:  {tm_latch, tm_rw}  <= {HIGH, LOW};

                    //  read back keys S1 - S8
                    4:  {keys[7], keys[3]} <= {tm_in[0], tm_in[4]};
                    5:  {tm_latch}         <= {HIGH};
                    6:  {keys[6], keys[2]} <= {tm_in[0], tm_in[4]};
                    7:  {tm_latch}         <= {HIGH};
                    8:  {keys[5], keys[1]} <= {tm_in[0], tm_in[4]};
                    9:  {tm_latch}         <= {HIGH};
                    10: {keys[4], keys[0]} <= {tm_in[0], tm_in[4]};
                    11: {tm_cs}            <= {HIGH};

                    // *** DISPLAY ***
                    12: {tm_cs, tm_rw}     <= {LOW, HIGH};
                    13: {tm_latch, tm_out} <= {HIGH, C_WRITE}; // write mode
                    14: {tm_cs}            <= {HIGH};

                    15: {tm_cs, tm_rw}     <= {LOW, HIGH};
                    16: {tm_latch, tm_out} <= {HIGH, C_ADDR}; // set addr 0 pos

                    17: display_digit(3'd7, S_1); // Digit 1
                    18: display_led(3'd0);        // LED 1

                    19: display_digit(3'd6, S_2); // Digit 2
                    20: display_led(3'd1);        // LED 2

                    21: display_digit(3'd5, S_3); // Digit 3
                    22: display_led(3'd2);        // LED 3

                    23: display_digit(3'd4, S_4); // Digit 4
                    24: display_led(3'd3);        // LED 4

                    25: display_digit(3'd3, S_5); // Digit 5
                    26: display_led(3'd4);        // LED 5

                    27: display_digit(3'd2, S_6); // Digit 6
                    28: display_led(3'd5);        // LED 6

                    29: display_digit(3'd1, S_7); // Digit 7
                    30: display_led(3'd6);        // LED 7

                    31: display_digit(3'd0, S_8); // Digit 8
                    32: display_led(3'd7);        // LED 8

                    33: {tm_cs}            <= {HIGH};

                    34: {tm_cs, tm_rw}     <= {LOW, HIGH};
                    35: {tm_latch, tm_out} <= {HIGH, C_DISP}; // display on, full bright
                    36: {tm_cs, instruction_step} <= {HIGH, 6'b0};

                endcase

                instruction_step <= instruction_step + 1;

            end else if (busy) begin
                // pull latch low next clock cycle after module has been
                // latched
                tm_latch <= LOW;
            end
				// counter[0] is used to allow u_tm1638 to take the command and acknowledge it with busy
            counter <= counter + 1;
        end
    end
endmodule

module rtc_controller(
    input           clk14,
    input           reset_n,

    input           scl_i,
    output          scl_o,
    output          scl_oen,
    input           sda_i,
    output          sda_o,
    output          sda_oen,

    input           read,
    input           write,
    output reg      ack,

    input   [3:0]   oki_second1,
    input   [3:0]   oki_second10,
    input   [3:0]   oki_minute1,
    input   [3:0]   oki_minute10,
    input   [3:0]   oki_hour1,
    input   [3:0]   oki_hour10,
    input   [3:0]   oki_day1,
    input   [3:0]   oki_day10,
    input   [3:0]   oki_month1,
    input   [3:0]   oki_month10,
    input   [3:0]   oki_year1,
    input   [3:0]   oki_year10,
    input   [3:0]   oki_weekday,

    output  [7:0]   ds_second,
    output  [7:0]   ds_minute,
    output  [7:0]   ds_hour,
    output  [7:0]   ds_weekday,
    output  [7:0]   ds_day,
    output  [7:0]   ds_month,
    output  [7:0]   ds_year
    );

    localparam      I2C_ADDRESS = 7'b1101000;

    reg     [4:0]   i2c_cmd;
    reg     [7:0]   i2c_din;

    wire            i2c_cmd_ack;
    wire            i2c_ack_out;
    wire    [7:0]   i2c_dout;
    wire            i2c_busy;
    wire            i2c_al;

    reg     [4:0]   state;
    reg     [7:0]   data[6:0];

    assign ds_second   = data[0];
    assign ds_minute   = data[1];
    assign ds_hour     = data[2];
    assign ds_weekday  = data[3];
    assign ds_day      = data[4];
    assign ds_month    = data[5];
    assign ds_year     = data[6];

    wire go = (read | write) && !ack;

    always @(posedge clk14 or negedge reset_n)
    begin
        if (!reset_n)
        begin
            i2c_cmd <= 5'b00000;
            state <= 5'd0;
        end
        else
        begin
            ack <= 1'b0;

            case (state)
            5'd0:
            if (go)
            begin
                i2c_cmd <= 5'b10010;
                i2c_din <= {I2C_ADDRESS, 1'b0};
                state <= state + 5'd1;
                if (write)
                begin
                    data[0] <= {oki_second10, oki_second1};
                    data[1] <= {oki_minute10, oki_minute1};
                    data[2] <= {oki_hour10, oki_hour1};
                    data[3] <= {4'd0, oki_weekday} + 8'd1;
                    data[4] <= {oki_day10, oki_day1};
                    data[5] <= {oki_month10, oki_month1};
                    data[6] <= {oki_year10, oki_year1};
                end
            end

            5'd1:
            if (i2c_cmd_ack)
            begin
                i2c_cmd <= 5'b00010;
                i2c_din <= 8'd0;

                if (read)
                    state <= 5'd2;
                else // if (write)
                    state <= 5'd11;
            end

            5'd2:
            if (i2c_cmd_ack)
            begin
                i2c_cmd <= 5'b10010;
                i2c_din <= {I2C_ADDRESS, 1'b1};
                state <= state + 5'd1;
            end

            5'd3:
            if (i2c_cmd_ack)
            begin
                i2c_cmd <= 5'b00100;
                state <= state + 5'd1;
            end

            5'd4, 5'd5, 5'd6, 5'd7, 5'd8:
            if (i2c_cmd_ack)
            begin
                data[state - 5'd4] <= i2c_dout;
                i2c_cmd <= 5'b00100;
                state <= state + 5'd1;
            end

            5'd9:
            if (i2c_cmd_ack)
            begin
                data[state - 5'd4] <= i2c_dout;
                i2c_cmd <= 5'b01101;
                state <= state + 5'd1;
            end

            5'd10:
            if (i2c_cmd_ack)
            begin
                data[state - 5'd4] <= i2c_dout;
                i2c_cmd <= 5'b00000;
                state <= 5'd0;
                ack <= 1'b1;
            end

            5'd11, 5'd12, 5'd13, 5'd14, 5'd15, 5'd16:
            if (i2c_cmd_ack)
            begin
                i2c_cmd <= 5'b00010;
                i2c_din <= data[state - 5'd11];
                state <= state + 5'd1;
            end

            5'd17:
            if (i2c_cmd_ack)
            begin
                i2c_cmd <= 5'b01010;
                i2c_din <= data[state - 5'd11];
                state <= state + 5'd1;
            end

            5'd18:
            if (i2c_cmd_ack)
            begin
                i2c_cmd <= 5'b00000;
                state <= 5'd0;
                ack <= 1'b1;
            end

            endcase
        end
    end

    i2c_master_byte_ctrl i2c_inst(
        .clk(clk14),
        .rst(1'b0),
        .nReset(reset_n),
        .ena(1'b1),
        .clk_cnt(16'd9), // 14 MHz / (5 * 280 kHz) - 1 = 9
        .start(i2c_cmd[4]),
        .stop(i2c_cmd[3]),
        .read(i2c_cmd[2]),
        .write(i2c_cmd[1]),
        .ack_in(i2c_cmd[0]),
        .din(i2c_din),
        .cmd_ack(i2c_cmd_ack),
        .ack_out(i2c_ack_out),
        .dout(i2c_dout),
        .i2c_busy(i2c_busy),
        .i2c_al(i2c_al),
        .scl_i(scl_i),
        .scl_o(scl_o),
        .scl_oen(scl_oen),
        .sda_i(sda_i),
        .sda_o(sda_o),
        .sda_oen(sda_oen)
    );

endmodule

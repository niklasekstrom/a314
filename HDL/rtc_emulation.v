module rtc_emulation(
    input               clk14,
    input               reset_n,

    input               cp_read_req,
    output reg          cp_read_ack,
    input               cp_write_req,
    output reg          cp_write_ack,

    input       [3:0]   cp_address,
    input       [3:0]   cp_out_emu_in,
    output reg  [3:0]   cp_in_emu_out,

    output reg          rtc_read,
    output reg          rtc_write,
    input               rtc_ack,

    output              cmem_bank,

    output  [3:0]   oki_second1,
    output  [3:0]   oki_second10,
    output  [3:0]   oki_minute1,
    output  [3:0]   oki_minute10,
    output  [3:0]   oki_hour1,
    output  [3:0]   oki_hour10,
    output  [3:0]   oki_day1,
    output  [3:0]   oki_day10,
    output  [3:0]   oki_month1,
    output  [3:0]   oki_month10,
    output  [3:0]   oki_year1,
    output  [3:0]   oki_year10,
    output  [3:0]   oki_weekday,

    input   [7:0]   ds_second,
    input   [7:0]   ds_minute,
    input   [7:0]   ds_hour,
    input   [7:0]   ds_weekday,
    input   [7:0]   ds_day,
    input   [7:0]   ds_month,
    input   [7:0]   ds_year
    );

    reg [3:0] oki_data[15:0];

    assign oki_second1 = oki_data[0];
    assign oki_second10 = oki_data[1];
    assign oki_minute1 = oki_data[2];
    assign oki_minute10 = oki_data[3];
    assign oki_hour1 = oki_data[4];
    assign oki_hour10 = oki_data[5];
    assign oki_day1 = oki_data[6];
    assign oki_day10 = oki_data[7];
    assign oki_month1 = oki_data[8];
    assign oki_month10 = oki_data[9];
    assign oki_year1 = oki_data[10];
    assign oki_year10 = oki_data[11];
    assign oki_weekday = oki_data[12];

    wire hold = oki_data[13][0];
    wire bank = oki_data[13][3];

    assign cmem_bank = bank;

    reg dirty = 1'b0;

    reg [22:0] read_countdown = 23'd0;

    reg read_req_sync1 = 1'b0;
    reg write_req_sync1 = 1'b0;

    always @(posedge clk14)
    begin
        read_req_sync1 <= cp_read_req;
        write_req_sync1 <= cp_write_req;
    end

    reg read_req_sync2 = 1'b0;
    reg write_req_sync2 = 1'b0;

    always @(negedge clk14)
    begin
        read_req_sync2 <= read_req_sync1;
        write_req_sync2 <= write_req_sync1;
    end

    wire want_read = read_req_sync2 != cp_read_ack;
    wire want_write = write_req_sync2 != cp_write_ack;

    always @(posedge clk14 or negedge reset_n)
    begin
        if (!reset_n)
        begin
            read_countdown <= 23'd0;

            oki_data[0] <= 4'h0;
            oki_data[1] <= 4'h0;
            oki_data[2] <= 4'h0;
            oki_data[3] <= 4'h0;
            oki_data[4] <= 4'h0;
            oki_data[5] <= 4'h0;
            oki_data[6] <= 4'h0;
            oki_data[7] <= 4'h0;
            oki_data[8] <= 4'h0;
            oki_data[9] <= 4'h0;
            oki_data[10] <= 4'h0;
            oki_data[11] <= 4'h0;
            oki_data[12] <= 4'h0;
            oki_data[13] <= 4'h0;
            oki_data[14] <= 4'h0;
            oki_data[15] <= 4'h4;

            dirty <= 1'b0;

            rtc_read <= 1'b0;
            rtc_write <= 1'b0;

            cp_write_ack <= 1'b0;
        end
        else
        begin
            read_countdown <= read_countdown + 23'd1;

            if (!rtc_read && !rtc_write && !hold)
            begin
                if (dirty)
                begin
                    rtc_write <= 1'b1;
                    dirty <= 1'b0;
                end
                else if (read_countdown == 24'd0)
                    rtc_read <= 1'b1;
            end

            if (rtc_write && rtc_ack)
                rtc_write <= 1'b0;

            if (rtc_read && rtc_ack)
                rtc_read <= 1'b0;

            if (rtc_read && rtc_ack && !hold && !dirty)
            begin
                oki_data[0]  <= ds_second[3:0];
                oki_data[1]  <= ds_second[7:4];
                oki_data[2]  <= ds_minute[3:0];
                oki_data[3]  <= ds_minute[7:4];
                oki_data[4]  <= ds_hour[3:0];
                oki_data[5]  <= {2'b0, ds_hour[5:4]};
                oki_data[6]  <= ds_day[3:0];
                oki_data[7]  <= ds_day[7:4];
                oki_data[8]  <= ds_month[3:0];
                oki_data[9]  <= {3'b0, ds_month[4]};
                oki_data[10] <= ds_year[3:0];
                oki_data[11] <= ds_year[7:4];
                oki_data[12] <= ds_weekday[3:0] - 4'd1;
            end

            if (want_write)
            begin
                if (cp_address >= 4'd13)
                begin
                    if (cp_address == 4'd13)
                        oki_data[13] <= cp_out_emu_in & 4'h9;
                    else if (cp_address == 4'd14)
                        oki_data[14] <= cp_out_emu_in;
                    else if (cp_address == 4'd15)
                        oki_data[15] <= cp_out_emu_in;
                end
                else if (hold)
                begin
                    oki_data[cp_address] <= cp_out_emu_in;
                    dirty <= 1'b1;
                end

                cp_write_ack <= write_req_sync2;
            end
        end
    end
    
    always @(posedge clk14 or negedge reset_n)
    begin
        if (!reset_n)
        begin
            cp_read_ack <= 1'b0;
            cp_in_emu_out <= 4'd0;
        end
        else
        begin
            if (want_read)
            begin
                cp_in_emu_out <= oki_data[cp_address];
                cp_read_ack <= read_req_sync2;
            end
        end
    end

endmodule

module spi_controller(
    input               clk200,

    input               SCK,
    input               SS,
    input               MOSI,
    output              MISO,

    output reg          spi_read_cmem,
    output reg          spi_write_cmem,
    output reg  [3:0]   spi_address_cmem,
    output reg  [3:0]   spi_out_cmem_in,
    input       [3:0]   spi_in_cmem_out,

    output reg          spi_req,
    input               spi_ack,
    output reg          spi_read_sram,

    output reg  [19:0]  spi_address_sram,

    output reg          spi_ub,
    output reg  [7:0]   spi_out_sram_in,
    input       [15:0]  spi_in_sram_out,

    input               swap_address_mapping
    );

    /*
    Commands:
    READ_SRAM   000aaaaa aaaaaaaa aaaaaaaa -------- oooooooo ...
    WRITE_SRAM  001aaaaa aaaaaaaa aaaaaaaa iiiiiiii ...
    READ_CMEM   010-aaaa ----oooo
    WRITE_CMEM  011-aaaa 0000iiii
    PROTO_VER   11111111 00000001
    */

    localparam READ_SRAM  = 3'd0;
    localparam WRITE_SRAM = 3'd1;
    localparam READ_CMEM  = 3'd2;
    localparam WRITE_CMEM = 3'd3;
    localparam PROTO_VER  = 8'd255;

    reg [23:0] counter;
    wire [20:0] byte_cnt = counter[23:3];
    wire [2:0] bit_cnt = counter[2:0];

    reg [2:0] cmd;

    reg [7:0] data_shift_in;

    reg [20:0] sram_address;
    reg [20:0] sram_offset = 21'd0;

    reg read_cmem_req = 1'b0;
    reg read_cmem_ack = 1'b0;

    reg write_cmem_req = 1'b0;
    reg write_cmem_ack = 1'b0;

    reg spi_req_async = 1'b0;

    always @(posedge SCK or negedge SS)
    begin
        if (!SS)
        begin
            counter <= 24'd0;
            cmd <= 4'd0;
        end
        else
        begin
            counter <= counter + 24'd1;

            if (counter <= 24'd2)
                cmd <= {cmd[1:0], MOSI};

            if (counter <= 24'd7)
                spi_address_cmem <= {spi_address_cmem[2:0], MOSI};

            if (counter <= 24'd23)
                sram_address <= {sram_address[19:0], MOSI};

            data_shift_in <= {data_shift_in[6:0], MOSI};

            if (counter == 24'd7 && cmd == READ_CMEM)
            begin
                read_cmem_req <= !read_cmem_ack;
            end

            if (counter == 24'd15 && cmd == WRITE_CMEM)
            begin
                spi_out_cmem_in <= {data_shift_in[2:0], MOSI};
                write_cmem_req <= !write_cmem_ack;
            end

            if (byte_cnt >= 21'd2 && bit_cnt == 3'd7 && cmd == READ_SRAM)
            begin
                spi_read_sram <= 1'b1;
                spi_req_async <= !spi_ack;
                sram_offset <= byte_cnt - 21'd2;
            end

            if (byte_cnt >= 21'd3 && bit_cnt == 3'd7 && cmd == WRITE_SRAM)
            begin
                spi_out_sram_in <= {data_shift_in[6:0], MOSI};
                spi_read_sram <= 1'b0;
                spi_req_async <= !spi_ack;
                sram_offset <= byte_cnt - 21'd3;
            end
        end
    end

    wire [20:0] sram_ea = sram_address + sram_offset;

    always @(*)
    begin
        spi_address_sram <= swap_address_mapping ? {sram_ea[20:17], sram_ea[8:1], sram_ea[16:9]} : sram_ea[20:1];
        spi_ub <= !sram_ea[0];
    end

    reg [7:0] data_shift_out = 8'd0;
    assign MISO = data_shift_out[7];

    always @(posedge SCK or negedge SS)
        if (!SS)
            data_shift_out <= 8'd0;
        else
        begin
            if (counter == 24'd7 && {sram_address[6:0], MOSI} == PROTO_VER)
                data_shift_out <= 8'd1; // SPI protocol version.
            else if (byte_cnt >= 21'd3 && bit_cnt == 3'd7 && cmd == READ_SRAM)
                data_shift_out <= spi_ub ? spi_in_sram_out[15:8] : spi_in_sram_out[7:0];
            else if (counter == 24'd11 && cmd == READ_CMEM)
                data_shift_out <= {spi_in_cmem_out, 4'd0};
            else
                data_shift_out <= {data_shift_out[6:0], 1'b0};
        end

    reg read_cmem_req_sync_0 = 1'b0;
    reg read_cmem_req_sync_1 = 1'b0;
    reg write_cmem_req_sync_0 = 1'b0;
    reg write_cmem_req_sync_1 = 1'b0;

    reg spi_req_sync = 1'b0;

    always @(posedge clk200)
    begin
        spi_req_sync <= spi_req_async;
        spi_req <= spi_req_sync;

        read_cmem_req_sync_0 <= read_cmem_req;
        read_cmem_req_sync_1 <= read_cmem_req_sync_0;

        if (spi_read_cmem)
        begin
            spi_read_cmem <= 1'b0;
            read_cmem_ack <= read_cmem_req_sync_1;
        end
        else if (read_cmem_req_sync_1 != read_cmem_ack)
            spi_read_cmem <= 1'b1;

        write_cmem_req_sync_0 <= write_cmem_req;
        write_cmem_req_sync_1 <= write_cmem_req_sync_0;

        if (spi_write_cmem)
        begin
            spi_write_cmem <= 1'b0;
            write_cmem_ack <= write_cmem_req_sync_1;
        end
        else if (write_cmem_req_sync_1 != write_cmem_ack)
            spi_write_cmem <= 1'b1;
    end

endmodule

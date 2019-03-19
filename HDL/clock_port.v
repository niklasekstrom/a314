module clock_port(
    input               clk200,

    input               CP_RD_n,
    input               CP_WR_n,
    input       [5:2]   CP_A,
    inout       [3:0]   CP_D,

    input               cmem_bank,

    output reg          cp_read_emu_req,
    input               cp_read_emu_ack,
    output reg          cp_write_emu_req,
    input               cp_write_emu_ack,
    input       [3:0]   cp_in_emu_out,

    output reg          cp_read_cmem,
    output reg          cp_write_cmem,
    input       [3:0]   cp_in_cmem_out,

    output reg  [3:0]   cp_address,
    output reg  [3:0]   cp_data_out
    );

    wire rd = !CP_RD_n;
    wire wr = !CP_WR_n;

    reg rd_sync_0 = 1'b0;
    reg rd_sync_1 = 1'b0;
    reg wr_sync_0 = 1'b0;
    reg wr_sync_1 = 1'b0;

    localparam STATE_IDLE = 3'd0;
    localparam STATE_LATCHING = 3'd1;
    localparam STATE_READ_CMEM = 3'd2;
    localparam STATE_WRITE_CMEM = 3'd3;
    localparam STATE_WAIT_CMEM = 3'd4;
    localparam STATE_WAIT_RTC = 3'd5;

    reg [2:0] state = STATE_IDLE;

    always @(posedge clk200)
    begin
        rd_sync_0 <= rd;
        rd_sync_1 <= rd_sync_0;
        wr_sync_0 <= wr;
        wr_sync_1 <= wr_sync_0;

        case (state)
        STATE_IDLE:
        begin
            cp_address <= CP_A[5:2];
            cp_data_out <= CP_D;

            if (rd_sync_1 || wr_sync_1)
                state <= STATE_LATCHING;
        end
        STATE_LATCHING:
        begin
            if (rd_sync_1)
            begin
                if (cmem_bank && cp_address != 4'hd)
                begin
                    cp_read_cmem <= 1'b1;
                    state <= STATE_READ_CMEM;
                end
                else
                begin
                    cp_read_emu_req <= !cp_read_emu_ack;
                    state <= STATE_WAIT_RTC;
                end
            end
            else
            begin
                if (cmem_bank && cp_address != 4'hd)
                begin
                    cp_write_cmem <= 1'b1;
                    state <= STATE_WRITE_CMEM;
                end
                else
                begin
                    cp_write_emu_req <= !cp_write_emu_ack;
                    state <= STATE_WAIT_RTC;
                end
            end
        end
        STATE_READ_CMEM:
        begin
            cp_read_cmem <= 1'b0;
            state <= STATE_WAIT_CMEM;
        end
        STATE_WRITE_CMEM:
        begin
            cp_write_cmem <= 1'b0;
            state <= STATE_WAIT_CMEM;
        end
        STATE_WAIT_CMEM, STATE_WAIT_RTC:
        if (!rd_sync_1 && !wr_sync_1)
        begin
            state <= STATE_IDLE;
        end
        default:
            state <= STATE_IDLE;
        endcase
    end

    wire [3:0] drive_data = state == STATE_WAIT_CMEM ? cp_in_cmem_out : (state == STATE_WAIT_RTC ? cp_in_emu_out : 4'd0);
    assign CP_D = rd ? drive_data : 4'bz;

endmodule

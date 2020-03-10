module cmem(
    input   clk200,

    output  AMI_INT2_n,
    output  RASP_IRQ,

    input spi_read,
    input spi_write,
    input [3:0] spi_address,
    input [3:0] spi_out_cmem_in,
    output reg [3:0] spi_in_cmem_out,

    input cp_read,
    input cp_write,
    input [3:0] cp_address,
    input [3:0] cp_out_cmem_in,
    output reg [3:0] cp_in_cmem_out,

    input dram_req,
    input dram_read,
    input [18:0] dram_address,

    output swap_address_mapping
    );

    reg [3:0] data[15:0];

    assign swap_address_mapping = data[11][0];
    wire autodetect_mode = data[11][1];

    reg dram_ack = 1'b0;
    reg [19:0] autodetect_address;

    reg [3:0] r_events = 4'd0;
    reg [3:0] r_enable = 4'd7;
    reg [3:0] a_events = 4'd0;
    reg [3:0] a_enable = 4'd3;

    reg a_block = 1'b0;
    reg drive_int2 = 1'b0;
    assign AMI_INT2_n = drive_int2 ? 1'b0 : 1'bz;

    reg r_armed = 1'b1;
    reg r_irq = 1'b0;
    assign RASP_IRQ = r_irq;

    /*
    Address map:
    
    reg0-5 - BA0-5
    regA   - firmware version
    regB   - autodetect, swap
    regC   - r-events
    regD   - r-enable
    regE   - a-events
    regF   - a-enable
    */

    wire rd_r_events = spi_read && spi_address == 4'd12;
    wire wr_r_events = cp_write && cp_address == 4'd12;
    wire wr_r_enable = spi_write && spi_address == 4'd13;

    wire rd_a_events = cp_read && cp_address == 4'd14;
    wire wr_a_events = spi_write && spi_address == 4'd14;
    wire wr_a_enable = cp_write && cp_address == 4'd15;

    wire r_trigger = ((wr_r_events ? (r_events | cp_out_cmem_in) : r_events) & (wr_r_enable ? spi_out_cmem_in : r_enable)) != 4'd0;

    reg [27:0] block_timeout = 28'd1;
    wire block_timed_out = block_timeout == 28'd0;

    wire a_should_drive = ((wr_a_events ? (a_events | spi_out_cmem_in) : a_events) & (wr_a_enable ? cp_out_cmem_in : a_enable)) != 4'd0 && !a_block;

    reg [19:0] rega_shift_out;

    always @(posedge clk200)
    begin
        if (spi_read)
            case (spi_address)
            4'd12: spi_in_cmem_out <= wr_r_events ? (r_events | cp_out_cmem_in) : r_events;
            4'd13: spi_in_cmem_out <= r_enable;
            4'd14, 4'd15: spi_in_cmem_out <= 4'd0;
            default: spi_in_cmem_out <= data[spi_address];
            endcase

        if (cp_read)
            case (cp_address)
            4'd10: cp_in_cmem_out <= rega_shift_out[3:0];
            4'd12, 4'd13: cp_in_cmem_out <= 4'd0;
            4'd14: cp_in_cmem_out <= wr_a_events ? (a_events | spi_out_cmem_in) : a_events;
            4'd15: cp_in_cmem_out <= a_enable;
            default: cp_in_cmem_out <= data[cp_address];
            endcase

        if (cp_write)
            data[cp_address] <= cp_out_cmem_in;

        if (cp_write && cp_address == 4'd11 && cp_out_cmem_in[1])
            autodetect_address <= 20'hfffff;
        else if (autodetect_mode && dram_req != dram_ack && !dram_read)
            autodetect_address <= {dram_address, 1'b0};

        dram_ack <= dram_req;

        if (cp_address == 4'd10)
        begin
            if (cp_write)
                case (cp_out_cmem_in)
                    4'd1: rega_shift_out <= autodetect_address;
                    default: rega_shift_out <= 20'd1; // Version
                endcase
            else if (cp_read)
                rega_shift_out[19:0] <= {4'd0, rega_shift_out[19:4]};
        end

        if (wr_a_enable)
            a_enable <= cp_out_cmem_in;

        if (wr_r_enable)
            r_enable <= spi_out_cmem_in;

        if (rd_r_events)
            r_armed <= 1'b1;
        else if (r_armed && r_trigger)
        begin
            r_irq <= !r_irq;
            r_armed <= 1'b0;
        end

        if (rd_r_events)
            r_events <= 4'd0;
        else if (wr_r_events)
            r_events <= r_events | cp_out_cmem_in;

        if (rd_a_events)
            a_events <= 4'd0;
        else if (wr_a_events)
            a_events <= a_events | spi_out_cmem_in;

        if (rd_a_events)
            block_timeout <= 28'd1;
        else if (drive_int2)
            block_timeout <= block_timeout + 28'd1;

        if (rd_a_events)
            a_block <= 1'b0;
        else if (block_timed_out)
            a_block <= 1'b1;

        if (!drive_int2 && a_should_drive)
            drive_int2 <= 1'b1;
        else if (drive_int2 && !a_should_drive)
            drive_int2 <= 1'b0;

    end

endmodule

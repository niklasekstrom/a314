module sram_arbiter(
    input               clk200,

    //output              SR_CE_n,
    output              SR_OE_n,
    output              SR_WE_n,
    output              SR_LB_n,
    output              SR_UB_n,
    output      [19:0]  SR_A,
    inout       [15:0]  SR_D,

    input               spi_req,
    output              spi_ack,
    input               spi_read,
    input       [19:0]  spi_address,
    input               spi_ub,
    input       [7:0]   spi_out_sram_in,
    output reg  [15:0]  spi_in_sram_out,

    input               dram_req,
    output              dram_ack,
    input               dram_read,
    input       [19:0]  dram_address,
    input               dram_lb,
    input               dram_ub,
    input       [15:0]  dram_out_sram_in,
    output reg  [15:0]  dram_in_sram_out
    );

    reg dram_ack_reg = 1'b0;
    assign dram_ack = dram_ack_reg;

    reg spi_ack_reg = 1'b0;
    assign spi_ack = spi_ack_reg;

    reg sr_oe_n = 1'b1;
    reg sr_we_n = 1'b1;
    reg sr_lb_n = 1'b1;
    reg sr_ub_n = 1'b1;
    reg [19:0] sr_a = 20'd0;

    assign SR_OE_n = sr_oe_n;
    assign SR_WE_n = sr_we_n;
    assign SR_LB_n = sr_lb_n;
    assign SR_UB_n = sr_ub_n;
    assign SR_A = sr_a;

    localparam DIR_READ   = 1'b0;
    localparam DIR_WRITE  = 1'b1;
    localparam SOURCE_DRAM = 1'b0;
    localparam SOURCE_SPI = 1'b1;

    reg [1:0] phase = 2'd0;
    reg accessing = 1'b0;
    reg access_source = 1'b0;
    reg access_dir = 1'b0;

    wire dram_wants = dram_req != dram_ack;
    wire spi_wants = spi_req != spi_ack;

    reg sram_drive = 1'b0;
    reg [15:0] sram_data_out = 16'd0;

    assign SR_D = sram_drive ? sram_data_out : 16'bz;

    always @(posedge clk200)
    begin
        case (phase)
            2'd0 : begin
                if (spi_wants)
                begin
                    accessing <= 1'b1;
                    access_source <= SOURCE_SPI;
                    access_dir <= spi_read ? DIR_READ : DIR_WRITE;
                    phase <= 2'd3;

                    sr_oe_n <= !spi_read;
                    sr_we_n <= 1'b1;
                    sr_lb_n <= spi_ub;
                    sr_ub_n <= !spi_ub;
                    sr_a <= spi_address;
                    sram_drive <= 1'b0;
                    sram_data_out <= spi_ub ? {spi_out_sram_in, 8'b0} : {8'b0, spi_out_sram_in};

                    spi_ack_reg <= spi_req;
                end
                else if (dram_wants)
                begin
                    accessing <= 1'b1;
                    access_source <= SOURCE_DRAM;
                    access_dir <= dram_read ? DIR_READ : DIR_WRITE;
                    phase <= 2'd3;

                    sr_oe_n <= !dram_read;
                    sr_we_n <= 1'b1;
                    sr_lb_n <= !dram_lb;
                    sr_ub_n <= !dram_ub;
                    sr_a <= dram_address;
                    sram_drive <= 1'b0;
                    sram_data_out <= dram_out_sram_in;

                    dram_ack_reg <= dram_req;
                end
                else
                begin
                    accessing <= 1'b0;
                    access_source <= SOURCE_DRAM;
                    access_dir <= DIR_READ;
                    phase <= 2'd0;

                    sr_oe_n <= 1'b1;
                    sr_we_n <= 1'b1;
                    sr_lb_n <= 1'b1;
                    sr_ub_n <= 1'b1;
                    sram_drive <= 1'b0;
                end
            end
            2'd3 : begin
                if (access_dir == DIR_WRITE)
                begin
                    sr_we_n <= 1'b0;
                    sram_drive <= 1'b1;
                end
                phase <= 2'd2;
            end
            2'd2 : begin
                phase <= 2'd1;
            end
            2'd1 : begin
                phase <= 2'd0;
            end
        endcase
    end

    always @(posedge clk200)
        if (phase == 2'd0 && accessing && access_dir == DIR_READ && access_source == SOURCE_SPI)
            spi_in_sram_out <= SR_D;

    always @(posedge clk200)
        if (phase == 2'd0 && accessing && access_dir == DIR_READ && access_source == SOURCE_DRAM)
            dram_in_sram_out <= SR_D;

endmodule

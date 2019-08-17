module dram_port(
    input               clk200,

    input               DR_WE_n,
    input               DR_RAS0_n,
    input               DR_RAS1_n,
    input               DR_CASL_n,
    input               DR_CASU_n,
    input       [8:0]   DR_A,
    inout       [15:0]  DR_D,

    output              req,
    input               ack,

    output              read,
    output      [18:0]  address,
    output              lb,
    output              ub,

    output reg  [15:0]  dram_out_sram_in,
    input       [15:0]  dram_in_sram_out
    );

    wire ras0 = !DR_RAS0_n;
    wire ras1 = !DR_RAS1_n;
    wire casl = !DR_CASL_n;
    wire casu = !DR_CASU_n;
    wire ras = ras0 || ras1;
    wire cas = casl || casu;
    wire rascas = ras && cas;

    reg [2:0] ras_sync = 3'd0;
    reg [2:0] cas_sync = 3'd0;
    reg [2:0] rascas_sync = 3'd0;

    always @(posedge clk200)
    begin
        ras_sync <= {ras_sync[1:0], ras};
        cas_sync <= {cas_sync[1:0], cas};
        rascas_sync <= {rascas_sync[1:0], rascas};
    end

    reg dram_read = 1'b0;
    reg drive_data = 1'b0;
    reg [18:0] dram_address = 19'd0;
    reg dram_lb = 1'b0;
    reg dram_ub = 1'b0;

    reg dram_req = 1'b0;
    wire dram_ack = ack;

    assign read = dram_read;
    assign address = dram_address;
    assign lb = dram_lb;
    assign ub = dram_ub;
    assign req = dram_req;

    always @(posedge clk200)
    begin
        if (ras_sync[1] && !ras_sync[2])
        begin
            dram_address[15:8] <= DR_A[7:0];
            dram_address[16] <= DR_A[8];
            dram_address[18] <= DR_RAS1_n;
            dram_read <= DR_WE_n;
        end

        if (rascas_sync[1] && !rascas_sync[2])
        begin
            dram_address[7:0] <= DR_A[7:0];
            dram_address[17] <= DR_A[8];
            dram_req <= !dram_ack;
            dram_lb <= casl;
            dram_ub <= casu;
            dram_out_sram_in <= DR_D;
            drive_data <= dram_read;
        end

        if (!cas_sync[1] && cas_sync[2])
            drive_data <= 1'b0;
    end

    wire drivel = drive_data && casl;
    wire driveu = drive_data && casu;

    assign DR_D[ 7:0] = drivel ? dram_in_sram_out[ 7:0] : 8'bz;
    assign DR_D[15:8] = driveu ? dram_in_sram_out[15:8] : 8'bz;

endmodule

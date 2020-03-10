module a314_top(
    input               CLK_14M,

    input               DR_WE_n,
    input               DR_RAS0_n,
    input               DR_RAS1_n,
    input               DR_CASL_n,
    input               DR_CASU_n,
    input       [8:0]   DR_A,
    inout       [15:0]  DR_D,

    input               CP_RD_n,
    input               CP_WR_n,
    input       [5:2]   CP_A,
    inout       [3:0]   CP_D,

    output              SR_OE_n,
    output              SR_WE_n,
    output              SR_LB_n,
    output              SR_UB_n,
    output      [18:0]  SR_A,
    inout       [15:0]  SR_D,

    output              AMI_INT2,

    input               RPI_SCLK0,
    input               RPI_SCE0,
    input               RPI_MOSI0,
    output              RPI_MISO0,

    output              RPI_IRQ,

    output              RTC_SCL,
    inout               RTC_SDA
    );

    wire clk14 = CLK_14M;
    wire clk200;

    a314_pll a314_pll_inst(
        .inclk0(clk14),
        .c0(clk200)
        );

    reg [4:0] reset_counter = 5'd31;
    wire reset_n = reset_counter == 5'd0;

    always @(posedge clk14)
        if (!reset_n)
            reset_counter <= reset_counter - 5'd1;

    wire spi_read_cmem;
    wire spi_write_cmem;
    wire [3:0] spi_address_cmem;
    wire [3:0] spi_out_cmem_in;
    wire [3:0] spi_in_cmem_out;

    wire spi_req;
    wire spi_ack;
    wire spi_read_sram;
    wire [18:0] spi_address_sram;
    wire spi_ub;
    wire [7:0] spi_out_sram_in;
    wire [15:0] spi_in_sram_out;

    wire swap_address_mapping;

    spi_controller spi_inst(
        .clk200(clk200),

        .SCK(RPI_SCLK0),
        .SS(RPI_SCE0),
        .MOSI(RPI_MOSI0),
        .MISO(RPI_MISO0),

        .spi_read_cmem(spi_read_cmem),
        .spi_write_cmem(spi_write_cmem),
        .spi_address_cmem(spi_address_cmem),
        .spi_out_cmem_in(spi_out_cmem_in),
        .spi_in_cmem_out(spi_in_cmem_out),

        .spi_req(spi_req),
        .spi_ack(spi_ack),
        .spi_read_sram(spi_read_sram),
        .spi_address_sram(spi_address_sram),
        .spi_ub(spi_ub),
        .spi_out_sram_in(spi_out_sram_in),
        .spi_in_sram_out(spi_in_sram_out),

        .swap_address_mapping(swap_address_mapping)
    );

    wire dram_req;
    wire dram_ack;
    wire dram_read;
    wire [18:0] dram_address;
    wire dram_lb;
    wire dram_ub;
    wire [15:0] dram_out_sram_in;
    wire [15:0] dram_in_sram_out;

    dram_port dram_port_inst(
        .clk200(clk200),

        //.DR_XMEM(DR_XMEM),
        .DR_WE_n(DR_WE_n),
        .DR_RAS0_n(1'b1), // Change to DR_RAS0_n(DR_RAS0_n) for rev 8 (A500+)
        .DR_RAS1_n(DR_RAS1_n),
        .DR_CASL_n(DR_CASL_n),
        .DR_CASU_n(DR_CASU_n),
        .DR_A(DR_A),
        .DR_D(DR_D),

        .req(dram_req),
        .ack(dram_ack),
        .read(dram_read),
        .address(dram_address),
        .lb(dram_lb),
        .ub(dram_ub),
        .dram_out_sram_in(dram_out_sram_in),
        .dram_in_sram_out(dram_in_sram_out)
        );

    sram_arbiter sram_arbiter_inst(
        .clk200(clk200),

        //.SR_CE_n(SR_CE_n),
        .SR_OE_n(SR_OE_n),
        .SR_WE_n(SR_WE_n),
        .SR_LB_n(SR_LB_n),
        .SR_UB_n(SR_UB_n),
        .SR_A(SR_A),
        .SR_D(SR_D),

        .spi_req(spi_req),
        .spi_ack(spi_ack),
        .spi_read(spi_read_sram),
        .spi_address(spi_address_sram),
        .spi_ub(spi_ub),
        .spi_out_sram_in(spi_out_sram_in),
        .spi_in_sram_out(spi_in_sram_out),

        .dram_req(dram_req),
        .dram_ack(dram_ack),
        .dram_read(dram_read),
        .dram_address(dram_address),
        .dram_lb(dram_lb),
        .dram_ub(dram_ub),
        .dram_out_sram_in(dram_out_sram_in),
        .dram_in_sram_out(dram_in_sram_out)
    );

    wire cmem_bank;

    wire cp_read_emu_req;
    wire cp_read_emu_ack;
    wire cp_write_emu_req;
    wire cp_write_emu_ack;
    wire [3:0] cp_in_emu_out;

    wire cp_read_cmem;
    wire cp_write_cmem;
    wire [3:0] cp_in_cmem_out;

    wire [3:0] cp_address;
    wire [3:0] cp_data_out;

    clock_port clock_port_inst(
        .clk200(clk200),

        .CP_RD_n(CP_RD_n),
        .CP_WR_n(CP_WR_n),
        .CP_A(CP_A),
        .CP_D(CP_D),

        .cmem_bank(cmem_bank),

        .cp_read_emu_req(cp_read_emu_req),
        .cp_read_emu_ack(cp_read_emu_ack),
        .cp_write_emu_req(cp_write_emu_req),
        .cp_write_emu_ack(cp_write_emu_ack),
        .cp_in_emu_out(cp_in_emu_out),

        .cp_read_cmem(cp_read_cmem),
        .cp_write_cmem(cp_write_cmem),
        .cp_in_cmem_out(cp_in_cmem_out),

        .cp_address(cp_address),
        .cp_data_out(cp_data_out)
    );

    cmem cmem_inst(
        .clk200(clk200),
        .AMI_INT2_n(AMI_INT2),
        .RASP_IRQ(RPI_IRQ),

        .spi_read(spi_read_cmem),
        .spi_write(spi_write_cmem),
        .spi_address(spi_address_cmem),
        .spi_out_cmem_in(spi_out_cmem_in),
        .spi_in_cmem_out(spi_in_cmem_out),

        .cp_read(cp_read_cmem),
        .cp_write(cp_write_cmem),
        .cp_address(cp_address),
        .cp_out_cmem_in(cp_data_out),
        .cp_in_cmem_out(cp_in_cmem_out),

        .dram_req(dram_req),
        .dram_read(dram_read),
        .dram_address(dram_address),

        .swap_address_mapping(swap_address_mapping)
        );

    wire rtc_read;
    wire rtc_write;
    wire rtc_ack;

    wire [3:0] oki_second1;
    wire [3:0] oki_second10;
    wire [3:0] oki_minute1;
    wire [3:0] oki_minute10;
    wire [3:0] oki_hour1;
    wire [3:0] oki_hour10;
    wire [3:0] oki_day1;
    wire [3:0] oki_day10;
    wire [3:0] oki_month1;
    wire [3:0] oki_month10;
    wire [3:0] oki_year1;
    wire [3:0] oki_year10;
    wire [3:0] oki_weekday;

    wire [7:0] ds_second;
    wire [7:0] ds_minute;
    wire [7:0] ds_hour;
    wire [7:0] ds_weekday;
    wire [7:0] ds_day;
    wire [7:0] ds_month;
    wire [7:0] ds_year;

    rtc_emulation rtc_emu_inst(
        .clk14(clk14),
        .reset_n(reset_n),

        .cp_read_req(cp_read_emu_req),
        .cp_read_ack(cp_read_emu_ack),
        .cp_write_req(cp_write_emu_req),
        .cp_write_ack(cp_write_emu_ack),

        .cp_address(cp_address),
        .cp_out_emu_in(cp_data_out),
        .cp_in_emu_out(cp_in_emu_out),

        .rtc_read(rtc_read),
        .rtc_write(rtc_write),
        .rtc_ack(rtc_ack),

        .cmem_bank(cmem_bank),

        .oki_second1(oki_second1),
        .oki_second10(oki_second10),
        .oki_minute1(oki_minute1),
        .oki_minute10(oki_minute10),
        .oki_hour1(oki_hour1),
        .oki_hour10(oki_hour10),
        .oki_day1(oki_day1),
        .oki_day10(oki_day10),
        .oki_month1(oki_month1),
        .oki_month10(oki_month10),
        .oki_year1(oki_year1),
        .oki_year10(oki_year10),
        .oki_weekday(oki_weekday),

        .ds_second(ds_second),
        .ds_minute(ds_minute),
        .ds_hour(ds_hour),
        .ds_day(ds_day),
        .ds_month(ds_month),
        .ds_year(ds_year),
        .ds_weekday(ds_weekday)
        );

    wire scl_i = RTC_SCL;
    wire scl_o;
    wire scl_oen;
    wire sda_i = RTC_SDA;
    wire sda_o;
    wire sda_oen;

    assign RTC_SCL = !scl_oen ? scl_o : 1'bz;
    assign RTC_SDA = !sda_oen ? sda_o : 1'bz;

    rtc_controller rtc_inst(
        .clk14(clk14),
        .reset_n(reset_n),

        .scl_i(scl_i),
        .scl_o(scl_o),
        .scl_oen(scl_oen),
        .sda_i(sda_i),
        .sda_o(sda_o),
        .sda_oen(sda_oen),

        .read(rtc_read),
        .write(rtc_write),
        .ack(rtc_ack),

        .oki_second1(oki_second1),
        .oki_second10(oki_second10),
        .oki_minute1(oki_minute1),
        .oki_minute10(oki_minute10),
        .oki_hour1(oki_hour1),
        .oki_hour10(oki_hour10),
        .oki_day1(oki_day1),
        .oki_day10(oki_day10),
        .oki_month1(oki_month1),
        .oki_month10(oki_month10),
        .oki_year1(oki_year1),
        .oki_year10(oki_year10),
        .oki_weekday(oki_weekday),

        .ds_second(ds_second),
        .ds_minute(ds_minute),
        .ds_hour(ds_hour),
        .ds_day(ds_day),
        .ds_month(ds_month),
        .ds_year(ds_year),
        .ds_weekday(ds_weekday)
        );

endmodule

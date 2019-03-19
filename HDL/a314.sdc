## VENDOR  "Altera"
## PROGRAM "Quartus Prime"
## VERSION "Version 18.0.0 Build 614 04/24/2018 SJ Lite Edition"

## DATE    "Sat Feb 09 15:38:50 2019"

##
## DEVICE  "10M08SCE144C8G"
##


#**************************************************************
# Time Information
#**************************************************************

set_time_format -unit ns -decimal_places 3



#**************************************************************
# Create Clock
#**************************************************************

create_clock -name clk14 -period 70.482 -waveform { 0.000 35.241 } [get_ports {CLK_14M}]
create_clock -name rpi_sclk -period 15.000 [get_ports {RPI_SCLK0}]

#**************************************************************
# Create Generated Clock
#**************************************************************

create_generated_clock -name clk200 -source [get_pins {a314_pll_inst|altpll_component|auto_generated|pll1|inclk[0]}] -duty_cycle 50/1 -multiply_by 14 -master_clock clk14 [get_pins {a314_pll_inst|altpll_component|auto_generated|pll1|clk[0]}] 

#**************************************************************
# Set Clock Latency
#**************************************************************



#**************************************************************
# Set Clock Uncertainty
#**************************************************************

derive_clock_uncertainty

#**************************************************************
# Set Input Delay
#**************************************************************

create_clock -period [get_clock_info -period clk200] -name clk200_vin
create_clock -period 285 -name clk3_vin
create_clock -period 3571 -name clk280k_vin
create_clock -period 15.000 -name clk_spi_vin

set_input_delay -clock clk200_vin -max 0 [get_ports {SR_D[*]}]
set_input_delay -clock clk200_vin -min 0 [get_ports {SR_D[*]}]

set_input_delay -clock clk3_vin -max 0 [get_ports {DR_*}]
set_input_delay -clock clk3_vin -min 0 [get_ports {DR_*}]

set_input_delay -clock clk3_vin -max 0 [get_ports {CP_*}]
set_input_delay -clock clk3_vin -min 0 [get_ports {CP_*}]

set_input_delay -clock clk280k_vin -max 0 [get_ports {RTC_SDA}]
set_input_delay -clock clk280k_vin -min 0 [get_ports {RTC_SDA}]

set_input_delay -clock clk_spi_vin -clock_fall -max 0 [get_ports {RPI_MOSI0}]
set_input_delay -clock clk_spi_vin -clock_fall -min 0 [get_ports {RPI_MOSI0}]

#**************************************************************
# Set Output Delay
#**************************************************************

create_clock -period [get_clock_info -period clk200] -name clk200_vout
create_clock -period 285 -name clk3_vout
create_clock -period 3571 -name clk280k_vout
create_clock -period 15.000 -name clk_spi_vout

set_output_delay -clock clk200_vout -max 0 [get_ports {SR_*}]
set_output_delay -clock clk200_vout -min 0 [get_ports {SR_*}]

set_output_delay -clock clk3_vout -max 0 [get_ports {DR_D[*]}]
set_output_delay -clock clk3_vout -min 0 [get_ports {DR_D[*]}]

set_output_delay -clock clk3_vout -max 0 [get_ports {CP_D[*]}]
set_output_delay -clock clk3_vout -min 0 [get_ports {CP_D[*]}]

set_output_delay -clock clk280k_vout -max 0 [get_ports {RTC_*}]
set_output_delay -clock clk280k_vout -min 0 [get_ports {RTC_*}]

set_output_delay -clock clk_spi_vout -max 0 [get_ports {RPI_MISO0}]
set_output_delay -clock clk_spi_vout -min 0 [get_ports {RPI_MISO0}]

#**************************************************************
# Set Clock Groups
#**************************************************************

set_clock_groups -asynchronous -group {clk14 clk200 clk200_vin clk200_vout} -group {clk3_vin clk3_vout} -group {clk280k_vin clk280k_vout} -group {rpi_sclk clk_spi_vin clk_spi_vout}

#**************************************************************
# Set False Path
#**************************************************************

set_false_path -from [get_keepers {rtc_emulation:rtc_emu_inst|*}] -to [get_keepers {clock_port:clock_port_inst|*}]
set_false_path -from [get_keepers {clock_port:clock_port_inst|*}] -to [get_keepers {rtc_emulation:rtc_emu_inst|*}]

set_false_path -from [get_ports {RPI_SCE0}]

set_false_path -to [get_ports {LED_C RPI_MISO1}]

#**************************************************************
# Set Multicycle Path
#**************************************************************



#**************************************************************
# Set Maximum Delay
#**************************************************************

set_max_delay -from [get_clocks {clk200}] -to [get_ports {SR_A[*] SR_LB_n SR_OE_n SR_UB_n SR_WE_n}] 7.1

set_max_delay -from [get_clocks {clk200}] -to [get_ports {SR_D[*]}] 7.5

#**************************************************************
# Set Minimum Delay
#**************************************************************



#**************************************************************
# Set Input Transition
#**************************************************************


CPP=g++
CC=gcc
DTC=dtc
BIN=bin_pi

clean:
	rm -f ${BIN}/a314d-td
	rm -f ${BIN}/a314d-cp
	rm -f ${BIN}/a314d-fe
	rm -f ${BIN}/spi-a314.dtbo
	rm -f ${BIN}/start_gpclk
	rmdir ${BIN}

${BIN}/a314d-td: a314d/a314d.cc
	${CPP} -DMODEL_TD=1 a314d/a314d.cc -O3 -o ${BIN}/a314d-td

${BIN}/a314d-cp: a314d/a314d.cc
	${CPP} -DMODEL_CP=1 a314d/a314d.cc -O3 -o ${BIN}/a314d-cp

${BIN}/a314d-fe: a314d/a314d.cc
	${CPP} -DMODEL_FE=1 a314d/a314d.cc -O3 -o ${BIN}/a314d-fe

${BIN}/spi-a314.dtbo: a314d/spi-a314-overlay.dts
	${DTC} -I dts -O dtb -o ${BIN}/spi-a314.dtbo a314d/spi-a314-overlay.dts

${BIN}/start_gpclk: a314d/start_gpclk.c
	${CC} a314d/start_gpclk.c -I/opt/vc/include -L/opt/vc/lib -lbcm_host -o ${BIN}/start_gpclk

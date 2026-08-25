openocd -f /usr/share/openocd/scripts/interface/raspberrypi-native.cfg -f /usr/share/openocd/scripts/cpld/xilinx-xc7.cfg  -c "init" -c "pld load xc7.pld ./top.bit; exit"

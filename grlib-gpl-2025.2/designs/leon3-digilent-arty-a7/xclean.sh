        rm -f simple.vvp *.vcd *.out result.txt simv ucli.key vcs-result.txt log.txt diff.txt final_mem.txt log.txt.gz trace.txt
        rm -rf simv.daidir csrc
        rm -rf obj_dir example_soc/libfpga/sd/*o example_soc/libfpga/sd/*cf *edf *json yosys*log *.il
        rm -rf synth.vg impl serialout.txt ftn.txt *out stip*.txt
        rm -rf laur2.txt log*txt ./a.out
        rm -rf xelab*.* xvlog*.* sd_model.log xsim*
        rm -f init_kernel.txt fh fl fout cl ch cr impl dump.vcd
        rm -f *vg *json yosys.txt
        rm -f initmem.bin simsd.fat32
        rm -rf webtalk* xelab*.* top_sim.wdb project.prj run.tcl *.vcd vivado* *bit capture* *out .Xil *dcp *rpt proj.*
        rm -rf example_soc/libfpga/sdsd_spi.o example_soc/libfpga/sdwork-obj93.cf tight_setup_hold_pins.txt
        rm -rf usage_statistics_webtalk* clockInfo.txt *wdb *log xvhdl.pb 


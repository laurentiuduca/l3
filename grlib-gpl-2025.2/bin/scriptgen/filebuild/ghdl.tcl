proc ghdl_create_tool {filetree fileinfo} {
	global GRLIB
	source "$GRLIB/bin/scriptgen/filebuild/ghdl_make.tcl"
	create_ghdl_make 
	set qpath "-Pgnu"
	# laur
	set chanvhd [open filesvhd.prj w]
	set chanv [open filesv.prj w]
	set xsimvarvhd ""
	set xsimvarv ""
	foreach k [dict keys $filetree] {
		set ktree [dict get $filetree $k]
		set kinfo [dict get $fileinfo $k]
		set bn [dict get $kinfo bn]
		set qpath "$qpath -Pgnu/$bn"
		append_lib_ghdl_make $k $kinfo 
		foreach l [dict keys $ktree] {
			set filelist [dict get $ktree $l]
			foreach f $filelist {
				set finfo [dict get $fileinfo $f]
				append_file_ghdl_make $f $finfo $qpath
				set laurbn [dict get $finfo bn]
				if {[string first ".vhd" $f] != -1} {
					append xsimvarvhd "\nvhdl $laurbn $f"
				} else {
					puts "-1 for $f"
					append xsimvarv "\nverilog work $f"
				}
			}
		}
	}
	puts $chanvhd "$xsimvarvhd"
	puts $chanv "$xsimvarv"
	close $chanvhd
	close $chanv
	eof_ghdl_make $qpath 
}

ghdl_create_tool $filetree $fileinfo
return

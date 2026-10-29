set -e
set -x
xvlog  -prj filesv.prj
xvhdl  -prj filesvhd.prj
xelab work.testbench -s simtop
xsim  simtop -wdb xsim_database.wdb -R


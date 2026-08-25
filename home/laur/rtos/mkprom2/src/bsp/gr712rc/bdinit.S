# MKPROM2 can not detect and initialize GR712RC AHBRAM. Use this bdinit0 to
# enable AHBRAM EDAC and clear the full 192 KiB AHBRAM.

# Can be used like this:
#   $ sparc-gaisler-elf-as bdinit.S -o bdinit.o
#   $ mkprom2 -freq 80 -bdinit main.elf -o prom.elf

        .section  .text
        .global   bdinit0
bdinit0:
        # GR712RC: ahbram0::cfg::en 1 (EDAC enable)
        set     0x80100000, %o0
        set     0x00200080, %o1
        st      %o1, [%o0]

        # Clear 192 KiB starting at address 0xa0000000
        set     0xa0000000, %o2
        set     0x00030000, %o3
        clr     %o4
ahbram0loop:
        cmp     %o3, %o4
        be      ahbram0end
         nop
        std     %g0, [%o2 + %o4]
        ba      ahbram0loop
         add  %o4, 8, %o4
ahbram0end:

        retl
         nop



# The bdinit1 below implements "Improve SDRAM timing" as described in the
# README in this directory.
#
# This table, from TSIM3, is provided as bit number reference. Please do not
# depend on the register values in the table.
#
#      0x80000004  Memory config register 2                0x7ac05220
#      31     sdramrf           0x0         SDRAM refresh enable
#      30     sdramtrp          0x1         SDRAM TRP parameter
#      29:27  sdramtrfc         0x7         SDRAM TRFC parameter
#      26     sdramtcas         0x0         SDRAM TCAS parameter
#      25:23  sdrambanksz       0x5         SDRAM bank size
#      22:21  sdramcolsz        0x2         SDRAM column size
#      20:19  sdramcmd          0x0         SDRAM command
#      18     d64               0x0         SDRAM 64-bit data bus
#      17     sdpb              0x0         SDRAM page burst
#      14     se                0x1         SDRAM enable
#      13     si                0x0         SRAM disable
#      12:9   rambanksz         0x9         RAM bank size
#       7     rbrdy             0x0         RAM bus ready enable
#       6     rmw               0x0         Read-modify-write enable
#       5:4   ramwidth          0x2         RAM width
#       3:2   ramwws            0x0         RAM write wait states
#       1:0   ramrws            0x0         RAM read wait states
#


        .global   bdinit1
bdinit1:
        # GR712RC: set ::mctrl0::mcfg2::sdramtcas 1
        set     0x80000004, %o0
        # get current mcfg2
        ld      [%o0], %o1
        # clear CMD
        set     (3<<19), %o2
        andn    %o1, %o2, %o1
        # set the TCAS
        set	(1<<26), %o2
        or      %o1, %o2, %o1
        # install new mcfg2
        st      %o1, [%o0]
        retl
         nop


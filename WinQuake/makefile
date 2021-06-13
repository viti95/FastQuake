###########################################################################
#                                                                         #
# Before running this, you have to do two things in SYS_DOS.C             #
#                                                                         #
# 1). add the following:                                                  #
#                                                                         #
#     #include <crt0.h>                                                   #
#     int _crt0_startup_flags = _CRT0_FLAG_UNIX_SBRK;                     #
#                                                                         #
# 2). remove "static" from line 64 ("static qboolean isDedicated;")       #
#                                                                         #
# That's all! Now type "make -f quake.mak" (and ignore the warnings, meh) #
#                                                                         #
# ftp://ftp.idsoftware.com/idstuff/source/    (GPL, too!!)                #
#                                                                         #
###########################################################################

CC=gcc
CFLAGS=-march=i486 -fno-strict-aliasing -Wall -Wno-trigraphs -Wno-unused-but-set-variable
LDFLAGS=
LIBS=-lm

ifdef DEBUG
	CFLAGS+=-O0 -gstabs+3 -fno-omit-frame-pointer -fno-fast-math
else
	CFLAGS+=-O3 -fomit-frame-pointer -ffast-math -DNDEBUG
	LDFLAGS+=-s
endif

PRG=486quake.exe

all:    $(PRG)

%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $<

%.o : %.s
	@echo Assembling $<
	@$(CC) $(CFLAGS) -x assembler-with-cpp -c $<

NET= \
	net_bw.o \
	net_dgrm.o \
	net_dos.o \
	net_ipx.o \
	net_loop.o \
	net_main.o \
	net_mp.o \
	net_ser.o \
	net_vcr.o

RENDER= \
	r_aclip.o \
	r_aclipa.o \
	r_alias.o \
	r_aliasa.o \
	r_bsp.o \
	r_draw.o \
	r_drawa.o \
	r_edge.o \
	r_edgea.o \
	r_light.o \
	r_efrag.o \
	r_main.o \
	r_misc.o \
	r_part.o \
	r_sky.o \
	r_sprite.o \
	r_surf.o \
	r_vars.o \
	r_varsa.o

DRAW= \
	d_copy.o \
	d_draw.o \
	d_draw16.o \
	d_edge.o \
	d_fill.o \
	d_init.o \
	d_modech.o \
	d_part.o \
	d_polyse.o \
	d_zpoint.o \
	d_polysa.o \
	d_parta.o \
	d_scan.o \
	d_scana.o \
	d_sky.o \
	d_spr8.o \
	d_sprite.o \
	d_surf.o \
	d_vars.o \
	d_varsa.o 

CLIENT= \
	cl_demo.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_tent.o

SERVER= \
	sv_main.o \
	sv_move.o \
 	sv_phys.o \
	sv_user.o

SOUND= \
	snd_dos.o \
	snd_dma.o \
	snd_gus.o \
	snd_mem.o \
	snd_mix.o \
	snd_mixa.o 

VIDEO= \
	vid_vga.o \
	vid_ext.o \
	vid_dos.o

SYSTEM= \
	sys_dos.o \
	sys_dosa.o

PROGRAMS= \
	pr_cmds.o \
	pr_edict.o \
	pr_exec.o


OBJS= \
	$(CLIENT) \
	$(DRAW) \
	$(NET) \
	$(PROGRAMS) \
	$(RENDER) \
	$(SERVER) \
	$(SOUND) \
	$(SYSTEM) \
	$(VIDEO) \
	cd_audio.o \
	chase.o \
	cmd.o \
	common.o \
	console.o \
	crc.o \
	cvar.o \
	dos_v2.o \
	draw.o \
	host.o \
	host_cmd.o \
	in_dos.o \
	keys.o \
	math.o \
	mathlib.o \
	menu.o \
	model.o \
	mplib.o \
	mplpc.o \
	sbar.o \
	screen.o \
	surf8.o \
	surf16.o \
	view.o \
	vregset.o \
	wad.o \
	world.o \
	worlda.o \
	zone.o 

$(PRG) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(PRG) $(LIBS)

clean:
	if exist *.o del *.o
	if exist 486quake.exe.exe del 486quake.exe.exe


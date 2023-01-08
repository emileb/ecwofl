LOCAL_PATH := $(call my-dir)/../

include $(CLEAR_VARS)

LOCAL_MODULE    := ecwolf

LOCAL_CFLAGS =  -fsigned-char -D__MOBILE__ -DNO_GTK -DBSTONE -DENGINE_NAME=\"ecwolf\" -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp
LOCAL_CFLAGS += -fexceptions -frtti

LOCAL_C_INCLUDES = $(LOCAL_PATH)/dosbox \
                   $(TOP_DIR)/AudioLibs_OpenTouch/openal/include/AL \
                   $(LOCAL_PATH)/mobile \
                   $(LOCAL_PATH)/g_shared \
                   $(LOCAL_PATH)/../deps/bzip2 \
                   $(LOCAL_PATH)/../deps/lzma/C \
                   $(LOCAL_PATH)/../deps/jpeg-6b \
                   $(LOCAL_PATH)/../deps/gdtoa \
                   $(TOP_DIR)/ \
	               $(TOP_DIR)/Clibs_OpenTouch/alpha


LOCAL_C_INCLUDES += $(SDL_INCLUDE_PATHS)  $(TOP_DIR)/MobileTouchControls $(TOP_DIR)/Clibs_OpenTouch

#mobile/game_interface.cpp \

SRC =  \
    ../../../Clibs_OpenTouch/alpha/android_jni.cpp \
    ../../../Clibs_OpenTouch/alpha/touch_interface.cpp \
    ../../../Clibs_OpenTouch/touch_interface_base.cpp \
    mobile/game_interface.cpp \
    mame/fmopl.cpp \
    posix/i_main.cpp \
	g_blake/a_smartanim.cpp \
	g_blake/blake_sbar.cpp \
	g_shared/a_deathcam.cpp \
	g_shared/a_inventory.cpp \
	g_shared/a_keys.cpp \
	g_shared/a_patrolpoint.cpp \
	g_shared/a_playerpawn.cpp \
	g_shared/a_randomspawner.cpp \
	g_wolf/a_spearofdestiny.cpp \
	g_wolf/wolf_sbar.cpp \
	r_2d/r_draw.cpp \
	r_2d/r_drawt.cpp \
	r_2d/r_main.cpp \
	r_2d/r_things.cpp \
	r_data/colormaps.cpp \
	r_data/r_translate.cpp \
	r_data/renderstyle.cpp \
	resourcefiles/resourcefile.cpp \
	resourcefiles/ancientzip.cpp \
	resourcefiles/file_7z.cpp \
	resourcefiles/file_audiot.cpp \
	resourcefiles/file_directory.cpp \
	resourcefiles/file_gamemaps.cpp \
	resourcefiles/file_grp.cpp \
	resourcefiles/file_pak.cpp \
	resourcefiles/file_rff.cpp \
	resourcefiles/file_rtl.cpp \
	resourcefiles/file_lump.cpp \
	resourcefiles/file_macbin.cpp \
	resourcefiles/file_vgagraph.cpp \
	resourcefiles/file_vswap.cpp \
	resourcefiles/file_wad.cpp \
	resourcefiles/file_zip.cpp \
	resourcefiles/wolfmapcommon.cpp \
	sfmt/SFMT.cpp \
	textures/anim_switches.cpp \
	textures/animations.cpp \
	textures/automaptexture.cpp \
	textures/bitmap.cpp \
	textures/ddstexture.cpp \
	textures/emptytexture.cpp \
	textures/flattexture.cpp \
	textures/imgztexture.cpp \
	textures/jpegtexture.cpp \
	textures/machudtexture.cpp \
	textures/multipatchtexture.cpp \
	textures/patchtexture.cpp \
	textures/picttexture.cpp \
	textures/pcxtexture.cpp \
	textures/pngtexture.cpp \
	textures/rawpagetexture.cpp \
	textures/rottflattexture.cpp \
	textures/solidtexture.cpp \
	textures/texture.cpp \
	textures/texturemanager.cpp \
	textures/tgatexture.cpp \
	textures/warptexture.cpp \
	textures/wolfrawtexture.cpp \
	textures/wolfshapetexture.cpp \
	thingdef/thingdef.cpp \
	thingdef/thingdef_codeptr.cpp \
	thingdef/thingdef_expression.cpp \
	thingdef/thingdef_parse.cpp \
	thingdef/thingdef_properties.cpp \
	thingdef/thingdef_type.cpp \
	actor.cpp \
	am_map.cpp \
	colormatcher.cpp \
	config.cpp \
	c_cvars.cpp \
	dobject.cpp \
	dobjgc.cpp \
	farchive.cpp \
	files.cpp \
	filesys.cpp \
	filesys_steam.cpp \
	g_conversation.cpp \
	g_intermission.cpp \
	g_mapinfo.cpp \
	gamemap.cpp \
	gamemap_planes.cpp \
	gamemap_uwmf.cpp \
	gitinfo.cpp \
	id_ca.cpp \
	id_in.cpp \
	id_sd.cpp \
	id_sd_n3dmus.cpp \
	id_us_1.cpp \
	id_vh.cpp \
	id_vl.cpp \
	language.cpp \
	lnspec.cpp \
	lumpremap.cpp \
	m_alloc.cpp \
	m_classes.cpp \
	m_random.cpp \
	m_png.cpp \
	name.cpp \
	p_switch.cpp \
	r_sprites.cpp \
	scanner.cpp \
	sdlvideo.cpp \
	sndinfo.cpp \
	sndseq.cpp \
	thinker.cpp \
	v_draw.cpp \
	v_font.cpp \
	v_palette.cpp \
	v_pfx.cpp \
	v_text.cpp \
	v_video.cpp \
	w_wad.cpp \
	weaponslots.cpp \
	wl_act2.cpp \
	wl_agent.cpp \
	wl_atmos.cpp \
	wl_cloudsky.cpp \
	wl_debug.cpp \
	wl_dir3dspr.cpp \
	wl_draw.cpp \
	wl_floorceiling.cpp \
	wl_game.cpp \
	wl_inter.cpp \
	wl_iwad.cpp \
	wl_loadsave.cpp \
	wl_main.cpp \
	wl_menu.cpp \
	wl_net.cpp \
	wl_parallax.cpp \
	wl_play.cpp \
	wl_state.cpp \
	wl_text.cpp \
	zstrformat.cpp \
	zstring.cpp \


LOCAL_SRC_FILES = $(SRC)

LOCAL_LDLIBS := -lEGL -ldl -llog -lz -lm -lc
LOCAL_STATIC_LIBRARIES := logwritter gdtoa zlib lzma jpeg6b bzip2
LOCAL_SHARED_LIBRARIES := touchcontrols  SDL2 SDL2_net SDL2_mixer core_shared saffal

LOCAL_STATIC_LIBRARIES +=

include $(BUILD_SHARED_LIBRARY)







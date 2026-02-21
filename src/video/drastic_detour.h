#ifndef __PATCH_H__
#define __PATCH_H__
    #include "drastic.h"
 
    //nds_system
    #define VAR_SYSTEM                  (uint64_t)(base_addr_rx + 0x003f6000)
    #define VAR_SYSTEM_GAMECARD_NAME    (base_addr_rx + 0x00480b38)
    #define VAR_SYSTEM_SAVESTATE_NUM    (VAR_SYSTEM + 0x47b9c0)
    
    #define VAR_SDL_SCREEN_BPP          VAR_DAT_03f2a5b0
    #define VAR_SDL_SCREEN_NEED_INIT    VAR_DAT_03f2a5dc
    #define VAR_SDL_SCREEN_WINDOW       VAR_DAT_03f2a578
    #define VAR_SDL_SCREEN_RENDERER     VAR_DAT_03f2a580

    #define VAR_SDL_SCREEN0_SHOW        0x0aee9544
    #define VAR_SDL_SCREEN0_HRES_MODE   VAR_DAT_03f2a549
    #define VAR_SDL_SCREEN0_TEXTURE     0x0aee952c
    #define VAR_SDL_SCREEN0_PIXELS      VAR_DAT_03f2a530
    #define VAR_SDL_SCREEN0_X           0x0aee9534
    #define VAR_SDL_SCREEN0_Y           0x0aee9538

    #define VAR_SDL_SCREEN1_SHOW        0x0aee9560
    #define VAR_SDL_SCREEN1_HRES_MODE    (VAR_DAT_03f2a549 + 0x28)
    #define VAR_SDL_SCREEN1_TEXTURE     0x0aee9548
    #define VAR_SDL_SCREEN1_PIXELS     ( VAR_DAT_03f2a530 + 8*5)
    #define VAR_SDL_SCREEN1_X           0x0aee9550
    #define VAR_SDL_SCREEN1_Y           0x0aee9554
	
	#define VAR_SDL_SWAP_SCREENS    VAR_DAT_03f2a5d4
	#define VAR_SDL_FRAME_COUNT      VAR_DAT_03f2a5f4
	#define VAR_SDL_SCREEN_CURSOR  VAR_DAT_03f2a5e4
	
    #define VAR_SDL_INPUT (base_addr_rx + 0x03f2a608)
	
    #define VAR_DAT_03f2a578		(base_addr_rx + 0x03f2a578)
    #define VAR_DAT_03f2a580		(base_addr_rx + 0x03f2a580)
    #define VAR_DAT_03f2a530		(base_addr_rx + 0x03f2a530)
    #define VAR_DAT_03f2a549		(base_addr_rx + 0x03f2a549)
    #define VAR_DAT_03f2a5b0		(base_addr_rx + 0x03f2a5b0)
    #define VAR_DAT_03f2a5dc		(base_addr_rx + 0x03f2a5dc)

    #define VAR_DAT_0402a548		(base_addr_rx + 0x0402a548)
    #define VAR_DAT_0402a549		(base_addr_rx + 0x0402a549)
    #define VAR_DAT_0402a5b0		(base_addr_rx + 0x0402a5b0)
    #define VAR_SDL_SCREEN		(base_addr_rx + 0x0402a528)
    #define VAR_DAT_0402a530		(base_addr_rx + 0x0402a530)
    
    #define VAR_DAT_0402a580		(base_addr_rx + 0x0402a580)
    #define VAR_DAT_0402a550		(base_addr_rx + 0x0402a550)
    #define VAR_DAT_0402a570		(base_addr_rx + 0x0402a570)
    #define VAR_DAT_0402a5f4		(base_addr_rx + 0x0402a5f4)
    #define VAR_DAT_0402a5f0		(base_addr_rx + 0x0402a5f0)
    #define VAR_DAT_0402a590		(base_addr_rx + 0x0402a590)
    #define VAR_DAT_0402a598		(base_addr_rx + 0x0402a598)
    
	#define VAR_DAT_03f2a5f4     (base_addr_rx + 0x03f2a5f4)
	#define VAR_DAT_03f2a5e4    (base_addr_rx + 0x03f2a5e4)
	
	#define VAR_DAT_03f2a5d4   (base_addr_rx + 0x03f2a5d4)
	
    #define VAR_ADPCM_STEP_TABLE        (base_addr_rx + 0x0025b230)
    #define VAR_ADPCM_INDEX_STEP_TABLE  (base_addr_rx + 0x0025b2e8)
    #define VAR_DESMUME_FOOTER_STR      (base_addr_rx + 0x0025b370)

    #define FUN_FREE                    (base_addr_rx + 0x0402C218)
    #define FUN_REALLOC                 (base_addr_rx + 0x0402c288)
    #define FUN_MALLOC                  (base_addr_rx + 0x0402c6f0)
    #define FUN_SCREEN_COPY16           (base_addr_rx + 0x00097df0)
    #define FUN_PRINT_STRING            (base_addr_rx + 0x00097c30)
    #define FUN_LOAD_STATE_INDEX        (base_addr_rx + 0x00088ff0)
    #define FUN_SAVE_STATE_INDEX        (base_addr_rx + 0x00088f10)
    #define FUN_QUIT                    (base_addr_rx + 0x0000fa20)
    //#define FUN_SAVESTATE_PRE           0x08095a80
    //#define FUN_SAVESTATE_POST          0x08095154
    
    #define FUN_UPDATE_SCREEN           (base_addr_rx + 0x00099df0)
    #define FUN_UPDATE_SCREENS          (base_addr_rx + 0x0009a350)
    #define FUN_FRAME_GEOMETRY         (base_addr_rx + 0x00074fd0)
    #define FUN_UPDATE_FRAME         	(base_addr_rx + 0x00034260)
    
    #define FUN_SET_SCREEN_MENU_OFF     (base_addr_rx + 0x0009a170)
    
    #define FUN_LOAD_STATE              (base_addr_rx + 0x000884c0)
    #define FUN_SAVE_STATE              (base_addr_rx + 0x00088b70)
    #define FUN_BLIT_SCREEN_MENU        (base_addr_rx + 0x000982f0)
    #define FUN_INITIALIZE_BACKUP       (base_addr_rx + 0x00086090)
    #define FUN_SET_SCREEN_MENU_OFF     (base_addr_rx + 0x0009a170)
    #define FUN_GET_SCREEN_PTR          (base_addr_rx + 0x0009a7e0)
    #define FUN_SPU_ADPCM_DECODE_BLOCK  (base_addr_rx + 0x0007f540)

    #define FUN_INITIALIZE_MEMORY (base_addr_rx + 0x0001e7e0)
    #define FUN_PLATFORM_INITIALIZE_INPUT (base_addr_rx + 0x0009c030)
    #define FUN_INITIALIZE_SYSTEM_DIRECTORY (base_addr_rx + 0x00010780)
    //#define CODE_FAST_FORWARD           0x08006ad0

    /* add by trngaje */
    #define FUN_UPDATE_INPUT            (base_addr_rx + 0x00098430)
    #define FUN_PLATFORM_GET_INPUT	(base_addr_rx + 0x0009aaf0)
    #define FUN_MENU                    (base_addr_rx + 0x00093460)
    #define FUN_SET_SCREEN_ORIENTATION  (base_addr_rx + 0x0009a7b0)
    #define FUN_UPDATE_SCREEN_MENU	(base_addr_rx + 0x00099f40)
    #define FUN_LUA_IS_ACTIVE		(base_addr_rx + 0x000999b0)
    #define FUN_LUA_ON_FRAME_UPDATE	(base_addr_rx + 0x00099be0)
    #define FUN_SET_DEBUG_MODE		(base_addr_rx + 0x00095fa0)
    #define FUN_CPU_BLOCK_LOG_ALL	(base_addr_rx + 0x000305c0)
    #define FUN_SET_SCREEN_SWAP		(base_addr_rx + 0x0009a7d0)
    #define FUN_QUIT			(base_addr_rx + 0x0000fa20)
    #define FUN_SPU_FAKE_MICROPHONE_STOP	(base_addr_rx + 0x00080220)
    #define FUN_SPU_FAKE_MICROPHONE_START	(base_addr_rx + 0x00080200)
    #define FUN_TOUCHSCREEN_SET_POSITION	(base_addr_rx + 0x00084bc0)
    #define FUN_BACKUP_SAVE_PART_0		(base_addr_rx + 0x00085840)
    #define FUN_BACKUP_SAVE 			(base_addr_rx + 0x00086040)
    #define FUN_BACKUP_AUTO_SAVE_STEP   (base_addr_rx + 0x00086060)
    #define FUN_GET_GUI_INPUT			(base_addr_rx + 0x0009b7b0)
    #define FUN_DRAW_MENU_BG			(base_addr_rx + 0x00093140)
    #define FUN_SAVESTATE_INDEX_TIMESTAMP 	(base_addr_rx + 0x000890b0)
    #define FUN_GAMECARD_CLOSE			(base_addr_rx + 0x00084520)
    #define FUN_AUDIO_EXIT			(base_addr_rx + 0x0009ce20)
    #define FUN_INPUT_LOG_CLOSE			(base_addr_rx + 0x00098cf0)
    #define FUN_UNINITIALIZE_MEMORY		(base_addr_rx + 0x0001f1e0)
    #define FUN_PLATFORM_QUIT			(base_addr_rx + 0x00099de0)
    #define FUN_SAVE_DIRECTORY_CONFIG_FILE	(base_addr_rx + 0x00099de0)
    #define FUN_CLEAR_GUI_ACTIONS		(base_addr_rx + 0x0009b0d0)
    #define FUN_SCREEN_SET_CURSOR_POSITION	(base_addr_rx + 0x0009a9f0)
    #define FUN_CONVERT_TOUCH_COORDINATES	(base_addr_rx + 0x0009aa10)

	#define FUN_LOAD_CONFIG_FILE					(base_addr_rx + 0x0008bd80)
	#define FUN_LOAD_CONFIG_FILE_BINARY	(base_addr_rx + 0x0008a9a0)
	#define FUN_SAVE_CONFIG_FILE					(base_addr_rx + 0x0008b020)
	#define FUN_CHOMP_WHITESPACE				(base_addr_rx + 0x000978a0)
	#define FUN_SET_SCREEN_SCALE_FACTOR				(base_addr_rx + 0x0009a790)
	#define FUN_SET_SCREEN_SWAP					(base_addr_rx + 0x0009a7d0)
	#define FUN_SKIP_WHITESPACE					(base_addr_rx + 0x00097800)
	
    #define FUN_CREATE_MENU_MAIN                (base_addr_rx + 0x00092cd0)
	#define FUN_CREATE_MENU_OPTION          (base_addr_rx + 0x00091530)
    #define FUN_CREATE_MENU_COLTROLS     (base_addr_rx + 0x000924f0)
    #define FUN_CREATE_MENU_FIRMWARE     (base_addr_rx + 0x00092880)
    #define FUN_DRAW_MENU_OPTION              (base_addr_rx + 0x0008d7d0)
    #define FUN_ACTION_SELECT_MENU              (base_addr_rx + 0x0008e000)
    #define FUN_DESTROY_SELECT_MENU         (base_addr_rx + 0x0008e520)
    #define FUN_SELECT_CHEAT_MENU               (base_addr_rx + 0x00090dc0)
    #define FUN_FOCUS_SAVESTATE                     (base_addr_rx + 0x0008d560)
    #define FUN_MODIFY_SNAPSHOT_BG            (base_addr_rx + 0x0008d4b0)
    #define FUN_SELECT_LOAD_STATE                 (base_addr_rx + 0x0008df50)
    #define FUN_DRAW_NUMERIC                            (base_addr_rx + 0x0008d9f0)
    #define FUN_ACTION_NUMERIC                         (base_addr_rx + 0x0008d4c0)
    #define FUN_ACTION_NUMERIC_SELECT        (base_addr_rx + 0x0008e420)
    #define FUN_SELECT_SAVE_STATE                 (base_addr_rx + 0x0008dfb0)
    #define FUN_ACTION_SELECT                           (base_addr_rx + 0x0008e140)
    #define FUN_SELECT_LOAD_GAME                 (base_addr_rx + 0x000905e0)
    #define FUN_SELECT_RESTART                        (base_addr_rx + 0x0008d570)
    #define FUN_SELECT_RETURN                        (base_addr_rx + 0x0008d590)
    #define FUN_SELECT_QUIT                              (base_addr_rx + 0x0008dfe0)
    #define FUN_DRAW_MENU_MAIN                              (base_addr_rx + 0x0008e2a0)

    #define FUN_CREATE_MENU_EXTRA_CONTROLS                          (base_addr_rx + 0x00092220)
    #define FUN_DRAW_BUTTON_CONFIG                                              (base_addr_rx + 0x0008d670)
    #define FUN_ACTION_BUTTON_CONFIG                                          (base_addr_rx + 0x0008e190)
    #define FUN_SELECT_RESTORE_DEFAULT_CONTROLS              (base_addr_rx + 0x0008dff0)
    #define FUN_SELECT_DELETE_CONFIG_LOCAL                              (base_addr_rx + 0x0008e5c0)
    #define FUN_SELECT_SAVE_CONFIG_GLOBAL                               (base_addr_rx + 0x0008e6c0)
    #define FUN_SELECT_EXIT_CURRENT_MENU                                 (base_addr_rx + 0x0008e0a0)
    #define FUN_DRAW_MENU_CONTROLS                                             (base_addr_rx + 0x0008d8d0)
    #define FUN_FOCUS_MENU_NONE                                                     (base_addr_rx + 0x0008e290)
    
    #define FUN_SDL_DRAW_BUTTON_CONFIG                                    (base_addr_rx + 0x0008d670)
    #define FUN_PLATFORM_PRINT_CODE                                             (base_addr_rx + 0x0009b1c0)
    #define FUN_PRINT_STRING                                                                 (base_addr_rx + 0x00097c30)
    #define FUN_ACTION_BUTTON_CONFIG                                          (base_addr_rx + 0x0008e190)
    #define FUN_PLATFORM_GET_CONFIG_INPUT                              (base_addr_rx + 0x0009b2d0)
    #define FUN_DELAY_US                                                                          (base_addr_rx + 0x00099cc0)
    
    #define FUN_CONFIG_UPDATE_SETTINGS                                       (base_addr_rx + 0x0008a6e0)
    
    #define FUN_SPU_LOAD_FAKE_MICROPHONE_DATA   (base_addr_rx + 0x0007f220)
    
    #define FUN_DRAW_NUMERIC_LABELED            (base_addr_rx + 0x0008dae0)
    #define FUN_SELECT_SAVE_CONFIG_LOCAL   (base_addr_rx + 0x0008e2b0)
    #define FUN_DRAW_MENU_OPTIONS                   (base_addr_rx + 0x0008d7b0)

    #define FUN_SELECT_LOAD_GAME                   (base_addr_rx + 0x000905e0)
    #define FUN_LOAD_FILE                                       (base_addr_rx + 0x0008f140)
    #define FUN_LOAD_NDS                                       (base_addr_rx + 0x000839d0)
    
    #define FUN_LOAD_LOGO                                     (base_addr_rx + 0x00093360)
    #define FUN_AUDIO_PAUSE                                 (base_addr_rx + 0x0009ce30)
    #define FUN_SET_SCREEN_MENU_ON             (base_addr_rx + 0x0009a900)
    #define FUN_DRAW_MENU_BG                            (base_addr_rx + 0x00093140)
    #define FUN_SET_FONT_NARROW                    (base_addr_rx + 0x00097dd0)
    #define FUN_SET_FONT_WIDE                            (base_addr_rx + 0x00097db0)
    #define FUN_UPDATE_SCREEN_MENU              (base_addr_rx + 0x00099f40)
    #define FUN_AUDIO_REVERT_PAUSE_STATE    (base_addr_rx + 0x00098420)
    #define FUN_CONFIG_UPDATE_SETTINGS         (base_addr_rx + 0x0008a6e0)
    #define FUN_SET_SCREEN_MENU_OFF              (base_addr_rx + 0x0009a170)
    #define FUN_RESET_SYSTEM                                  (base_addr_rx + 0x00010e90)

    #define FUN_GET_TICKS_US                                 (base_addr_rx + 0x00099c50)

    #define ALIGN_ADDR(addr)        ((void*)((size_t)(addr) & ~(page_size - 1)))

    typedef enum _backup_type_enum {
        BACKUP_TYPE_NONE   = 0,
        BACKUP_TYPE_FLASH  = 1,
        BACKUP_TYPE_EEPROM = 2,
        BACKUP_TYPE_NAND   = 3
    } backup_type_enum;

    typedef struct _backup_struct {
        uint32_t dirty_page_bitmap[2048];
        char file_path[1024];
        backup_type_enum type;
        uint32_t access_address;
        uint32_t address_mask;
        uint32_t fix_file_size;
        uint8_t *data;
        uint8_t jedec_id[4];
        uint32_t write_frame_counter;
        uint16_t mode;
        uint8_t state;
        uint8_t status;
        uint8_t address_bytes;
        uint8_t state_step;
        uint8_t firmware;
        uint8_t footer_written;
    } backup_struct;

    typedef struct _spu_channel_struct {
        int16_t adpcm_sample_cache[64];
        uint64_t sample_offset;
        uint64_t frequency_step;
        uint32_t adpcm_cache_block_offset;
        uint8_t *io_region;
        uint8_t *samples;
        uint32_t sample_address;
        uint32_t sample_length;
        uint32_t loop_wrap;
        int16_t volume_multiplier_left;
        int16_t volume_multiplier_right;
        int16_t adpcm_loop_sample;
        int16_t adpcm_sample;
        uint8_t format;
        uint8_t dirty_bits;
        uint8_t active;
        uint8_t adpcm_loop_index;
        uint8_t adpcm_current_index;
        uint8_t adpcm_looped;
        uint8_t capture_timer;
    } spu_channel_struct;

    typedef void (*nds_free)(void *ptr);
    typedef void (*nds_set_screen_menu_off)(void);
    typedef void (*nds_quit)(void *system);
    typedef void (*nds_screen_copy16)(unsigned short *param_1,unsigned int param_2);
    typedef void (*nds_load_state_index)(void *system, uint32_t index, uint16_t *snapshot_top, uint16_t *snapshot_bottom, uint32_t snapshot_only);
                    
    typedef void (*nds_save_state_index)(void *system, uint32_t index, uint16_t *snapshot_top, uint16_t *snapshot_bottom);
 
    typedef int32_t (*nds_load_state)(void *system, const char *path, uint16_t *snapshot_top, uint16_t *snapshot_bottom, uint32_t snapshot_only);
    typedef void (*nds_save_state)(ulong *param_1,char *param_2,char *param_3,void *param_4,void *param_5);
    
    typedef void* (*nds_get_screen_ptr)(uint32_t screen_number);
    typedef void* (*nds_realloc)(void *ptr, size_t size);
    typedef void* (*nds_malloc)(size_t size);
    typedef void (*nds_spu_adpcm_decode_block)(spu_channel_struct *channel);

    typedef void (*nds_platform_get_input)(long param_1/*input_struct *input*/);
    typedef bool (*nds_lua_is_active)(void);
    typedef void (*nds_lua_on_frame_update)(void);
    typedef void (*nds_set_debug_mode)(long param_1,unsigned char param_2);
    typedef int (*nds_cpu_block_log_all)(long param_1,char *param_2);
    //typedef void (*nds_screen_copy16)(ushort *param_1,unsigned int param_2);
    typedef void (*nds_menu)(long param_1,int param_2);
    typedef void (*nds_set_screen_swap)(unsigned int param_1);
    //typedef void (*nds_quit)(long param_1);
    typedef void (*nds_spu_fake_microphone_stop)(long param_1);
    typedef void (*nds_spu_fake_microphone_start)(long param_1);
    typedef void (*nds_touchscreen_set_position)(long param_1,uint param_2,uint param_3);
    typedef void (*nds_set_screen_orientation)(unsigned int param_1);
    typedef void (*nds_get_gui_input)(long param_1,unsigned int *param_2);
    typedef long (*nds_savestate_index_timestamp)(long param_1,unsigned int param_2);
    typedef int (*nds_gamecard_close)(long param_1);
    typedef void (*nds_audio_exit)(long param_1);
    typedef FILE * (*nds_input_log_close)(long param_1);
    typedef int (*nds_uninitialize_memory)(long param_1);
    typedef void (*nds_platform_quit)(void);
    typedef unsigned long (*nds_save_directory_config_file)(long param_1,unsigned long param_2);
    typedef void (*nds_clear_gui_actions)(void);
    typedef void (*nds_screen_set_cursor_position)(unsigned int param_1,unsigned int param_2,unsigned int param_3);
    typedef void (*nds_convert_touch_coordinates)(int param_1,int param_2,uint *param_3,uint *param_4,int param_5);
 
    typedef unsigned long (*nds_load_config_file_binary)(long param_1, unsigned long param_2,int param_3);
    typedef void (*nds_save_config_file)(long param_1, unsigned long param_2,int param_3);
    typedef void (*nds_chomp_whitespace)(char *param_1);
    typedef void (*nds_set_screen_scale_factor)(unsigned int param_1);
    typedef void (*nds_set_screen_swap)(unsigned int param_1);
	typedef byte * (*nds_skip_whitespace)(byte *param_1);

	typedef unsigned char ** (*nds_create_menu_options)(long param_1,unsigned char  *param_2);
	typedef unsigned char ** (*nds_create_menu_controls)(long param_1,unsigned char  *param_2);
	typedef unsigned char ** (*nds_create_menu_firmware)(long param_1,unsigned char  *param_2);
	typedef void (*nds_draw_menu_option)(long param_1,unsigned long *param_2,int param_3);
	typedef int (*nds_action_select_menu)(long param_1,long param_2,int *param_3);
	typedef void (*nds_destroy_select_menu)(unsigned long param_1,long param_2);
	typedef void (*nds_select_cheat_menu)(long *param_1);
	typedef void (*nds_focus_savestate)(void);
	typedef void (*nds_modify_snapshot_bg)(void);
	typedef void (*nds_select_load_state)(long *param_1);
	typedef void (*nds_draw_numeric)(long param_1,unsigned long *param_2,int param_3);
	typedef uint (*nds_action_numeric)(unsigned long param_1,long param_2,uint *param_3);
	typedef uint (*nds_action_numeric_select)(unsigned long param_1,long param_2,uint *param_3);
	typedef void (*nds_select_save_state)(long *param_1);
	typedef int (*nds_action_select)(unsigned long param_1,long param_2,int *param_3);
	typedef void (*nds_select_load_game)(long *param_1);
	typedef void (*nds_select_restart)(long *param_1);
	typedef void (*nds_select_return)(long *param_1);
	typedef void (*nds_select_quit)(unsigned long *param_1);
	typedef void (*nds_draw_menu_main)(void);

	typedef void (*nds_draw_button_config)(long param_1,char **param_2,int param_3);
	typedef uint (*nds_action_button_config)(long param_1,long param_2,uint *param_3);
	typedef void (*nds_select_restore_default_controls)(long param_1);
	typedef void (*nds_select_delete_config_local)(long *param_1);
	typedef void (*nds_select_save_config_global)(long *param_1);
	typedef void (*nds_select_exit_current_menu)(long *param_1,long param_2);
	typedef void (*nds_draw_menu_controls)(long *param_1,long param_2);
	typedef void (*nds_focus_menu_none)(void);

	typedef void (*nds_platform_print_code)(unsigned long *param_1,ulong param_2);
	typedef void (*nds_print_string)(unsigned long param_1,unsigned int param_2,unsigned int param_3,unsigned int param_4, unsigned int param_5);
 	typedef ulong (*nds_platform_get_config_input)(void);
    typedef void (*nds_delay_us)(ulong param_1);
    
    typedef void (*nds_backup_save_part_0)(long param_1);

     typedef void (*nds_draw_numeric_labeled)(long param_1,unsigned long *param_2,int param_3);
     typedef void (*nds_select_save_config_local)(long *param_1);
     typedef void (*nds_draw_menu_options)(unsigned long param_1,long param_2);

     typedef void (*nds_select_load_game)(long *param_1);
     typedef int (*nds_load_file)(long *param_1,char **param_2,char *param_3);
     typedef int (*nds_load_nds)(long param_1,char *param_2);

     typedef void (*nds_load_logo)(long *param_1);
     typedef unsigned char ** (*nds_create_menu_main)(long param_1);
     typedef char (*nds_audio_pause)(long param_1);
     typedef void (*nds_set_screen_menu_on)(void);
     typedef void (*nds_draw_menu_bg)(unsigned long *param_1);
     typedef void (*nds_set_font_narrow)(void);
     typedef void (*nds_set_font_wide)(void);
     typedef void (*nds_update_screen_menu)(void);
     typedef void (*nds_audio_revert_pause_state)(unsigned long param_1,int param_2);
     typedef void (*nds_config_update_settings)(long param_1);
     typedef void (*nds_set_screen_menu_off)(void);
     typedef void (*nds_reset_system)(unsigned long *param_1);
     typedef void (*nds_get_ticks_us)(long *param_1);
    
    void detour_init(size_t page_size, const char *path);
    void detour_quit(void);
    void detour_hook(uint64_t old_func, uint64_t new_func);

    int dtr_quit(void);
    int dtr_savestate(int slot);
    int dtr_loadstate(int slot);
    int dtr_fastforward(uint8_t v);

   extern uint64_t base_addr_rx;

#endif

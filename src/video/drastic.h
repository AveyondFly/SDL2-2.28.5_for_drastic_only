#ifndef __DRASTIC_H__
#define __DRASTIC_H__

#pragma pack(1)

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef unsigned char    byte;

typedef unsigned char   undefined;
typedef unsigned char    undefined1;
typedef unsigned short    undefined2;
typedef unsigned int    undefined4;
typedef unsigned long    undefined8;


typedef struct
{
	wchar_t username[11];
	u32 language;
	u32 favorite_color;
	u32 birthday_month;
	u32 birthday_day;
} config_firmware_struct;

typedef struct
{
	config_firmware_struct firmware;
	char rom_directory[1024];
	u32 file_list_display_type;
	u32 frameskip_type;
	u32 frameskip_value;
	u32 show_frame_counter;
	u32 screen_orientation;
	u32 screen_scaling;
	u32 screen_swap;
	u32 savestate_number;
	u32 fast_forward;
	u32 enable_sound;
	u32 clock_speed;
	u32 threaded_3d;
	u32 mirror_touch;
	u32 compress_savestates;
	u32 savestates_snapshot;
	u32 enable_cheats;
	u32 unzip_roms;
	u32 backup_in_savestates;
	u32 ignore_gamecard_limit;
	u32 frame_interval;
	u32 batch_threads_3d_count;
	u32 trim_roms;
	u32 fix_main_2d_screen;
	u32 disable_edge_marking;
	u32 hires_3d;
	u32 bypass_3d;
	u32 use_rtc_custom_time;
	time_t rtc_custom_time;
	u32 rtc_system_time;
	u16 controls_a[40];
	u16 controls_b[40];
	u64 controls_code_to_config_map[2048];
} config_struct;

typedef struct event_s event_struct;
struct event_s
{
	u32 cycles_forward;
//	event_callback_type callback;
	void *data;
	event_struct *next;
	event_struct *previous;
	u8 type;
};

typedef struct
{
	event_struct event_storage[16];
	event_struct *base;
} event_list_struct;

typedef struct input_s input_struct;
typedef struct system_s system_struct;

struct input_s
{
	u8 capture_buffer[0x80000];
	u8 *capture_ptr;
	system_struct *system;
	u32 button_status;
	u32 touch_x;
	u32 touch_y;
	u8 touch_status;
	u8 touch_pressure;
	u32 last_button_status;
	u32 last_touch_x;
	u32 last_touch_y;
	u8 last_touch_status;
	FILE *log_file;
	u8 log_mode;
};

struct system_s
{
	u64 frame_number;
	u64 global_cycles;
	u32 cycles_to_next_event;
	u16 scanline_number;
	u8 undefined_1[2];
	event_list_struct event_list; // 0x184
	u8 undefined_2[4];
	//gamecard_struct gamecard; // 0x2cf0;
	unsigned char dummy1[0x2cf0];
	//spi_peripherals_struct spi_peripherals; // 0x2448
	unsigned char dummy2[0x2448];
	//rtc_struct rtc; // 0x1c
	unsigned char dummy3[0x1c];
	input_struct input; //0x80030
	u8 undefined_3[4];
	config_struct config; // 0x4558
	//benchmark_struct benchmark; // 0x68
	unsigned char dummy4[0x68];
	char gamecard_path[1024];
	char root_path[1024];
	char user_root_path[1024];
	char gamecard_filename[1024];
	char gamecard_name[1024];
	
};



#endif

/**
 * LVGL 8.x Configuration for SmartHub
 * STM32MP157F-DK2 with Linux framebuffer
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* Color depth: 16 (RGB565), 24 (RGB888), or 32 (ARGB8888) */
#define LV_COLOR_DEPTH 32

/* Swap the 2 bytes of RGB565 color. */
#define LV_COLOR_16_SWAP 0

/* Enable more complex drawing routines to manage screens transparency */
#define LV_COLOR_SCREEN_TRANSP 0

/* Adjust color mix functions rounding */
#define LV_COLOR_MIX_ROUND_OFS 128

/* Images pixels with this color will not be drawn */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*====================
   MEMORY SETTINGS
 *====================*/

/* Size of the memory available for `lv_mem_alloc()` in bytes */
#define LV_MEM_SIZE (256U * 1024U)

/* Use the standard `malloc` and `free` */
#define LV_MEM_CUSTOM 0

/* Number of the intermediate memory buffer used during rendering */
#define LV_MEM_BUF_MAX_NUM 16

/* Use the standard `memcpy` and `memset` */
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL SETTINGS
 *====================*/

/* Default display refresh period in ms */
#define LV_DISP_DEF_REFR_PERIOD 16

/* Input device read period in milliseconds */
#define LV_INDEV_DEF_READ_PERIOD 16

/* Use a custom tick source */
#define LV_TICK_CUSTOM 0

/* Default Dot Per Inch */
#define LV_DPI_DEF 130

/*====================
   FEATURE CONFIGURATION
 *====================*/

/*-------------
 * Drawing
 *-----------*/

/* Enable complex draw engine */
#define LV_DRAW_COMPLEX 1

/* If a widget has `style_opa < 255` enable to buffer it */
#define LV_DRAW_SW_COMPLEX_BUF_TEMP_ARRAY_SIZE (100 * 1024)

/* Shadow cache size */
#define LV_SHADOW_CACHE_SIZE 0

/* Set number of maximally cached circle data */
#define LV_CIRCLE_CACHE_SIZE 4

/* Image decode layer count */
#define LV_IMG_CACHE_DEF_SIZE 0

/* Number of image decoders to allow */
#define LV_IMG_CACHE_DEF_SIZE 1

/* Maximum buffer size to allocate for rotation */
#define LV_DISP_ROT_MAX_BUF (10*1024)

/*-------------
 * GPU
 *-----------*/

/* Use ARM's 2D GPU */
#define LV_USE_GPU_ARM2D 0

/* Use STM32's DMA2D */
#define LV_USE_GPU_STM32_DMA2D 0

/* Use NXP's GPU iMX RTxxx */
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0

/* Use SDL2 renderer */
#define LV_USE_GPU_SDL 0

/*-------------
 * Logging
 *-----------*/

/* Enable logging */
#define LV_USE_LOG 1

#if LV_USE_LOG
/* Log level */
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

/* Print log with printf */
#define LV_LOG_PRINTF 1

/* Enable/disable LV_LOG_TRACE */
#define LV_LOG_TRACE_MEM        0
#define LV_LOG_TRACE_TIMER      0
#define LV_LOG_TRACE_INDEV      0
#define LV_LOG_TRACE_DISP_REFR  0
#define LV_LOG_TRACE_EVENT      0
#define LV_LOG_TRACE_OBJ_CREATE 0
#define LV_LOG_TRACE_LAYOUT     0
#define LV_LOG_TRACE_ANIM       0
#endif

/*-------------
 * Asserts
 *-----------*/

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* Add a custom handler when assert happens */
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);

/*-------------
 * Others
 *-----------*/

/* Show CPU and FPS count */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT

/* Show memory usage */
#define LV_USE_MEM_MONITOR 0
#define LV_USE_MEM_MONITOR_POS LV_ALIGN_BOTTOM_LEFT

/* Draw random colored rectangles over the redrawn areas */
#define LV_USE_REFR_DEBUG 0

/* sprintf formatting */
#define LV_SPRINTF_CUSTOM 0
#define LV_SPRINTF_USE_FLOAT 0

/* User data in objects */
#define LV_USE_USER_DATA 1

/* Garbage Collector settings */
#define LV_ENABLE_GC 0

/*====================
   FONT USAGE
 *====================*/

/* Montserrat fonts */
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    0
#define LV_FONT_MONTSERRAT_12    1
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_MONTSERRAT_18    1
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_MONTSERRAT_22    0
#define LV_FONT_MONTSERRAT_24    1
#define LV_FONT_MONTSERRAT_26    0
#define LV_FONT_MONTSERRAT_28    0
#define LV_FONT_MONTSERRAT_30    0
#define LV_FONT_MONTSERRAT_32    1
#define LV_FONT_MONTSERRAT_34    0
#define LV_FONT_MONTSERRAT_36    0
#define LV_FONT_MONTSERRAT_38    0
#define LV_FONT_MONTSERRAT_40    0
#define LV_FONT_MONTSERRAT_42    0
#define LV_FONT_MONTSERRAT_44    0
#define LV_FONT_MONTSERRAT_46    0
#define LV_FONT_MONTSERRAT_48    0

/* Other built-in fonts */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK            0
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Enable FreeType */
#define LV_USE_FREETYPE 0

/*====================
   TEXT SETTINGS
 *====================*/

/* Select character encoding */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Can break lines on these chars */
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/* If a word is longer than N characters, break it */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/* Minimum characters before breaking a long word */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/* Minimum characters after breaking a long word */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/* Default text color */
#define LV_TXT_COLOR_CMD "#"

/* Support bidirectional text */
#define LV_USE_BIDI 0

/* Support Arabic/Persian */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   WIDGET USAGE
 *====================*/

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1

/*====================
   EXTRA COMPONENTS
 *====================*/

/* Widgets */
#define LV_USE_ANIMIMG    1
#define LV_USE_CALENDAR   1
#define LV_USE_CHART      1
#define LV_USE_COLORWHEEL 1
#define LV_USE_IMGBTN     1
#define LV_USE_KEYBOARD   1
#define LV_USE_LED        1
#define LV_USE_LIST       1
#define LV_USE_MENU       1
#define LV_USE_METER      1
#define LV_USE_MSGBOX     1
#define LV_USE_SPAN       1
#define LV_USE_SPINBOX    1
#define LV_USE_SPINNER    1
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   1
#define LV_USE_WIN        1

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_USE_THEME_BASIC 1
#define LV_USE_THEME_MONO 1

/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* File system */
#define LV_USE_FS_STDIO 1
#if LV_USE_FS_STDIO
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH "/opt/smarthub/assets"
#define LV_FS_STDIO_CACHE_SIZE 0
#endif

#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_FATFS 0

/* Image decoders */
#define LV_USE_PNG 0
#define LV_USE_BMP 1
#define LV_USE_SJPG 0
#define LV_USE_GIF 1

/* Others */
#define LV_USE_SNAPSHOT 1
#define LV_USE_MONKEY 0
#define LV_USE_GRIDNAV 1
#define LV_USE_FRAGMENT 1
#define LV_USE_IMGFONT 0
#define LV_USE_MSG 1
#define LV_USE_IME_PINYIN 0

/*====================
   LINUX DRIVERS
 *====================*/

/* Enable Linux framebuffer driver */
#define LV_USE_LINUX_FBDEV 1

/* Enable Linux DRM driver */
#define LV_USE_LINUX_DRM 0

/* Enable Linux evdev input driver */
#define LV_USE_EVDEV 1

/* Enable SDL for simulation */
#define LV_USE_SDL 0

#endif /* LV_CONF_H */

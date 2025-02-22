#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <mach/platform.h>
#include <mach/display.h>

/* Default to lcd vs070cxn at070tn92 */
int CFG_DISP_PRI_SCREEN_LAYER = 0;
int CFG_DISP_PRI_SCREEN_RGB_FORMAT = MLC_RGBFMT_X8R8G8B8;
int CFG_DISP_PRI_SCREEN_PIXEL_BYTE = 4;
int CFG_DISP_PRI_SCREEN_COLOR_KEY = 0x090909;
int CFG_DISP_PRI_VIDEO_PRIORITY = 2;
int CFG_DISP_PRI_BACK_GROUND_COLOR = 0x000000;
int CFG_DISP_PRI_MLC_INTERLACE = 0;

int	CFG_DISP_PRI_LCD_WIDTH_MM = 155;
int	CFG_DISP_PRI_LCD_HEIGHT_MM = 93;
int CFG_DISP_PRI_RESOL_WIDTH = 800;
int CFG_DISP_PRI_RESOL_HEIGHT = 480;
int CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 10;
int CFG_DISP_PRI_HSYNC_BACK_PORCH = 36;
int CFG_DISP_PRI_HSYNC_FRONT_PORCH = 80;
int CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
int CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 8;
int CFG_DISP_PRI_VSYNC_BACK_PORCH = 15;
int CFG_DISP_PRI_VSYNC_FRONT_PORCH = 22;
int CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 1;

int CFG_DISP_PRI_CLKGEN0_SOURCE = DPC_VCLK_SRC_PLL2;
int CFG_DISP_PRI_CLKGEN0_DIV = 23;
int CFG_DISP_PRI_CLKGEN0_DELAY = 0;
int CFG_DISP_PRI_CLKGEN0_INVERT = 0;
int CFG_DISP_PRI_CLKGEN1_SOURCE = DPC_VCLK_SRC_VCLK2;
int CFG_DISP_PRI_CLKGEN1_DIV = 1;
int CFG_DISP_PRI_CLKGEN1_DELAY = 0;
int CFG_DISP_PRI_CLKGEN1_INVERT = 0;
int CFG_DISP_PRI_CLKSEL1_SELECT = 0;
int CFG_DISP_PRI_PADCLKSEL = DPC_PADCLKSEL_VCLK;
int	CFG_DISP_PRI_PIXEL_CLOCK = (710000000 / 23);
int	CFG_DISP_PRI_OUT_SWAPRB = 0;
int CFG_DISP_PRI_OUT_FORMAT = DPC_FORMAT_RGB888;
int CFG_DISP_PRI_OUT_YCORDER = DPC_YCORDER_CbYCrY;
int CFG_DISP_PRI_OUT_INTERLACE = 0;
int CFG_DISP_PRI_OUT_INVERT_FIELD = 0;
int CFG_DISP_LCD_MPY_TYPE = 0;
int CFG_DISP_LVDS_LCD_FORMAT = LVDS_LCDFORMAT_JEIDA;
int CFG_DISP_HDMI_USING = 0;

int CFG_DISP_MIPI_PLLPMS = 0x2281;
int CFG_DISP_MIPI_BANDCTL = 0x7;
int CFG_DISP_MIPI_PLLCTL = 0;
int CFG_DISP_MIPI_DPHYCTL = 0;
static struct mipi_reg_val mipidef[] = {
	{0,    0,     0, {0}},
};
struct mipi_reg_val * CFG_DISP_MIPI_INIT_DATA = &mipidef[0];

static unsigned char lcdname[32] = "at070tn92";
static int __init lcd_setup(char * str)
{
    if((str != NULL) && (*str != '\0'))
    	strcpy(lcdname, str);

	extern void GEC6818_lcd_select(void);
	GEC6818_lcd_select();
	return 1;
}
__setup("lcd=", lcd_setup);

void GEC6818_lcd_select(void)
{
	if(strcmp(lcdname, "vga-1024x768") == 0)
	{
		CFG_DISP_PRI_RESOL_WIDTH = 1024;
		CFG_DISP_PRI_RESOL_HEIGHT = 768;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 136;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 160;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 24;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 0;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 6;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 29;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 3;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 0;

		CFG_DISP_PRI_CLKGEN0_DIV = 12;
		CFG_DISP_PRI_PIXEL_CLOCK = (780000000 / CFG_DISP_PRI_CLKGEN0_DIV);
	}
	else if(strcmp(lcdname, "vga-1440x900") == 0)
	{
		CFG_DISP_PRI_RESOL_WIDTH = 1440;
		CFG_DISP_PRI_RESOL_HEIGHT = 900;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 32;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 80;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 48;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 6;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 17;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 3;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 0;

		CFG_DISP_PRI_CLKGEN0_DIV = 8;
		CFG_DISP_PRI_PIXEL_CLOCK = (710000000 / CFG_DISP_PRI_CLKGEN0_DIV);
	}
	else if(strcmp(lcdname, "hdmi-720p") == 0)
	{
		CFG_DISP_PRI_RESOL_WIDTH = 1280;
		CFG_DISP_PRI_RESOL_HEIGHT = 720;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 20;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 140;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 160;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 3;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 20;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 12;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 1;

		CFG_DISP_PRI_CLKGEN0_DIV = 5;
		CFG_DISP_PRI_PIXEL_CLOCK = (780000000 / CFG_DISP_PRI_CLKGEN0_DIV);
		CFG_DISP_HDMI_USING = 1;
	}
	else if(strcmp(lcdname, "hdmi-1080p") == 0)
	{
		CFG_DISP_PRI_RESOL_WIDTH = 1920;
		CFG_DISP_PRI_RESOL_HEIGHT = 1080;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 20;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 140;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 160;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 3;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 20;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 12;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 1;

		CFG_DISP_PRI_CLKGEN0_DIV = 5;
		CFG_DISP_PRI_PIXEL_CLOCK = (780000000 / CFG_DISP_PRI_CLKGEN0_DIV);
		CFG_DISP_HDMI_USING = 1;
	}
	else if(strcmp(lcdname, "vs070cxn") == 0)
	{
		CFG_DISP_PRI_RESOL_WIDTH = 1024;
		CFG_DISP_PRI_RESOL_HEIGHT = 600;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 20;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 140;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 160;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 3;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 20;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 12;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 1;

		CFG_DISP_PRI_CLKGEN0_DIV = 15;
		CFG_DISP_PRI_PIXEL_CLOCK = (780000000 / CFG_DISP_PRI_CLKGEN0_DIV); //65M
	}
	else if(strcmp(lcdname, "at070tn92") == 0)
	{
		CFG_DISP_PRI_RESOL_WIDTH = 800;
		CFG_DISP_PRI_RESOL_HEIGHT = 480;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 10;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 36;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 80;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 8;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 15;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 22;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 1;

		CFG_DISP_PRI_CLKGEN0_DIV = 23;
		CFG_DISP_PRI_PIXEL_CLOCK = (780000000 / CFG_DISP_PRI_CLKGEN0_DIV); //33M
	}
	else if(strcmp(lcdname, "b116xtn04") == 0)
	{
		CFG_DISP_PRI_RESOL_WIDTH = 1366;
		CFG_DISP_PRI_RESOL_HEIGHT = 768;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 2;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 44;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 44;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 3;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 11;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 11;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 0;

		CFG_DISP_PRI_CLKGEN0_DIV = 12;
		CFG_DISP_PRI_PIXEL_CLOCK = (780000000 / CFG_DISP_PRI_CLKGEN0_DIV); //65M
	}
	else if(strcmp(lcdname, "wy070ml") == 0)
	{
		static struct mipi_reg_val cfg[] = {
			{0x05, 0x00,  1, {0x01}},
			{0xff, 30,    0, {0}},
			{0x15, 0x80,  1, {0x47}},
			{0x15, 0x81,  1, {0x40}},
			{0x15, 0x82,  1, {0x04}},
			{0x15, 0x83,  1, {0x77}},
			{0x15, 0x84,  1, {0x0f}},
			{0x15, 0x85,  1, {0x70}},
			{0x15, 0x86,  1, {0x70}},
			{0,    0,     0, {0}},
		};

		CFG_DISP_PRI_RESOL_WIDTH = 1024;
		CFG_DISP_PRI_RESOL_HEIGHT = 600;

		CFG_DISP_PRI_HSYNC_SYNC_WIDTH = 20;
		CFG_DISP_PRI_HSYNC_BACK_PORCH = 140;
		CFG_DISP_PRI_HSYNC_FRONT_PORCH = 160;
		CFG_DISP_PRI_HSYNC_ACTIVE_HIGH = 1;
		CFG_DISP_PRI_VSYNC_SYNC_WIDTH = 3;
		CFG_DISP_PRI_VSYNC_BACK_PORCH = 20;
		CFG_DISP_PRI_VSYNC_FRONT_PORCH = 12;
		CFG_DISP_PRI_VSYNC_ACTIVE_HIGH = 1;

		CFG_DISP_PRI_CLKGEN0_DIV = 12;
		CFG_DISP_PRI_PIXEL_CLOCK = (780000000 / CFG_DISP_PRI_CLKGEN0_DIV);

		CFG_DISP_MIPI_PLLPMS = 0x2281;
		CFG_DISP_MIPI_BANDCTL = 0x7;
		CFG_DISP_MIPI_PLLCTL = 0;
		CFG_DISP_MIPI_DPHYCTL = 0;
		CFG_DISP_MIPI_INIT_DATA = &cfg[0];
	}
}




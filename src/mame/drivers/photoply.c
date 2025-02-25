/*******************************************************************************************************

Photo Play (c) 199? Funworld

Preliminary driver by Angelo Salese

TODO:
- asserts with "i386: Invalid REP/opcode 2E combination", but it puts something if you disable that
  assert (i386\i386ops.c, line:1083), what is the cause?
- Puts "BIOS ROM checksum error" on POST just like California Chase, maybe a CPU bug or missing MMU?
- Puts a FDC error, needs a DASM investigation / work-around.

*******************************************************************************************************/

#include "emu.h"
#include "cpu/i386/i386.h"
#include "memconv.h"
#include "devconv.h"
#include "machine/8237dma.h"
#include "machine/pic8259.h"
#include "machine/pit8253.h"
#include "machine/mc146818.h"
#include "machine/pcshare.h"
#include "machine/pci.h"
#include "machine/8042kbdc.h"
#include "machine/pckeybrd.h"
#include "machine/idectrl.h"


class photoply_state : public driver_device
{
public:
	photoply_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	UINT32 *m_vga_vram;
	UINT8 m_vga_regs[0x19];
	int m_dma_channel;
	UINT8 m_dma_offset[2][4];
	UINT8 m_at_pages[0x10];
	UINT8 m_vga_address;
	struct { int r,g,b,offs,offs_internal; } m_pal;

	device_t	*m_pit8253;
	device_t	*m_pic8259_1;
	device_t	*m_pic8259_2;
	device_t	*m_dma8237_1;
	device_t	*m_dma8237_2;
};


#define SET_VISIBLE_AREA(_x_,_y_) \
	{ \
	rectangle visarea; \
	visarea.min_x = 0; \
	visarea.max_x = _x_-1; \
	visarea.min_y = 0; \
	visarea.max_y = _y_-1; \
	machine.primary_screen->configure(_x_, _y_, visarea, machine.primary_screen->frame_period().attoseconds ); \
	} \

#define RES_320x200 0
#define RES_640x200 1

static VIDEO_START(photoply)
{
}

static void cga_alphanumeric_tilemap(running_machine &machine, bitmap_t *bitmap,const rectangle *cliprect,UINT16 size,UINT32 map_offs,UINT8 gfx_num)
{
	photoply_state *state = machine.driver_data<photoply_state>();
	UINT32 offs,x,y,max_x,max_y;
	int tile,color;

	/*define the visible area*/
	switch(size)
	{
		case RES_320x200:
			SET_VISIBLE_AREA(320,200);
			max_x = 40;
			max_y = 25;
			break;
		case RES_640x200:
			SET_VISIBLE_AREA(640,200);
			max_x = 80;
			max_y = 25;
			break;
		default:
			fatalerror("Unknown size");
	}

	offs = map_offs;

	for(y=0;y<max_y;y++)
		for(x=0;x<max_x;x+=2)
		{
			tile =  (state->m_vga_vram[offs] & 0x00ff0000)>>16;
			color = (state->m_vga_vram[offs] & 0xff000000)>>24;

			drawgfx_opaque(bitmap,cliprect,machine.gfx[gfx_num],
					tile,
					color,
					0,0,
					(x+1)*8,y*8);


			tile =  (state->m_vga_vram[offs] & 0x000000ff);
			color = (state->m_vga_vram[offs] & 0x0000ff00)>>8;

			drawgfx_opaque(bitmap,cliprect,machine.gfx[gfx_num],
					tile,
					color,
					0,0,
					(x+0)*8,y*8);

			offs++;
		}
}


static SCREEN_UPDATE(photoply)
{
	cga_alphanumeric_tilemap(screen->machine(),bitmap,cliprect,RES_640x200,0x18000/4,0);

	return 0;
}

/******************
DMA8237 Controller
******************/


static WRITE_LINE_DEVICE_HANDLER( pc_dma_hrq_changed )
{
	cputag_set_input_line(device->machine(), "maincpu", INPUT_LINE_HALT, state ? ASSERT_LINE : CLEAR_LINE);

	/* Assert HLDA */
	i8237_hlda_w( device, state );
}


static READ8_HANDLER( pc_dma_read_byte )
{
	photoply_state *state = space->machine().driver_data<photoply_state>();
	offs_t page_offset = (((offs_t) state->m_dma_offset[0][state->m_dma_channel]) << 16)
		& 0xFF0000;

	return space->read_byte(page_offset + offset);
}


static WRITE8_HANDLER( pc_dma_write_byte )
{
	photoply_state *state = space->machine().driver_data<photoply_state>();
	offs_t page_offset = (((offs_t) state->m_dma_offset[0][state->m_dma_channel]) << 16)
		& 0xFF0000;

	space->write_byte(page_offset + offset, data);
}

static READ8_HANDLER(dma_page_select_r)
{
	photoply_state *state = space->machine().driver_data<photoply_state>();
	UINT8 data = state->m_at_pages[offset % 0x10];

	switch(offset % 8)
	{
	case 1:
		data = state->m_dma_offset[(offset / 8) & 1][2];
		break;
	case 2:
		data = state->m_dma_offset[(offset / 8) & 1][3];
		break;
	case 3:
		data = state->m_dma_offset[(offset / 8) & 1][1];
		break;
	case 7:
		data = state->m_dma_offset[(offset / 8) & 1][0];
		break;
	}
	return data;
}


static WRITE8_HANDLER(dma_page_select_w)
{
	photoply_state *state = space->machine().driver_data<photoply_state>();
	state->m_at_pages[offset % 0x10] = data;

	switch(offset % 8)
	{
	case 1:
		state->m_dma_offset[(offset / 8) & 1][2] = data;
		break;
	case 2:
		state->m_dma_offset[(offset / 8) & 1][3] = data;
		break;
	case 3:
		state->m_dma_offset[(offset / 8) & 1][1] = data;
		break;
	case 7:
		state->m_dma_offset[(offset / 8) & 1][0] = data;
		break;
	}
}

static void set_dma_channel(device_t *device, int channel, int state)
{
	photoply_state *drvstate = device->machine().driver_data<photoply_state>();
	if (!state) drvstate->m_dma_channel = channel;
}

static WRITE_LINE_DEVICE_HANDLER( pc_dack0_w ) { set_dma_channel(device, 0, state); }
static WRITE_LINE_DEVICE_HANDLER( pc_dack1_w ) { set_dma_channel(device, 1, state); }
static WRITE_LINE_DEVICE_HANDLER( pc_dack2_w ) { set_dma_channel(device, 2, state); }
static WRITE_LINE_DEVICE_HANDLER( pc_dack3_w ) { set_dma_channel(device, 3, state); }

static I8237_INTERFACE( dma8237_1_config )
{
	DEVCB_LINE(pc_dma_hrq_changed),
	DEVCB_NULL,
	DEVCB_MEMORY_HANDLER("maincpu", PROGRAM, pc_dma_read_byte),
	DEVCB_MEMORY_HANDLER("maincpu", PROGRAM, pc_dma_write_byte),
	{ DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL },
	{ DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL },
	{ DEVCB_LINE(pc_dack0_w), DEVCB_LINE(pc_dack1_w), DEVCB_LINE(pc_dack2_w), DEVCB_LINE(pc_dack3_w) }
};

static I8237_INTERFACE( dma8237_2_config )
{
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	DEVCB_NULL,
	{ DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL },
	{ DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL },
	{ DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL }
};


/******************
8259 IRQ controller
******************/

static WRITE_LINE_DEVICE_HANDLER( pic8259_1_set_int_line )
{
	cputag_set_input_line(device->machine(), "maincpu", 0, state ? HOLD_LINE : CLEAR_LINE);
}

static const struct pic8259_interface pic8259_1_config =
{
	DEVCB_LINE(pic8259_1_set_int_line)
};

static const struct pic8259_interface pic8259_2_config =
{
	DEVCB_DEVICE_LINE("pic8259_1", pic8259_ir2_w)
};

static IRQ_CALLBACK(irq_callback)
{
	photoply_state *state = device->machine().driver_data<photoply_state>();
	int r = 0;
	r = pic8259_acknowledge(state->m_pic8259_2);
	if (r==0)
	{
		r = pic8259_acknowledge(state->m_pic8259_1);
	}
	return r;
}

static WRITE_LINE_DEVICE_HANDLER( at_pit8254_out0_changed )
{
	photoply_state *drvstate = device->machine().driver_data<photoply_state>();
	if ( drvstate->m_pic8259_1 )
	{
		pic8259_ir0_w(drvstate->m_pic8259_1, state);
	}
}


static WRITE_LINE_DEVICE_HANDLER( at_pit8254_out2_changed )
{
	//at_speaker_set_input( state ? 1 : 0 );
}


static const struct pit8253_config at_pit8254_config =
{
	{
		{
			4772720/4,				/* heartbeat IRQ */
			DEVCB_NULL,
			DEVCB_LINE(at_pit8254_out0_changed)
		}, {
			4772720/4,				/* dram refresh */
			DEVCB_NULL,
			DEVCB_NULL
		}, {
			4772720/4,				/* pio port c pin 4, and speaker polling enough */
			DEVCB_NULL,
			DEVCB_LINE(at_pit8254_out2_changed)
		}
	}
};

static ADDRESS_MAP_START( photoply_map, AS_PROGRAM, 32 )
	AM_RANGE(0x00000000, 0x0009ffff) AM_RAM
	AM_RANGE(0x000a0000, 0x000bffff) AM_RAM AM_BASE_MEMBER(photoply_state, m_vga_vram)
	AM_RANGE(0x000c0000, 0x000c7fff) AM_RAM AM_REGION("video_bios", 0) //???
	AM_RANGE(0x000c8000, 0x000cffff) AM_RAM AM_REGION("video_bios", 0)
	AM_RANGE(0x000d0000, 0x000dffff) AM_RAM AM_REGION("ex_bios", 0)
	AM_RANGE(0x000e0000, 0x000fffff) AM_ROM AM_REGION("bios", 0)
	AM_RANGE(0xfffe0000, 0xffffffff) AM_ROM AM_REGION("bios", 0)
ADDRESS_MAP_END

static READ32_HANDLER( kludge_r )
{
	return space->machine().rand();
}

/* 3c8-3c9 -> ramdac*/
static WRITE32_HANDLER( vga_ramdac_w )
{
	photoply_state *state = space->machine().driver_data<photoply_state>();
	if (ACCESSING_BITS_0_7)
	{
		//printf("%02x X\n",data);
		state->m_pal.offs = state->m_pal.offs_internal = data;
	}
	if (ACCESSING_BITS_8_15)
	{
		//printf("%02x\n",data);
		data>>=8;
		switch(state->m_pal.offs_internal)
		{
			case 0:
				state->m_pal.r = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
				state->m_pal.offs_internal++;
				break;
			case 1:
				state->m_pal.g = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
				state->m_pal.offs_internal++;
				break;
			case 2:
				state->m_pal.b = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
				palette_set_color(space->machine(), 0x200+state->m_pal.offs, MAKE_RGB(state->m_pal.r, state->m_pal.g, state->m_pal.b));
				state->m_pal.offs_internal = 0;
				state->m_pal.offs++;
				break;
		}
	}
}

static WRITE32_HANDLER( vga_regs_w )
{
	photoply_state *state = space->machine().driver_data<photoply_state>();

	if (ACCESSING_BITS_0_7)
		state->m_vga_address = data;
	if (ACCESSING_BITS_8_15)
	{
		if(state->m_vga_address < 0x19)
		{
			state->m_vga_regs[state->m_vga_address] = data>>8;
			logerror("VGA reg %02x with data %02x\n",state->m_vga_address,state->m_vga_regs[state->m_vga_address]);
		}
		else
			logerror("Warning: used undefined VGA reg %02x with data %02x\n",state->m_vga_address,data>>8);
	}
}

static ADDRESS_MAP_START( photoply_io, AS_IO, 32 )
	AM_RANGE(0x0000, 0x001f) AM_DEVREADWRITE8("dma8237_1", i8237_r, i8237_w, 0xffffffff)
	AM_RANGE(0x0020, 0x003f) AM_DEVREADWRITE8("pic8259_1", pic8259_r, pic8259_w, 0xffffffff)
	AM_RANGE(0x0040, 0x005f) AM_DEVREADWRITE8("pit8254", pit8253_r, pit8253_w, 0xffffffff)
	AM_RANGE(0x0060, 0x006f) AM_READWRITE(kbdc8042_32le_r,			kbdc8042_32le_w)
	AM_RANGE(0x0070, 0x007f) AM_RAM//DEVREADWRITE8_MODERN("rtc", mc146818_device, read, write, 0xffffffff)
	AM_RANGE(0x0080, 0x009f) AM_READWRITE8(dma_page_select_r,dma_page_select_w, 0xffffffff)//TODO
	AM_RANGE(0x00a0, 0x00bf) AM_DEVREADWRITE8("pic8259_2", pic8259_r, pic8259_w, 0xffffffff)
	AM_RANGE(0x00c0, 0x00df) AM_DEVREADWRITE8("dma8237_2", i8237_r, i8237_w, 0xffff)
	AM_RANGE(0x0278, 0x027f) AM_RAM //parallel port 2
	AM_RANGE(0x0378, 0x037f) AM_RAM //parallel port
	AM_RANGE(0x03c0, 0x03c3) AM_RAM
	AM_RANGE(0x03cc, 0x03cf) AM_RAM
	AM_RANGE(0x03b4, 0x03b7) AM_WRITE(vga_regs_w)
	AM_RANGE(0x03c4, 0x03c7) AM_RAM //vga regs
	AM_RANGE(0x03c8, 0x03cb) AM_WRITE(vga_ramdac_w)
	AM_RANGE(0x03b8, 0x03bb) AM_READ(kludge_r) //hv_retrace
	AM_RANGE(0x03c8, 0x03cb) AM_READ(kludge_r) //hv_retrace
	AM_RANGE(0x03d4, 0x03d7) AM_WRITE(vga_regs_w)
	AM_RANGE(0x03d8, 0x03db) AM_RAM
	AM_RANGE(0x03bc, 0x03bf) AM_RAM //parallel port 3
	AM_RANGE(0x03f4, 0x03f7) AM_READ(kludge_r) // fdc
ADDRESS_MAP_END

#define AT_KEYB_HELPER(bit, text, key1) \
	PORT_BIT( bit, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME(text) PORT_CODE(key1)

static INPUT_PORTS_START( photoply )
	PORT_START("pc_keyboard_0")
	PORT_BIT ( 0x0001, 0x0000, IPT_UNUSED ) 	/* unused scancode 0 */
	AT_KEYB_HELPER( 0x0002, "Esc",          KEYCODE_Q           ) /* Esc                         01  81 */

	PORT_START("pc_keyboard_1")
	AT_KEYB_HELPER( 0x0020, "Y",            KEYCODE_Y           ) /* Y                           15  95 */
	AT_KEYB_HELPER( 0x1000, "Enter",        KEYCODE_ENTER       ) /* Enter                       1C  9C */

	PORT_START("pc_keyboard_2")

	PORT_START("pc_keyboard_3")
	AT_KEYB_HELPER( 0x0002, "N",            KEYCODE_N           ) /* N                           31  B1 */
	AT_KEYB_HELPER( 0x0800, "F1",           KEYCODE_S           ) /* F1                          3B  BB */

	PORT_START("pc_keyboard_4")

	PORT_START("pc_keyboard_5")

	PORT_START("pc_keyboard_6")
	AT_KEYB_HELPER( 0x0040, "(MF2)Cursor Up",		KEYCODE_UP          ) /* Up                          67  e7 */
	AT_KEYB_HELPER( 0x0080, "(MF2)Page Up",			KEYCODE_PGUP        ) /* Page Up                     68  e8 */
	AT_KEYB_HELPER( 0x0100, "(MF2)Cursor Left",		KEYCODE_LEFT        ) /* Left                        69  e9 */
	AT_KEYB_HELPER( 0x0200, "(MF2)Cursor Right",	KEYCODE_RIGHT       ) /* Right                       6a  ea */
	AT_KEYB_HELPER( 0x0800, "(MF2)Cursor Down",		KEYCODE_DOWN        ) /* Down                        6c  ec */
	AT_KEYB_HELPER( 0x1000, "(MF2)Page Down",		KEYCODE_PGDN        ) /* Page Down                   6d  ed */
	AT_KEYB_HELPER( 0x4000, "Del",      		    KEYCODE_A           ) /* Delete                      6f  ef */

	PORT_START("pc_keyboard_7")
INPUT_PORTS_END

static const rgb_t defcolors[]=
{
	MAKE_RGB(0x00,0x00,0x00),
	MAKE_RGB(0x00,0x00,0xaa),
	MAKE_RGB(0x00,0xaa,0x00),
	MAKE_RGB(0x00,0xaa,0xaa),
	MAKE_RGB(0xaa,0x00,0x00),
	MAKE_RGB(0xaa,0x00,0xaa),
	MAKE_RGB(0xaa,0xaa,0x00),
	MAKE_RGB(0xaa,0xaa,0xaa),
	MAKE_RGB(0x55,0x55,0x55),
	MAKE_RGB(0x55,0x55,0xff),
	MAKE_RGB(0x55,0xff,0x55),
	MAKE_RGB(0x55,0xff,0xff),
	MAKE_RGB(0xff,0x55,0x55),
	MAKE_RGB(0xff,0x55,0xff),
	MAKE_RGB(0xff,0xff,0x55),
	MAKE_RGB(0xff,0xff,0xff)
};

static PALETTE_INIT(pcat_286)
{
	/*Note:palette colors are 6bpp...
    xxxx xx--
    */
	int ix,iy;

	for(ix=0;ix<0x300;ix++)
		palette_set_color(machine, ix,MAKE_RGB(0x00,0x00,0x00));

	//regular colors
	for(iy=0;iy<0x10;iy++)
	{
		for(ix=0;ix<0x10;ix++)
		{
			palette_set_color(machine,(ix*2)+1+(iy*0x20),defcolors[ix]);
			palette_set_color(machine,(ix*2)+0+(iy*0x20),defcolors[iy]);
		}
	}

	//bitmap mode
	for(ix=0;ix<0x10;ix++)
		palette_set_color(machine, 0x200+ix,defcolors[ix]);
	//todo: 256 colors
}

static void photoply_set_keyb_int(running_machine &machine, int state)
{
	photoply_state *drvstate = machine.driver_data<photoply_state>();
	pic8259_ir1_w(drvstate->m_pic8259_1, state);
}


static MACHINE_START( photoply )
{
	photoply_state *state = machine.driver_data<photoply_state>();
//  bank = -1;
//  lastvalue = -1;
//  hv_blank = 0;
	device_set_irq_callback(machine.device("maincpu"), irq_callback);
	state->m_pit8253 = machine.device( "pit8254" );
	state->m_pic8259_1 = machine.device( "pic8259_1" );
	state->m_pic8259_2 = machine.device( "pic8259_2" );
	state->m_dma8237_1 = machine.device( "dma8237_1" );
	state->m_dma8237_2 = machine.device( "dma8237_2" );

	init_pc_common(machine, PCCOMMON_KEYBOARD_AT, photoply_set_keyb_int);
}

static const gfx_layout CGA_charlayout =
{
	8,8,
    256,
    1,
    { 0 },
    { 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
    8*8
};

static GFXDECODE_START( photoply )
	GFXDECODE_ENTRY( "video_bios", 0x6000+0xa5*8+7, CGA_charlayout,              0, 256 )
	//there's also a 8x16 entry (just after the 8x8)
GFXDECODE_END


static MACHINE_CONFIG_START( photoply, photoply_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", I486, 75000000)	/* I486DX4, 75 or 100 Mhz */
	MCFG_CPU_PROGRAM_MAP(photoply_map)
	MCFG_CPU_IO_MAP(photoply_io)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(64*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0*8, 64*8-1, 0*8, 32*8-1)
	MCFG_SCREEN_UPDATE(photoply)

	MCFG_GFXDECODE( photoply )

	MCFG_MACHINE_START(photoply)
	MCFG_MC146818_ADD( "rtc", MC146818_STANDARD )

//  MCFG_FRAGMENT_ADD( at_kbdc8042 )
	MCFG_PIC8259_ADD( "pic8259_1", pic8259_1_config )
	MCFG_PIC8259_ADD( "pic8259_2", pic8259_2_config )
	MCFG_I8237_ADD( "dma8237_1", XTAL_14_31818MHz/3, dma8237_1_config )
	MCFG_I8237_ADD( "dma8237_2", XTAL_14_31818MHz/3, dma8237_2_config )
	MCFG_PIT8254_ADD( "pit8254", at_pit8254_config )

	MCFG_PALETTE_INIT(pcat_286)

	MCFG_PALETTE_LENGTH(0x300)

	MCFG_VIDEO_START(photoply)
MACHINE_CONFIG_END


ROM_START(photoply)
	ROM_REGION(0x20000, "bios", 0)	/* motherboard bios */
	ROM_LOAD("award bootblock bios v1.0.bin", 0x000000, 0x20000, CRC(e96d1bbc) SHA1(64d0726c4e9ecee8fddf4cc39d92aecaa8184d5c) )

	ROM_REGION(0x10000, "ex_bios", 0) /* multifunction board with a ESS AudioDrive chip,  M27128A */
	ROM_LOAD("enhanced bios.bin", 0x000000, 0x4000, CRC(a216404e) SHA1(c9067cf87d5c8106de00866bb211eae3a6c02c65) )
	ROM_RELOAD(                   0x004000, 0x4000 )
	ROM_RELOAD(                   0x008000, 0x4000 )
	ROM_RELOAD(                   0x00c000, 0x4000 )

	ROM_REGION(0x8000, "video_bios", 0 )
	ROM_LOAD("vga.bin", 0x000000, 0x8000, CRC(7a859659) SHA1(ff667218261969c48082ec12aa91088a01b0cb2a) )

	DISK_REGION( "ide" )
	DISK_IMAGE( "photoply", 0,NO_DUMP )
ROM_END


GAME( 199?, photoply,  0,   photoply, photoply, 0, ROT0, "Funworld", "PhotoPlay", GAME_NOT_WORKING|GAME_NO_SOUND )

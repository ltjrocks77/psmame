/*

PCB# -   ANIMAL-01  Cosmo Electronics Corporation
68000 + OSC 8MHz
YM2203 + YM3014 + OSC 12MHz
DIPSw 8-position x2
RAM - 6264 (x2), TC5588 (x2), CXK5814 (x2)
3.6V battery

driver by David Haywood and few bits by Pierpaolo Prazzoli

*/

#include "emu.h"
#include "cpu/m68000/m68000.h"
#include "sound/2203intf.h"
#include "machine/nvram.h"


class pkscram_state : public driver_device
{
public:
	pkscram_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	UINT16 m_out;
	UINT8 m_interrupt_line_active;
	UINT16* m_pkscramble_fgtilemap_ram;
	UINT16* m_pkscramble_mdtilemap_ram;
	UINT16* m_pkscramble_bgtilemap_ram;
	tilemap_t *m_fg_tilemap;
	tilemap_t *m_md_tilemap;
	tilemap_t *m_bg_tilemap;
};


enum { interrupt_scanline=192 };


static WRITE16_HANDLER( pkscramble_fgtilemap_w )
{
	pkscram_state *state = space->machine().driver_data<pkscram_state>();
	COMBINE_DATA(&state->m_pkscramble_fgtilemap_ram[offset]);
	tilemap_mark_tile_dirty(state->m_fg_tilemap, offset >> 1);
}

static WRITE16_HANDLER( pkscramble_mdtilemap_w )
{
	pkscram_state *state = space->machine().driver_data<pkscram_state>();
	COMBINE_DATA(&state->m_pkscramble_mdtilemap_ram[offset]);
	tilemap_mark_tile_dirty(state->m_md_tilemap, offset >> 1);
}

static WRITE16_HANDLER( pkscramble_bgtilemap_w )
{
	pkscram_state *state = space->machine().driver_data<pkscram_state>();
	COMBINE_DATA(&state->m_pkscramble_bgtilemap_ram[offset]);
	tilemap_mark_tile_dirty(state->m_bg_tilemap, offset >> 1);
}

// input bit 0x20 in port1 should stay low until bit 0x20 is written here, then
// it should stay high for some time (currently we cheat keeping the input always active)
static WRITE16_HANDLER( pkscramble_output_w )
{
	pkscram_state *state = space->machine().driver_data<pkscram_state>();
	// OUTPUT
	// BIT
	// 0x0001 -> STL
	// 0x0002 -> SPL1
	// 0x0004 -> SPL2
	// 0x0008 -> SPL3
	// 0x0010 -> MSK
	// 0x0020 -> HPM
	// 0x0040 -> CNT1
	// 0x0080 -> CNT2
	// 0x0100 -> LED1
	// 0x0200 -> LED2
	// 0x0400 -> LED3
	// 0x0800 -> LED4
	// 0x1000 -> LED5

	// 0x2000 and 0x4000 are used too
	// 0x2000 -> vblank interrupt enable
	// 0x4000 -> set on every second frame - not used

	state->m_out = data;

	if (!(state->m_out & 0x2000) && state->m_interrupt_line_active)
	{
	    cputag_set_input_line(space->machine(), "maincpu", 1, CLEAR_LINE);
		state->m_interrupt_line_active = 0;
	}

	coin_counter_w(space->machine(), 0, data & 0x80);
}

static ADDRESS_MAP_START( pkscramble_map, AS_PROGRAM, 16 )
	ADDRESS_MAP_GLOBAL_MASK(0x7ffff)
	AM_RANGE(0x000000, 0x01ffff) AM_ROM
	AM_RANGE(0x040000, 0x0400ff) AM_RAM AM_SHARE("nvram")
	AM_RANGE(0x041000, 0x043fff) AM_RAM // main ram
	AM_RANGE(0x044000, 0x044fff) AM_RAM_WRITE(pkscramble_fgtilemap_w) AM_BASE_MEMBER(pkscram_state, m_pkscramble_fgtilemap_ram) // fg tilemap
	AM_RANGE(0x045000, 0x045fff) AM_RAM_WRITE(pkscramble_mdtilemap_w) AM_BASE_MEMBER(pkscram_state, m_pkscramble_mdtilemap_ram) // md tilemap (just a copy of fg?)
	AM_RANGE(0x046000, 0x046fff) AM_RAM_WRITE(pkscramble_bgtilemap_w) AM_BASE_MEMBER(pkscram_state, m_pkscramble_bgtilemap_ram) // bg tilemap
	AM_RANGE(0x047000, 0x047fff) AM_RAM // unused
	AM_RANGE(0x048000, 0x048fff) AM_RAM_WRITE(paletteram16_xRRRRRGGGGGBBBBB_word_w) AM_BASE_GENERIC(paletteram)
	AM_RANGE(0x049000, 0x049001) AM_READ_PORT("DSW")
	AM_RANGE(0x049004, 0x049005) AM_READ_PORT("INPUTS")
	AM_RANGE(0x049008, 0x049009) AM_WRITE(pkscramble_output_w)
	AM_RANGE(0x049010, 0x049011) AM_WRITENOP
	AM_RANGE(0x049014, 0x049015) AM_WRITENOP
	AM_RANGE(0x049018, 0x049019) AM_WRITENOP
	AM_RANGE(0x04901c, 0x04901d) AM_WRITENOP
	AM_RANGE(0x049020, 0x049021) AM_WRITENOP
	AM_RANGE(0x04900c, 0x04900f) AM_DEVREADWRITE8("ymsnd", ym2203_r, ym2203_w, 0x00ff)
	AM_RANGE(0x052086, 0x052087) AM_WRITENOP
ADDRESS_MAP_END


static INPUT_PORTS_START( pkscramble )
	PORT_START("DSW")
	PORT_DIPNAME( 0x0007, 0x0003, "Level" )
	PORT_DIPSETTING(      0x0000, "0" )
	PORT_DIPSETTING(      0x0001, "1" )
	PORT_DIPSETTING(      0x0002, "2" )
	PORT_DIPSETTING(      0x0003, "3" )
	PORT_DIPSETTING(      0x0004, "4" )
	PORT_DIPSETTING(      0x0005, "5" )
	PORT_DIPSETTING(      0x0006, "6" )
	PORT_DIPSETTING(      0x0007, "7" )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( On ) )
	PORT_DIPNAME( 0x0700, 0x0100, "Coin to Start" )
	PORT_DIPSETTING(      0x0100, "1" )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPSETTING(      0x0300, "3" )
	PORT_DIPSETTING(      0x0400, "4" )
	PORT_DIPSETTING(      0x0500, "5" )
	PORT_DIPSETTING(      0x0600, "6" )
	PORT_DIPSETTING(      0x0700, "7" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x3800, 0x0800, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x1800, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x2800, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(      0x3000, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(      0x3800, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(      0x0000, "No Credit" )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_HIGH )

	PORT_START("INPUTS")
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_BUTTON1 ) // Kick
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_BUTTON2 ) // Left
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_BUTTON3 ) // Center
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_BUTTON4 ) // Right
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW,  IPT_SPECIAL ) // Hopper status
	PORT_DIPNAME( 0x0040, 0x0000, "Coin Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SERVICE1 )
	PORT_BIT( 0xff00, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END


static TILE_GET_INFO( get_bg_tile_info )
{
	pkscram_state *state = machine.driver_data<pkscram_state>();
	int tile  = state->m_pkscramble_bgtilemap_ram[tile_index*2];
	int color = state->m_pkscramble_bgtilemap_ram[tile_index*2 + 1] & 0x7f;

	SET_TILE_INFO(0,tile,color,0);
}

static TILE_GET_INFO( get_md_tile_info )
{
	pkscram_state *state = machine.driver_data<pkscram_state>();
	int tile  = state->m_pkscramble_mdtilemap_ram[tile_index*2];
	int color = state->m_pkscramble_mdtilemap_ram[tile_index*2 + 1] & 0x7f;

	SET_TILE_INFO(0,tile,color,0);
}

static TILE_GET_INFO( get_fg_tile_info )
{
	pkscram_state *state = machine.driver_data<pkscram_state>();
	int tile  = state->m_pkscramble_fgtilemap_ram[tile_index*2];
	int color = state->m_pkscramble_fgtilemap_ram[tile_index*2 + 1] & 0x7f;

	SET_TILE_INFO(0,tile,color,0);
}

static TIMER_DEVICE_CALLBACK( scanline_callback )
{
	pkscram_state *state = timer.machine().driver_data<pkscram_state>();
	if (param == interrupt_scanline)
	{
    	if (state->m_out & 0x2000)
    		cputag_set_input_line(timer.machine(), "maincpu", 1, ASSERT_LINE);
		timer.adjust(timer.machine().primary_screen->time_until_pos(param + 1), param+1);
		state->m_interrupt_line_active = 1;
	}
	else
	{
		if (state->m_interrupt_line_active)
	    	cputag_set_input_line(timer.machine(), "maincpu", 1, CLEAR_LINE);
		timer.adjust(timer.machine().primary_screen->time_until_pos(interrupt_scanline), interrupt_scanline);
		state->m_interrupt_line_active = 0;
	}
}

static VIDEO_START( pkscramble )
{
	pkscram_state *state = machine.driver_data<pkscram_state>();
	state->m_bg_tilemap = tilemap_create(machine, get_bg_tile_info, tilemap_scan_rows, 8, 8,32,32);
	state->m_md_tilemap = tilemap_create(machine, get_md_tile_info, tilemap_scan_rows, 8, 8,32,32);
	state->m_fg_tilemap = tilemap_create(machine, get_fg_tile_info, tilemap_scan_rows, 8, 8,32,32);

	tilemap_set_transparent_pen(state->m_md_tilemap,15);
	tilemap_set_transparent_pen(state->m_fg_tilemap,15);
}

static SCREEN_UPDATE( pkscramble )
{
	pkscram_state *state = screen->machine().driver_data<pkscram_state>();
	tilemap_draw(bitmap,cliprect,state->m_bg_tilemap,0,0);
	tilemap_draw(bitmap,cliprect,state->m_md_tilemap,0,0);
	tilemap_draw(bitmap,cliprect,state->m_fg_tilemap,0,0);
	return 0;
}

static const gfx_layout tiles8x8_layout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 12, 8, 4, 0, 28, 24, 20, 16 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};


static GFXDECODE_START( pkscram )
	GFXDECODE_ENTRY( "gfx1", 0, tiles8x8_layout, 0, 0x80 )
GFXDECODE_END

static void irqhandler(device_t *device, int irq)
{
	pkscram_state *state = device->machine().driver_data<pkscram_state>();
	if(state->m_out & 0x10)
		cputag_set_input_line(device->machine(), "maincpu", 2, irq ? ASSERT_LINE : CLEAR_LINE);
}

static const ym2203_interface ym2203_config =
{
	{
		AY8910_LEGACY_OUTPUT,
		AY8910_DEFAULT_LOADS,
		DEVCB_NULL, DEVCB_NULL, DEVCB_NULL, DEVCB_NULL
	},
	irqhandler
};

static MACHINE_START( pkscramble)
{
	pkscram_state *state = machine.driver_data<pkscram_state>();
	state_save_register_global(machine, state->m_out);
	state_save_register_global(machine, state->m_interrupt_line_active);
}

static MACHINE_RESET( pkscramble)
{
	pkscram_state *state = machine.driver_data<pkscram_state>();
	state->m_out = 0;
	state->m_interrupt_line_active=0;
	timer_device *scanline_timer = machine.device<timer_device>("scan_timer");
	scanline_timer->adjust(machine.primary_screen->time_until_pos(interrupt_scanline), interrupt_scanline);
}

static MACHINE_CONFIG_START( pkscramble, pkscram_state )
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000, 8000000 )
	MCFG_CPU_PROGRAM_MAP(pkscramble_map)
	//MCFG_CPU_VBLANK_INT("screen", irq1_line_hold) /* only valid irq */

	MCFG_NVRAM_ADD_0FILL("nvram")

	MCFG_MACHINE_START(pkscramble)
	MCFG_MACHINE_RESET(pkscramble)

	MCFG_TIMER_ADD("scan_timer", scanline_callback)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MCFG_SCREEN_SIZE(32*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 0*8, 24*8-1)
	MCFG_SCREEN_UPDATE(pkscramble)

	MCFG_PALETTE_LENGTH(0x800)
	MCFG_GFXDECODE(pkscram)

	MCFG_VIDEO_START(pkscramble)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")

	MCFG_SOUND_ADD("ymsnd", YM2203, 12000000/4)
	MCFG_SOUND_CONFIG(ym2203_config)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.60)
MACHINE_CONFIG_END


ROM_START( pkscram )
	ROM_REGION( 0x20000, "maincpu", 0 ) /* 68k */
	ROM_LOAD16_BYTE( "pk1.6e", 0x00000, 0x10000, CRC(80e972e5) SHA1(cbbc6e1e3fbb65b40164e140f368d8fff85c1521) )
	ROM_LOAD16_BYTE( "pk2.6j", 0x00001, 0x10000, CRC(752c86d1) SHA1(2e0c669307bed6f9eab957b0e1316416e653a72f) )

	ROM_REGION( 0x40000, "gfx1", 0 ) /* gfx */
	ROM_LOAD16_BYTE( "pk3.1c", 0x00000, 0x20000, CRC(0b18f2bc) SHA1(32892589442884ba02a1c6059ecb94e4ef516b86) )
	ROM_LOAD16_BYTE( "pk4.1e", 0x00001, 0x20000, CRC(a232d993) SHA1(1b7b15cf0fabf3b2b2e429506a78ff4c08f4f7a5) )
ROM_END


GAME( 1993, pkscram, 0, pkscramble, pkscramble, 0, ROT0, "Cosmo Electronics Corporation", "PK Scramble", GAME_SUPPORTS_SAVE)

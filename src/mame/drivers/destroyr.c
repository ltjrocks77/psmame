/***************************************************************************

Atari Destroyer Driver

***************************************************************************/

#include "emu.h"
#include "cpu/m6800/m6800.h"
#include "deprecat.h"


class destroyr_state : public driver_device
{
public:
	destroyr_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	/* memory pointers */
	UINT8 *        m_major_obj_ram;
	UINT8 *        m_minor_obj_ram;
	UINT8 *        m_alpha_num_ram;

	/* video-related */
	int            m_cursor;
	int            m_wavemod;

	/* misc */
	int            m_potmask[2];
	int            m_potsense[2];
	int            m_attract;
	int            m_motor_speed;
	int            m_noise;

	/* devices */
	device_t *m_maincpu;
};


static SCREEN_UPDATE( destroyr )
{
	destroyr_state *state = screen->machine().driver_data<destroyr_state>();
	int i, j;

	bitmap_fill(bitmap, cliprect, 0);

	/* draw major objects */
	for (i = 0; i < 16; i++)
	{
		int attr = state->m_major_obj_ram[2 * i + 0] ^ 0xff;
		int horz = state->m_major_obj_ram[2 * i + 1];

		int num = attr & 3;
		int scan = attr & 4;
		int flipx = attr & 8;

		if (scan == 0)
		{
			if (horz >= 192)
				horz -= 256;
		}
		else
		{
			if (horz < 192)
				continue;
		}

		drawgfx_transpen(bitmap, cliprect, screen->machine().gfx[2], num, 0, flipx, 0, horz, 16 * i, 0);
	}

	/* draw alpha numerics */
	for (i = 0; i < 8; i++)
	{
		for (j = 0; j < 32; j++)
		{
			int num = state->m_alpha_num_ram[32 * i + j];

			drawgfx_transpen(bitmap, cliprect, screen->machine().gfx[0], num, 0, 0, 0, 8 * j, 8 * i, 0);
		}
	}

	/* draw minor objects */
	for (i = 0; i < 2; i++)
	{
		int num = i << 4 | (state->m_minor_obj_ram[i + 0] & 0xf);
		int horz = 256 - state->m_minor_obj_ram[i + 2];
		int vert = 256 - state->m_minor_obj_ram[i + 4];

		drawgfx_transpen(bitmap, cliprect, screen->machine().gfx[1], num, 0, 0, 0, horz, vert, 0);
	}

	/* draw waves */
	for (i = 0; i < 4; i++)
	{
		drawgfx_transpen(bitmap, cliprect, screen->machine().gfx[3], state->m_wavemod ? 1 : 0, 0, 0, 0, 64 * i, 0x4e, 0);
	}

	/* draw cursor */
	for (i = 0; i < 256; i++)
	{
		if (i & 4)
			*BITMAP_ADDR16(bitmap, state->m_cursor ^ 0xff, i) = 7;
	}
	return 0;
}

static TIMER_CALLBACK( destroyr_dial_callback )
{
	destroyr_state *state = machine.driver_data<destroyr_state>();
	int dial = param;

	/* Analog inputs come from the player's depth control potentiometer.
       The voltage is compared to a voltage ramp provided by a discrete
       analog circuit that conditions the VBLANK signal. When the ramp
       voltage exceeds the input voltage an NMI signal is generated. The
       computer then reads the VSYNC data functions to tell where the
       cursor should be located. */

	state->m_potsense[dial] = 1;

	if (state->m_potmask[dial])
	{
		device_set_input_line(state->m_maincpu, INPUT_LINE_NMI, PULSE_LINE);
	}
}


static TIMER_CALLBACK( destroyr_frame_callback )
{
	destroyr_state *state = machine.driver_data<destroyr_state>();
	state->m_potsense[0] = 0;
	state->m_potsense[1] = 0;

	/* PCB supports two dials, but cab has only got one */
	machine.scheduler().timer_set(machine.primary_screen->time_until_pos(input_port_read(machine, "PADDLE")), FUNC(destroyr_dial_callback));
	machine.scheduler().timer_set(machine.primary_screen->time_until_pos(0), FUNC(destroyr_frame_callback));
}


static MACHINE_RESET( destroyr )
{
	destroyr_state *state = machine.driver_data<destroyr_state>();

	machine.scheduler().timer_set(machine.primary_screen->time_until_pos(0), FUNC(destroyr_frame_callback));

	state->m_cursor = 0;
	state->m_wavemod = 0;
	state->m_potmask[0] = 0;
	state->m_potmask[1] = 0;
	state->m_potsense[0] = 0;
	state->m_potsense[1] = 0;
	state->m_attract = 0;
	state->m_motor_speed = 0;
	state->m_noise = 0;
}


static WRITE8_HANDLER( destroyr_misc_w )
{
	destroyr_state *state = space->machine().driver_data<destroyr_state>();

	/* bits 0 to 2 connect to the sound circuits */
	state->m_attract = data & 0x01;
	state->m_noise = data & 0x02;
	state->m_motor_speed = data & 0x04;
	state->m_potmask[0] = data & 0x08;
	state->m_wavemod = data & 0x10;
	state->m_potmask[1] = data & 0x20;

	coin_lockout_w(space->machine(), 0, !state->m_attract);
	coin_lockout_w(space->machine(), 1, !state->m_attract);
}


static WRITE8_HANDLER( destroyr_cursor_load_w )
{
	destroyr_state *state = space->machine().driver_data<destroyr_state>();
	state->m_cursor = data;
	watchdog_reset_w(space, offset, data);
}


static WRITE8_HANDLER( destroyr_interrupt_ack_w )
{
	destroyr_state *state = space->machine().driver_data<destroyr_state>();
	device_set_input_line(state->m_maincpu, 0, CLEAR_LINE);
}


static WRITE8_HANDLER( destroyr_output_w )
{
	if (offset & 8) destroyr_misc_w(space, 8, data);

	else switch (offset & 7)
	{
	case 0:
		set_led_status(space->machine(), 0, data & 1);
		break;
	case 1:
		set_led_status(space->machine(), 1, data & 1); /* no second LED present on cab */
		break;
	case 2:
		/* bit 0 => songate */
		break;
	case 3:
		/* bit 0 => launch */
		break;
	case 4:
		/* bit 0 => explosion */
		break;
	case 5:
		/* bit 0 => sonar */
		break;
	case 6:
		/* bit 0 => high explosion */
		break;
	case 7:
		/* bit 0 => low explosion */
		break;
	}
}


static READ8_HANDLER( destroyr_input_r )
{
	destroyr_state *state = space->machine().driver_data<destroyr_state>();

	if (offset & 1)
	{
		return input_port_read(space->machine(), "IN1");
	}

	else
	{
		UINT8 ret = input_port_read(space->machine(), "IN0");

		if (state->m_potsense[0] && state->m_potmask[0])
			ret |= 4;
		if (state->m_potsense[1] && state->m_potmask[1])
			ret |= 8;

		return ret;
	}
}


static READ8_HANDLER( destroyr_scanline_r )
{
	return space->machine().primary_screen->vpos();
}


static ADDRESS_MAP_START( destroyr_map, AS_PROGRAM, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0x7fff)
	AM_RANGE(0x0000, 0x00ff) AM_MIRROR(0xf00) AM_RAM
	AM_RANGE(0x1000, 0x1fff) AM_READWRITE(destroyr_input_r, destroyr_output_w)
	AM_RANGE(0x2000, 0x2fff) AM_READ_PORT("IN2")
	AM_RANGE(0x3000, 0x30ff) AM_MIRROR(0xf00) AM_WRITEONLY AM_BASE_MEMBER(destroyr_state, m_alpha_num_ram)
	AM_RANGE(0x4000, 0x401f) AM_MIRROR(0xfe0) AM_WRITEONLY AM_BASE_MEMBER(destroyr_state, m_major_obj_ram)
	AM_RANGE(0x5000, 0x5000) AM_MIRROR(0xff8) AM_WRITE(destroyr_cursor_load_w)
	AM_RANGE(0x5001, 0x5001) AM_MIRROR(0xff8) AM_WRITE(destroyr_interrupt_ack_w)
	AM_RANGE(0x5002, 0x5007) AM_MIRROR(0xff8) AM_WRITEONLY AM_BASE_MEMBER(destroyr_state, m_minor_obj_ram)
	AM_RANGE(0x6000, 0x6fff) AM_READ(destroyr_scanline_r)
	AM_RANGE(0x7000, 0x7fff) AM_ROM
ADDRESS_MAP_END


static INPUT_PORTS_START( destroyr )
	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED ) /* call 7400 */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED ) /* potsense1 */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED ) /* potsense2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_DIPNAME( 0xc0, 0x80, "Extended Play" ) PORT_DIPLOCATION("SW2:8,7")
	PORT_DIPSETTING( 0x40, "1500 points" )
	PORT_DIPSETTING( 0x80, "2500 points" )
	PORT_DIPSETTING( 0xc0, "3500 points" )
	PORT_DIPSETTING( 0x00, "never" )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_TILT )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) /* actually a lever */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_SERVICE( 0x08, IP_ACTIVE_LOW )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START("IN2")
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Coinage ) ) PORT_DIPLOCATION("SW:4,3")
	PORT_DIPSETTING( 0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING( 0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING( 0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING( 0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x08, "Play Time" ) PORT_DIPLOCATION("SW:2,1")
	PORT_DIPSETTING( 0x00, "50 seconds" )
	PORT_DIPSETTING( 0x04, "75 seconds" )
	PORT_DIPSETTING( 0x08, "100 seconds" )
	PORT_DIPSETTING( 0x0c, "125 seconds" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Language ) ) PORT_DIPLOCATION("SW2:5,6")
	PORT_DIPSETTING( 0x30, DEF_STR( German ) )
	PORT_DIPSETTING( 0x20, DEF_STR( French ) )
	PORT_DIPSETTING( 0x10, DEF_STR( Spanish ) )
	PORT_DIPSETTING( 0x00, DEF_STR( English ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("PADDLE")
	PORT_BIT( 0xff, 0x00, IPT_PADDLE_V ) PORT_MINMAX(0,160) PORT_SENSITIVITY(30) PORT_KEYDELTA(10) PORT_CENTERDELTA(0) PORT_REVERSE
INPUT_PORTS_END


static const gfx_layout destroyr_alpha_num_layout =
{
	8, 8,     /* width, height */
	64,       /* total         */
	1,        /* planes        */
	{ 0 },    /* plane offsets */
	{
		0x4, 0x5, 0x6, 0x7, 0xC, 0xD, 0xE, 0xF
	},
	{
		0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70
	},
	0x80      /* increment */
};


static const gfx_layout destroyr_minor_object_layout =
{
	16, 16,   /* width, height */
	32,       /* total         */
	1,        /* planes        */
	{ 0 },    /* plane offsets */
	{
	  0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F,
	  0x14, 0x15, 0x16, 0x17, 0x1C, 0x1D, 0x1E, 0x1F
	},
	{
	  0x000, 0x020, 0x040, 0x060, 0x080, 0x0a0, 0x0c0, 0x0e0,
	  0x100, 0x120, 0x140, 0x160, 0x180, 0x1a0, 0x1c0, 0x1e0
	},
	0x200     /* increment */
};

static const UINT32 destroyr_major_object_layout_xoffset[64] =
{
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
	0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E,
	0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E,
	0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E,
	0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0x4E,
	0x50, 0x52, 0x54, 0x56, 0x58, 0x5A, 0x5C, 0x5E,
	0x60, 0x62, 0x64, 0x66, 0x68, 0x6A, 0x6C, 0x6E,
	0x70, 0x72, 0x74, 0x76, 0x78, 0x7A, 0x7C, 0x7E
};

static const gfx_layout destroyr_major_object_layout =
{
	64, 16,   /* width, height */
	4,        /* total         */
	2,        /* planes        */
	{ 1, 0 },  /* plane offsets */
	EXTENDED_XOFFS,
	{
		0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
		0x400, 0x480, 0x500, 0x580, 0x600, 0x680, 0x700, 0x780
	},
	0x0800,   /* increment */
	destroyr_major_object_layout_xoffset,
	NULL
};

static const UINT32 destroyr_waves_layout_xoffset[64] =
{
	0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0A, 0x0B,
	0x10, 0x11, 0x12, 0x13, 0x18, 0x19, 0x1A, 0x1B,
	0x20, 0x21, 0x22, 0x23, 0x28, 0x29, 0x2A, 0x2B,
	0x30, 0x31, 0x32, 0x33, 0x38, 0x39, 0x3A, 0x3B,
	0x40, 0x41, 0x42, 0x43, 0x48, 0x49, 0x4A, 0x4B,
	0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5A, 0x5B,
	0x60, 0x61, 0x62, 0x63, 0x68, 0x69, 0x6A, 0x6B,
	0x70, 0x71, 0x72, 0x73, 0x78, 0x79, 0x7A, 0x7B
};

static const gfx_layout destroyr_waves_layout =
{
	64, 2,    /* width, height */
	2,        /* total         */
	1,        /* planes        */
	{ 0 },
	EXTENDED_XOFFS,
	{ 0x00, 0x80 },
	0x04,     /* increment */
	destroyr_waves_layout_xoffset,
	NULL
};


static GFXDECODE_START( destroyr )
	GFXDECODE_ENTRY( "gfx1", 0, destroyr_alpha_num_layout, 4, 1 )
	GFXDECODE_ENTRY( "gfx2", 0, destroyr_minor_object_layout, 4, 1 )
	GFXDECODE_ENTRY( "gfx3", 0, destroyr_major_object_layout, 0, 1 )
	GFXDECODE_ENTRY( "gfx4", 0, destroyr_waves_layout, 4, 1 )
GFXDECODE_END


static PALETTE_INIT( destroyr )
{
	palette_set_color(machine, 0, MAKE_RGB(0x00, 0x00, 0x00));   /* major objects */
	palette_set_color(machine, 1, MAKE_RGB(0x50, 0x50, 0x50));
	palette_set_color(machine, 2, MAKE_RGB(0xAF, 0xAF, 0xAF));
	palette_set_color(machine, 3, MAKE_RGB(0xFF ,0xFF, 0xFF));
	palette_set_color(machine, 4, MAKE_RGB(0x00, 0x00, 0x00));   /* alpha numerics, waves, minor objects */
	palette_set_color(machine, 5, MAKE_RGB(0xFF, 0xFF, 0xFF));
	palette_set_color(machine, 6, MAKE_RGB(0x00, 0x00, 0x00));   /* cursor */
	palette_set_color(machine, 7, MAKE_RGB(0x78, 0x78, 0x78));
}


static MACHINE_START( destroyr )
{
	destroyr_state *state = machine.driver_data<destroyr_state>();

	state->m_maincpu = machine.device("maincpu");

	state->save_item(NAME(state->m_cursor));
	state->save_item(NAME(state->m_wavemod));
	state->save_item(NAME(state->m_attract));
	state->save_item(NAME(state->m_motor_speed));
	state->save_item(NAME(state->m_noise));
	state->save_item(NAME(state->m_potmask));
	state->save_item(NAME(state->m_potsense));
}

static MACHINE_CONFIG_START( destroyr, destroyr_state )

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M6800, XTAL_12_096MHz / 16)
	MCFG_CPU_PROGRAM_MAP(destroyr_map)
	MCFG_CPU_VBLANK_INT_HACK(irq0_line_assert, 4)

	MCFG_MACHINE_START(destroyr)
	MCFG_MACHINE_RESET(destroyr)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(15750.0/262)
	MCFG_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MCFG_SCREEN_SIZE(256, 262)
	MCFG_SCREEN_VISIBLE_AREA(0, 255, 0, 239)
	MCFG_SCREEN_UPDATE(destroyr)

	MCFG_GFXDECODE(destroyr)
	MCFG_PALETTE_LENGTH(8)
	MCFG_PALETTE_INIT(destroyr)

	/* sound hardware */
MACHINE_CONFIG_END


ROM_START( destroyr )
	ROM_REGION( 0x8000, "maincpu", 0 )	/* program code */
	ROM_LOAD( "language.rom",0x7000, 0x0800, NO_DUMP ) // optional add-on translation rom
	ROM_LOAD( "30146-01.c3", 0x7800, 0x0800, CRC(e560c712) SHA1(0505ab57eee5421b4ff4e87d14505e02b18fd54c) )

	ROM_REGION( 0x0400, "gfx1", 0 )		/* alpha numerics */
	ROM_LOAD( "30135-01.p4", 0x0000, 0x0400, CRC(184824cf) SHA1(713cfd1d41ef7b1c345ea0038b652c4ba3f08301) )

	ROM_REGION( 0x0800, "gfx2", 0 )		/* minor objects */
	ROM_LOAD( "30132-01.f4", 0x0000, 0x0400, CRC(e09d3d55) SHA1(b26013397ef2cb32d0416ecb118387b9c2dffa9a) )
	ROM_LOAD( "30132-01.k4", 0x0400, 0x0400, CRC(e09d3d55) SHA1(b26013397ef2cb32d0416ecb118387b9c2dffa9a) ) // identical to f4

	ROM_REGION( 0x0400, "gfx3", 0 )		/* major objects */
	ROM_LOAD_NIB_HIGH( "30134-01.p8", 0x0000, 0x0400, CRC(6259e007) SHA1(049f5f7160305cb4f4b499dd113cb11eea73fc95) )
	ROM_LOAD_NIB_LOW ( "30133-01.n8", 0x0000, 0x0400, CRC(108d3e2c) SHA1(8c993369d37c6713670483af78e6d04d38f4b4fc) )

	ROM_REGION( 0x0020, "gfx4", 0 )		/* waves */
	ROM_LOAD( "30136-01.k2", 0x0000, 0x0020, CRC(532c11b1) SHA1(18ab5369a3f2cfcc9a44f38fa8649524bea5b203) )

	ROM_REGION( 0x0100, "user1", 0 )	/* sync (used for vsync/vblank signals, not hooked up yet) */
	ROM_LOAD( "30131-01.m1", 0x0000, 0x0100, CRC(b8094b4c) SHA1(82dc6799a19984f3b204ee3aeeb007e55afc8be3) )
ROM_END

ROM_START( destroyr1 )
	ROM_REGION( 0x8000, "maincpu", 0 )	/* program code */
	ROM_LOAD( "language.rom",0x7000, 0x0800, NO_DUMP ) // optional add-on translation rom
	ROM_LOAD_NIB_HIGH( "30142-01.f3", 0x7800, 0x0400, CRC(9e9a08d3) SHA1(eb31bab1537caf43ab8c3d23a6c9cc2009fcb98e) )
	ROM_LOAD_NIB_LOW ( "30141-01.e2", 0x7800, 0x0400, CRC(c924fbce) SHA1(53aa9a3c4c6e90fb94500ddfa6c2ae3076eee2ef) )
	ROM_LOAD_NIB_HIGH( "30144-01.j3", 0x7c00, 0x0400, CRC(0c7135c6) SHA1(6a0180353a0a6f34639dadc23179f6323aae8d62) )
	ROM_LOAD_NIB_LOW ( "30143-01.h2", 0x7c00, 0x0400, CRC(b946e6f0) SHA1(b906024bb0e03a644fff1d5516637c24916b096e) )

	ROM_REGION( 0x0400, "gfx1", 0 )		/* alpha numerics */
	ROM_LOAD( "30135-01.p4", 0x0000, 0x0400, CRC(184824cf) SHA1(713cfd1d41ef7b1c345ea0038b652c4ba3f08301) )

	ROM_REGION( 0x0800, "gfx2", 0 )		/* minor objects */
	ROM_LOAD( "30132-01.f4", 0x0000, 0x0400, CRC(e09d3d55) SHA1(b26013397ef2cb32d0416ecb118387b9c2dffa9a) )
	ROM_LOAD( "30132-01.k4", 0x0400, 0x0400, CRC(e09d3d55) SHA1(b26013397ef2cb32d0416ecb118387b9c2dffa9a) ) // identical to f4

	ROM_REGION( 0x0400, "gfx3", 0 )		/* major objects */
	ROM_LOAD_NIB_HIGH( "30134-01.p8", 0x0000, 0x0400, CRC(6259e007) SHA1(049f5f7160305cb4f4b499dd113cb11eea73fc95) )
	ROM_LOAD_NIB_LOW ( "30133-01.n8", 0x0000, 0x0400, CRC(108d3e2c) SHA1(8c993369d37c6713670483af78e6d04d38f4b4fc) )

	ROM_REGION( 0x0020, "gfx4", 0 )		/* waves */
	ROM_LOAD( "30136-01.k2", 0x0000, 0x0020, CRC(532c11b1) SHA1(18ab5369a3f2cfcc9a44f38fa8649524bea5b203) )

	ROM_REGION( 0x0100, "user1", 0 )	/* sync (used for vsync/vblank signals, not hooked up yet) */
	ROM_LOAD( "30131-01.m1", 0x0000, 0x0100, CRC(b8094b4c) SHA1(82dc6799a19984f3b204ee3aeeb007e55afc8be3) )
ROM_END


GAME( 1977, destroyr,  0,        destroyr, destroyr, 0, ORIENTATION_FLIP_X, "Atari", "Destroyer (version O2)", GAME_NO_SOUND )
GAME( 1977, destroyr1, destroyr, destroyr, destroyr, 0, ORIENTATION_FLIP_X, "Atari", "Destroyer (version O1)", GAME_NO_SOUND )

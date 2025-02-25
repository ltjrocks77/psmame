class clshroad_state : public driver_device
{
public:
	clshroad_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	UINT8 *m_vram_0;
	UINT8 *m_vram_1;
	UINT8 *m_vregs;
	tilemap_t *m_tilemap_0a;
	tilemap_t *m_tilemap_0b;
	tilemap_t *m_tilemap_1;
	UINT8 *m_spriteram;
	size_t m_spriteram_size;
};


/*----------- defined in video/clshroad.c -----------*/

WRITE8_HANDLER( clshroad_vram_0_w );
WRITE8_HANDLER( clshroad_vram_1_w );
WRITE8_HANDLER( clshroad_flipscreen_w );

PALETTE_INIT( firebatl );
PALETTE_INIT( clshroad );
VIDEO_START( firebatl );
VIDEO_START( clshroad );
SCREEN_UPDATE( clshroad );

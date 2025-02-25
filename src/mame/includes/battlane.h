/***************************************************************************

    Battle Lane Vol. 5

***************************************************************************/

class battlane_state : public driver_device
{
public:
	battlane_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	/* memory pointers */
	UINT8 *     m_tileram;
	UINT8 *     m_spriteram;

	/* video-related */
	tilemap_t     *m_bg_tilemap;
	bitmap_t    *m_screen_bitmap;
	int         m_video_ctrl;
	int         m_cpu_control;	/* CPU interrupt control register */

	/* devices */
	device_t *m_maincpu;
	device_t *m_subcpu;
};


/*----------- defined in video/battlane.c -----------*/

WRITE8_HANDLER( battlane_palette_w );
WRITE8_HANDLER( battlane_scrollx_w );
WRITE8_HANDLER( battlane_scrolly_w );
WRITE8_HANDLER( battlane_tileram_w );
WRITE8_HANDLER( battlane_spriteram_w );
WRITE8_HANDLER( battlane_bitmap_w );
WRITE8_HANDLER( battlane_video_ctrl_w );

VIDEO_START( battlane );
SCREEN_UPDATE( battlane );

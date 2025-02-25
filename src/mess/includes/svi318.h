/*****************************************************************************
 *
 * includes/svi318.h
 *
 * Spectravideo SVI-318 and SVI-328
 *
 ****************************************************************************/

#ifndef SVI318_H_
#define SVI318_H_

#include "machine/i8255a.h"
#include "machine/ins8250.h"
#include "machine/wd17xx.h"

typedef struct
{
	/* general */
	UINT8	svi318;		/* Are we dealing with an SVI-318 or a SVI-328 model. 0 = 328, 1 = 318 */
	/* memory */
	UINT8	*empty_bank;
	UINT8	bank_switch;
	UINT8	bankLow;
	UINT8	bankHigh1;
	UINT8	*bankLow_ptr;
	UINT8	bankLow_read_only;
	UINT8	*bankHigh1_ptr;
	UINT8	bankHigh1_read_only;
	UINT8	*bankHigh2_ptr;
	UINT8	bankHigh2_read_only;
	/* keyboard */
	UINT8	keyboard_row;
	/* SVI-806 80 column card */
	UINT8	svi806_present;
	UINT8	svi806_ram_enabled;
	memory_region	*svi806_ram;
	UINT8	*svi806_gfx;
} SVI_318;

typedef struct
{
	UINT8 driveselect;
	int drq;
	int irq;
	UINT8 heads[2];
} SVI318_FDC_STRUCT;


class svi318_state : public driver_device
{
public:
	svi318_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	SVI_318 m_svi;
	UINT8 *m_pcart;
	UINT32 m_pcart_rom_size;
	SVI318_FDC_STRUCT m_fdc;
};


/*----------- defined in machine/svi318.c -----------*/

extern const i8255a_interface svi318_ppi8255_interface;
extern const ins8250_interface svi318_ins8250_interface[2];
extern const wd17xx_interface svi_wd17xx_interface;

DRIVER_INIT( svi318 );
MACHINE_START( svi318_pal );
MACHINE_START( svi318_ntsc );
MACHINE_RESET( svi318 );

DEVICE_START( svi318_cart );
DEVICE_IMAGE_LOAD( svi318_cart );
DEVICE_IMAGE_UNLOAD( svi318_cart );

INTERRUPT_GEN( svi318_interrupt );
void svi318_vdp_interrupt(running_machine &machine, int i);

WRITE8_HANDLER( svi318_writemem1 );
WRITE8_HANDLER( svi318_writemem2 );
WRITE8_HANDLER( svi318_writemem3 );
WRITE8_HANDLER( svi318_writemem4 );

READ8_HANDLER( svi318_io_ext_r );
WRITE8_HANDLER( svi318_io_ext_w );

READ8_DEVICE_HANDLER( svi318_ppi_r );
WRITE8_DEVICE_HANDLER( svi318_ppi_w );

WRITE8_HANDLER( svi318_psg_port_b_w );
READ8_HANDLER( svi318_psg_port_a_r );

int svi318_cassette_present(running_machine &machine, int id);

MC6845_UPDATE_ROW( svi806_crtc6845_update_row );
VIDEO_START( svi328_806 );
SCREEN_UPDATE( svi328_806 );
MACHINE_RESET( svi328_806 );

#endif /* SVI318_H_ */

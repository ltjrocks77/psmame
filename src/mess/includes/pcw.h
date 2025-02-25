/*****************************************************************************
 *
 * includes/pcw.h
 *
 ****************************************************************************/

#ifndef PCW_H_
#define PCW_H_


#define PCW_BORDER_HEIGHT 8
#define PCW_BORDER_WIDTH 8
#define PCW_NUM_COLOURS 2
#define PCW_DISPLAY_WIDTH 720
#define PCW_DISPLAY_HEIGHT 256

#define PCW_SCREEN_WIDTH	(PCW_DISPLAY_WIDTH + (PCW_BORDER_WIDTH<<1))
#define PCW_SCREEN_HEIGHT	(PCW_DISPLAY_HEIGHT  + (PCW_BORDER_HEIGHT<<1))
#define PCW_PRINTER_WIDTH	(80*16)
#define PCW_PRINTER_HEIGHT	(20*16)


class pcw_state : public driver_device
{
public:
	pcw_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	int m_boot;
	int m_system_status;
	int m_fdc_interrupt_code;
	int m_interrupt_counter;
	UINT8 m_banks[4];
	unsigned char m_bank_force;
	UINT8 m_timer_irq_flag;
	UINT8 m_nmi_flag;
	UINT8 m_printer_command;
	UINT8 m_printer_data;
	UINT8 m_printer_status;
	INT16 m_printer_headpos;
	UINT16 m_kb_scan_row;
	UINT8 m_mcu_keyboard_data[16];
	UINT8 m_mcu_transmit_reset_seq;
	UINT8 m_mcu_transmit_count;
	UINT8 m_mcu_selected;
	UINT8 m_mcu_buffer;
	UINT8 m_mcu_prev;
	unsigned int m_roller_ram_addr;
	unsigned short m_roller_ram_offset;
	unsigned char m_vdu_video_control_register;
	UINT8 m_printer_serial;  // value if shift/store data pin
	UINT8 m_printer_shift;  // state of shift register
	UINT8 m_printer_shift_output;  // output presented to the paper feed motor and print head motor
	UINT8 m_head_motor_state;
	UINT8 m_linefeed_motor_state;
	UINT16 m_printer_pins;
	UINT8 m_printer_p2;  // MCU port P2 state
	UINT32 m_paper_feed;  // amount of paper fed through printer, by n/360 inches.  One line feed is 61/360in (from the linefeed command in CP/M;s ptr menu)
	bitmap_t* m_prn_output;
	UINT8 m_printer_p2_prev;
	emu_timer* m_prn_stepper;
	emu_timer* m_prn_pins;
};


/*----------- defined in video/pcw.c -----------*/

extern VIDEO_START( pcw );
extern SCREEN_UPDATE( pcw );
extern SCREEN_UPDATE( pcw_printer );
extern PALETTE_INIT( pcw );


#endif /* PCW_H_ */

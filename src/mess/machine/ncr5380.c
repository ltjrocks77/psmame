/*
 * ncr5380.c
 *
 * NCR 5380 SCSI controller, as seen in many 680x0 Macs,
 * official Apple add-on cards for the Apple II series,
 * and probably some PC and Amiga cards as well.
 *
 * Emulation by R. Belmont.
 *
 * References:
 * Zilog 5380 manual
 * "Inside Macintosh: Devices" (formerly online at http://www.manolium.org/dev/techsupport/insidemac/Devices/Devices-2.html )
 *
 * NOTES:
 * This implementation is tied closely to the drivers found in the Mac Plus ROM and the routines in Mac
 * System 6 and 7 that it patches out the ROM traps with.  While attempts have been made to
 * have the behavior work according to the manual and not the specific Apple driver code,
 * there are almost certainly areas where that is true.
 *
 */

#include "emu.h"
#include "ncr5380.h"
#include "machine/devhelpr.h"

#define VERBOSE	(0)

static const char *const rnames[] =
{
	"Current data",
	"Initiator cmd",
	"Mode",
	"Target cmd",
	"Bus status",
	"Bus and status",
	"Input data",
	"Reset parity"
};

static const char *const wnames[] =
{
	"Output data",
	"Initiator cmd",
	"Mode",
	"Target cmd",
	"Select enable",
	"Start DMA",
	"DMA target",
	"DMA initiator rec"
};

// get the length of a SCSI command based on it's command byte type
static int get_cmd_len(int cbyte)
{
	int group;

	group = (cbyte>>5) & 7;

	if (group == 0) return 6;
	if (group == 1 || group == 2) return 10;
	if (group == 5) return 12;

	fatalerror("NCR5380: Unknown SCSI command group %d", group);

	return 6;
}

//**************************************************************************
//  DEVICE CONFIGURATION
//**************************************************************************

ncr5380_device_config::ncr5380_device_config(const machine_config &mconfig, const char *tag, const device_config *owner, UINT32 clock)
    : device_config(mconfig, static_alloc_device_config, "5380 SCSI", tag, owner, clock)
{
}

device_config *ncr5380_device_config::static_alloc_device_config(const machine_config &mconfig, const char *tag, const device_config *owner, UINT32 clock)
{
	return global_alloc(ncr5380_device_config(mconfig, tag, owner, clock));
}

device_t *ncr5380_device_config::alloc_device(running_machine &machine) const
{
	return auto_alloc(machine, ncr5380_device(machine, *this));
}

//-------------------------------------------------
//  device_config_complete - perform any
//  operations now that the configuration is
//  complete
//-------------------------------------------------

void ncr5380_device_config::device_config_complete()
{
	// inherit a copy of the static data
	const NCR5380interface *intf = reinterpret_cast<const NCR5380interface *>(static_config());
	if (intf != NULL)
		*static_cast<NCR5380interface *>(this) = *intf;

	// or initialize to defaults if none provided
	else
	{
		scsidevs = NULL;
		irq_callback = NULL;
	}
}

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

const device_type NCR5380 = ncr5380_device_config::static_alloc_device_config;

//-------------------------------------------------
//  ncr5380_device - constructor/destructor
//-------------------------------------------------

ncr5380_device::ncr5380_device(running_machine &_machine, const ncr5380_device_config &config)
    : device_t(_machine, config),
      m_config(config)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void ncr5380_device::device_start()
{
	int i;

	memset(m_5380_Registers, 0, sizeof(m_5380_Registers));
	memset(m_5380_Data, 0, sizeof(m_5380_Data));
	memset(m_scsi_devices, 0, sizeof(m_scsi_devices));

	m_next_req_flag = 0;

	// try to open the devices
	for (i = 0; i < m_config.scsidevs->devs_present; i++)
	{
		SCSIAllocInstance( m_machine,
				m_config.scsidevs->devices[i].scsiClass,
				&m_scsi_devices[m_config.scsidevs->devices[i].scsiID],
				m_config.scsidevs->devices[i].diskregion );
	}

	save_item(NAME(m_5380_Registers));
	save_item(NAME(m_5380_Command));
	save_item(NAME(m_5380_Data));
	save_item(NAME(m_last_id));
	save_item(NAME(m_cmd_ptr));
	save_item(NAME(m_d_ptr));
	save_item(NAME(m_d_limit));
	save_item(NAME(m_next_req_flag));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void ncr5380_device::device_reset()
{
	memset(m_5380_Registers, 0, sizeof(m_5380_Registers));
	memset(m_5380_Data, 0, sizeof(m_5380_Data));
	memset(m_scsi_devices, 0, sizeof(m_scsi_devices));

	m_next_req_flag = 0;
	m_cmd_ptr = 0;
	m_d_ptr = 0;
	m_d_limit = 0;
	m_last_id = 0;

	ncr5380_scan_devices();
}

//-------------------------------------------------
//  device_stop - device-specific stop/shutdown
//-------------------------------------------------
void ncr5380_device::device_stop()
{
	int i;

	// clean up the devices
	for (i = 0; i < m_config.scsidevs->devs_present; i++)
	{
		SCSIDeleteInstance( m_scsi_devices[m_config.scsidevs->devices[i].scsiID] );
	}
}

//-------------------------------------------------
//  Public API
//-------------------------------------------------
READ8_DEVICE_HANDLER_TRAMPOLINE(ncr5380, ncr5380_read_reg)
{
	int reg = offset & 7;
	UINT8 rv = 0;

	switch( reg )
	{
		case R5380_CURDATA:
		case R5380_INPUTDATA:
			rv = m_5380_Registers[reg];

			// if we're in the data transfer phase or DMA, readback device data instead
			if (((m_5380_Registers[R5380_BUSSTATUS] & 0x1c) == 0x04) || (m_5380_Registers[R5380_BUSSTATUS] & 0x40))
			{
				rv = m_5380_Data[m_d_ptr];

				// if the limit's less than 512, only read "limit" bytes
				if (m_d_limit < 512)
				{
					if (m_d_ptr < (m_d_limit-1))
					{
						m_d_ptr++;
					}
					else
					{
						m_next_req_flag = 1;
					}
				}
				else
				{
					if (m_d_ptr < 511)
					{
						m_d_ptr++;
					}
					else
					{
						m_d_limit -= 512;
						m_d_ptr = 0;

						m_next_req_flag = 1;

						// don't issue a "false" read
						if (m_d_limit > 0)
						{
							ncr5380_read_data((m_d_limit < 512) ? m_d_limit : 512, m_5380_Data);
						}
						else
						{
							// if this is DMA, signal DMA end
							if (m_5380_Registers[R5380_BUSSTATUS] & 0x40)
							{
								m_5380_Registers[R5380_BUSSTATUS] |= 0x80;
							}

							// drop /REQ
							m_5380_Registers[R5380_BUSSTATUS] &= ~0x20;

							// clear phase match
							m_5380_Registers[R5380_BUSANDSTAT] &= ~0x08;
						}
					}
				}

			}
			break;

		default:
			rv = m_5380_Registers[reg];

			// temporarily drop /REQ
			if ((reg == R5380_BUSSTATUS) && (m_next_req_flag))
			{
				rv &= ~0x20;
				m_next_req_flag = 0;
			}
			break;
	}

	if (VERBOSE)
		logerror("NCR5380: read %s (reg %d) = %02x [PC=%x]\n", rnames[reg], reg, rv, cpu_get_pc(m_machine.firstcpu));

	return rv;
}

WRITE8_DEVICE_HANDLER_TRAMPOLINE(ncr5380, ncr5380_write_reg)
{
	int reg = offset & 7;

	if (VERBOSE)
		logerror("NCR5380: %02x to %s (reg %d) [PC=%x]\n", data, wnames[reg], reg, cpu_get_pc(m_machine.firstcpu));

	switch( reg )
	{
		case R5380_OUTDATA:
			// if we're in the command phase, collect the command bytes
			if ((m_5380_Registers[R5380_BUSSTATUS] & 0x1c) == 0x08)
			{
				m_5380_Command[m_cmd_ptr++] = data;
			}

			// if we're in the select phase, this is the target id
			if (m_5380_Registers[R5380_INICOMMAND] == 0x04)
			{
				data &= 0x7f;	// clear the high bit
				if (data == 0x40)
				{
					m_last_id = 6;
				}
				else if (data == 0x20)
				{
					m_last_id = 5;
				}
				else if (data == 0x10)
				{
					m_last_id = 4;
				}
				else if (data == 0x08)
				{
					m_last_id = 3;
				}
				else if (data == 0x04)
				{
					m_last_id = 2;
				}
				else if (data == 0x02)
				{
					m_last_id = 1;
				}
				else if (data == 0x01)
				{
					m_last_id = 0;
				}
			}

			// if this is a write, accumulate accordingly
			if (((m_5380_Registers[R5380_BUSSTATUS] & 0x1c) == 0x00) && (m_5380_Registers[R5380_INICOMMAND] == 1))
			{
				m_5380_Data[m_d_ptr] = data;

				// if we've hit a sector, flush
				if (m_d_ptr == 511)
				{
					ncr5380_write_data(512, &m_5380_Data[0]);

					m_d_limit -= 512;
					m_d_ptr = 0;

					// no more data?  set DMA END flag
					if (m_d_limit <= 0)
					{
						m_5380_Registers[R5380_BUSANDSTAT] = 0xc8;
					}
				}
				else
				{
					m_d_ptr++;
				}

				// make sure we don't upset the status readback
				data = 0;
			}
			break;

		case R5380_INICOMMAND:
			if (data == 0)	// dropping the bus
			{
				// make sure it's not busy
				m_5380_Registers[R5380_BUSSTATUS] &= ~0x40;

				// are we in the command phase?
				if ((m_5380_Registers[R5380_BUSSTATUS] & 0x1c) == 0x08)
				{
					// is the current command complete?
					if (get_cmd_len(m_5380_Command[0]) == m_cmd_ptr)
					{
						if (VERBOSE)
							logerror("NCR5380: Command (to ID %d): %x %x %x %x %x %x %x %x %x %x (PC %x)\n", m_last_id, m_5380_Command[0], m_5380_Command[1], m_5380_Command[2], m_5380_Command[3], m_5380_Command[4], m_5380_Command[5], m_5380_Command[6], m_5380_Command[7], m_5380_Command[8], m_5380_Command[9], cpu_get_pc(m_machine.firstcpu));

						SCSISetCommand(m_scsi_devices[m_last_id], &m_5380_Command[0], 16);
						SCSIExecCommand(m_scsi_devices[m_last_id], &m_d_limit);

						if (VERBOSE)
							logerror("NCR5380: Command returned %d bytes\n",  m_d_limit);

						m_d_ptr = 0;

						// is data available?
						if (m_d_limit > 0)
						{
							// make sure for transfers under 512 bytes that we always pad with a zero
							if (m_d_limit < 512)
							{
								m_5380_Data[m_d_limit] = 0;
							}

							// read back the amount available, or 512 bytes, whichever is smaller
							ncr5380_read_data((m_d_limit < 512) ? m_d_limit : 512, m_5380_Data);

							// raise REQ to indicate data is available
							m_5380_Registers[R5380_BUSSTATUS] |= 0x20;
						}
					}
				}

			}

			if (data == 5)	// want the bus?
			{
				// if the device exists, make the bus busy.
				// otherwise don't.

				if (m_scsi_devices[m_last_id])
				{
					if (VERBOSE)
						logerror("NCR5380: Giving the bus for ID %d\n", m_last_id);
					m_5380_Registers[R5380_BUSSTATUS] |= 0x40;
				}
				else
				{
					if (VERBOSE)
						logerror("NCR5380: Rejecting the bus for ID %d\n", m_last_id);
					m_5380_Registers[R5380_BUSSTATUS] &= ~0x40;
				}
			}

			if (data == 1)	// data bus (prelude to command?)
			{
				// raise REQ
				m_5380_Registers[R5380_BUSSTATUS] |= 0x20;
			}

			if (data & 0x10)	// ACK drops REQ
			{
				// drop REQ
				m_5380_Registers[R5380_BUSSTATUS] &= ~0x20;
			}
			break;

		case R5380_MODE:
			if (data == 2)	// DMA
			{
				// put us in DMA mode
				m_5380_Registers[R5380_BUSANDSTAT] |= 0x40;
			}

			if (data == 1)	// arbitrate?
			{
				m_5380_Registers[R5380_INICOMMAND] |= 0x40;	// set arbitration in progress
				m_5380_Registers[R5380_INICOMMAND] &= ~0x20;	// clear "lost arbitration"
			}

			if (data == 0)
			{
				// drop DMA mode
				m_5380_Registers[R5380_BUSANDSTAT] &= ~0x40;
			}
			break;

		case R5380_TARGETCMD:
			// sync the bus phase with what was just written
			m_5380_Registers[R5380_BUSSTATUS] &= ~0x1c;
			m_5380_Registers[R5380_BUSSTATUS] |= (data & 7)<<2;

			// and set the "phase match" flag
			m_5380_Registers[R5380_BUSANDSTAT] |= 0x08;

			// and set /REQ
			m_5380_Registers[R5380_BUSSTATUS] |= 0x20;

			// if we're entering the command phase, start accumulating the data
			if ((m_5380_Registers[R5380_BUSSTATUS] & 0x1c) == 0x08)
			{
				m_cmd_ptr = 0;
			}
			break;

		default:
			break;
	}

	m_5380_Registers[reg] = data;

	// note: busandstat overlaps startdma, so we need to do this here!
	if (reg == R5380_STARTDMA)
	{
		m_5380_Registers[R5380_BUSANDSTAT] = 0x48;
	}
}


void ncr5380_device::ncr5380_read_data(int bytes, UINT8 *pData)
{
	if (m_scsi_devices[m_last_id])
	{
		if (VERBOSE)
			logerror("NCR5380: issuing read for %d bytes\n", bytes);
		SCSIReadData(m_scsi_devices[m_last_id], pData, bytes);
	}
	else
	{
		logerror("ncr5380: read unknown device SCSI ID %d\n", m_last_id);
	}
}


void ncr5380_device::ncr5380_write_data(int bytes, UINT8 *pData)
{
	if (m_scsi_devices[m_last_id])
	{
		SCSIWriteData(m_scsi_devices[m_last_id], pData, bytes);
	}
	else
	{
		logerror("ncr5380: write to unknown device SCSI ID %d\n", m_last_id);
	}
}

void ncr5380_device::ncr5380_scan_devices()
{
	int i;

	// try to open the devices
	for (i = 0; i < m_config.scsidevs->devs_present; i++)
	{
		// if a device wasn't already allocated
		if (!m_scsi_devices[m_config.scsidevs->devices[i].scsiID])
		{
			SCSIAllocInstance( m_machine,
					m_config.scsidevs->devices[i].scsiClass,
					&m_scsi_devices[m_config.scsidevs->devices[i].scsiID],
					m_config.scsidevs->devices[i].diskregion );
		}
	}
}

void ncr5380_read_data(device_t *dev, UINT8 bytes, UINT8 *pData)
{
	return downcast<ncr5380_device*>(dev)->ncr5380_read_data(bytes, pData);
}

void ncr5380_write_data(device_t *dev, UINT8 bytes, UINT8 *pData)
{
	downcast<ncr5380_device*>(dev)->ncr5380_write_data(bytes, pData);
}

void ncr5380_scan_devices(device_t *dev)
{
	downcast<ncr5380_device*>(dev)->ncr5380_scan_devices();
}


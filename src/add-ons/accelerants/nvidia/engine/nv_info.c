/* Read initialisation information from card */
/* some bits are hacks, where PINS is not known */
/* Author:
   Rudolf Cornelissen 7/2003-8/2004
*/

#define MODULE_BIT 0x00002000

#include "nv_std.h"

static void detect_panels(void);
static void setup_output_matrix(void);
static void pinsnv4_fake(void);
static void pinsnv5_nv5m64_fake(void);
static void pinsnv6_fake(void);
static void pinsnv10_arch_fake(void);
static void pinsnv20_arch_fake(void);
static void pinsnv30_arch_fake(void);
static void getstrap_arch_nv4(void);
static void getstrap_arch_nv10_20_30(void);
static status_t pins2_read(uint8 *rom, uint32 offset);
static status_t pins3_6_read(uint8 *rom, uint32 offset);
static status_t coldstart_card(uint8* rom, uint16 init1, uint16 init2, uint16 init_size);
static status_t exec_type1_script(uint8* rom, uint16 adress, int16* size);
static void log_pll(uint32 reg);
static void	setup_ram_config(uint8* rom);

/* Parse the BIOS PINS structure if there */
status_t parse_pins ()
{
	uint8 *rom;
	uint8 chksum = 0;
	int i;
	uint32 offset;
	status_t result = B_ERROR;

	/* preset PINS read status to failed */
	si->ps.pins_status = B_ERROR;

	/* check the validity of PINS */
	LOG(2,("INFO: Reading PINS info\n"));
	rom = (uint8 *) si->rom_mirror;
	/* check BIOS signature - this is defined in the PCI standard */
	if (rom[0]!=0x55 || rom[1]!=0xaa)
	{
		LOG(8,("INFO: BIOS signature not found\n"));
		return B_ERROR;
	}
	LOG(2,("INFO: BIOS signature $AA55 found OK\n"));

	/* find the PINS struct adress */
	for (offset = 0; offset < 65536; offset++)
	{
		if (rom[offset    ] != 0xff) continue;
		if (rom[offset + 1] != 0x7f) continue;
		if (rom[offset + 2] != 0x4e) continue; /* N */
		if (rom[offset + 3] != 0x56) continue; /* V */
		if (rom[offset + 4] != 0x00) continue;

		LOG(8,("INFO: PINS signature found\n"));
		break;
	}

	if (offset > 65535)
	{
		LOG(8,("INFO: PINS signature not found\n"));
		return B_ERROR;
	}

	/* verify PINS checksum */
	for (i = 0; i < 8; i++)
	{
		chksum += rom[offset + i];
	}
	if (chksum)
	{
		LOG(8,("INFO: PINS checksum error\n"));
		return B_ERROR;
	}

	/* checkout PINS struct version */
	LOG(2,("INFO: PINS checksum is OK; PINS version is %d.%d\n",
		rom[offset + 5], rom[offset + 6]));

	/* fill out the si->ps struct as far as is possible */
	switch (rom[offset + 5])
	{
	case 2:
		result = pins2_read(rom, offset);
		break;
	case 3:
	case 4:
	case 5:
	case 6:
		result = pins3_6_read(rom, offset);
		break;
	default:
		LOG(8,("INFO: unknown PINS version\n"));
		return B_ERROR;
		break;
	}

	/* check PINS read result */
	if (result == B_ERROR)
	{
		LOG(8,("INFO: PINS read/decode/execute error\n"));
		return B_ERROR;
	}
	/* PINS scan succeeded */
	si->ps.pins_status = B_OK;
	LOG(2,("INFO: PINS scan completed succesfully\n"));
	return B_OK;
}

static status_t pins2_read(uint8 *rom, uint32 offset)
{
	uint16 init1 = rom[offset + 18] + (rom[offset + 19] * 256);
	uint16 init2 = rom[offset + 20] + (rom[offset + 21] * 256);
	uint16 init_size = rom[offset + 22] + (rom[offset + 23] * 256) + 1;
	char* signon_msg   = &(rom[(rom[offset + 24] + (rom[offset + 25] * 256))]);
	char* vendor_name  = &(rom[(rom[offset + 40] + (rom[offset + 41] * 256))]);
	char* product_name = &(rom[(rom[offset + 42] + (rom[offset + 43] * 256))]);
	char* product_rev  = &(rom[(rom[offset + 44] + (rom[offset + 45] * 256))]);

	LOG(8,("INFO: init_1 $%04x, init_2 $%04x, size $%04x\n", init1, init2, init_size));
	LOG(8,("INFO: signon msg:\n%s\n", signon_msg));
	LOG(8,("INFO: vendor name: %s\n", vendor_name));
	LOG(8,("INFO: product name: %s\n", product_name));
	LOG(8,("INFO: product rev: %s\n", product_rev));

	return coldstart_card(rom, init1, init2, init_size);
}

static status_t pins3_6_read(uint8 *rom, uint32 offset)
{
	uint16 init1 = rom[offset + 18] + (rom[offset + 19] * 256);
	uint16 init2 = rom[offset + 20] + (rom[offset + 21] * 256);
	uint16 init_size = rom[offset + 22] + (rom[offset + 23] * 256) + 1;
	char* signon_msg   = &(rom[(rom[offset + 30] + (rom[offset + 31] * 256))]);
	char* vendor_name  = &(rom[(rom[offset + 46] + (rom[offset + 47] * 256))]);
	char* product_name = &(rom[(rom[offset + 48] + (rom[offset + 49] * 256))]);
	char* product_rev  = &(rom[(rom[offset + 50] + (rom[offset + 51] * 256))]);

	LOG(8,("INFO: init_1 $%04x, init_2 $%04x, size $%04x\n", init1, init2, init_size));
	LOG(8,("INFO: signon msg:\n%s\n", signon_msg));
	LOG(8,("INFO: vendor name: %s\n", vendor_name));
	LOG(8,("INFO: product name: %s\n", product_name));
	LOG(8,("INFO: product rev: %s\n", product_rev));

	/* pins 5.16 and higher only contain PLL VCO range info */
	if (((rom[offset + 5]) >= 5) && ((rom[offset + 6]) >= 0x10))
	{
		uint32 fvco_max = *((uint32*)(&(rom[offset + 67])));
		uint32 fvco_min = *((uint32*)(&(rom[offset + 71])));

		LOG(8,("INFO: PLL VCO range is %dkHz - %dkHz\n", fvco_min, fvco_max));

		/* only pins 5 contains this info */
		//fixme: not finished..
/*		if ((rom[offset + 5]) == 5)
		{
			uint16 IOFlagConditionTablePointer =
				rom[offset + 85] + (rom[offset + 86] * 256);
		}
*/	}

	return coldstart_card(rom, init1, init2, init_size);
}

static status_t coldstart_card(uint8* rom, uint16 init1, uint16 init2, uint16 init_size)
{
	status_t result = B_OK;
	int16 size = init_size;

	LOG(8,("INFO: executing coldstart...\n"));

	/* select colormode CRTC registers base adresses */
	NV_REG8(NV8_MISCW) = 0xcb;

	/* unknown.. */
	NV_REG8(NV8_VSE2) = 0x01;

	/* enable access to primary head */
	set_crtc_owner(0);
	/* unlock head's registers for R/W access */
	CRTCW(LOCK, 0x57);
	CRTCW(VSYNCE ,(CRTCR(VSYNCE) & 0x7f));
	/* disable RMA as it's not used */
	/* (RMA is the cmd register for the 32bit port in the GPU to access 32bit registers
	 *  and framebuffer via legacy ISA I/O space.) */
	CRTCW(RMA, 0x00);

	if (si->ps.secondary_head)
	{
		/* enable access to secondary head */
		set_crtc_owner(1);
		/* unlock head's registers for R/W access */
		CRTC2W(LOCK, 0x57);
		CRTC2W(VSYNCE ,(CRTCR(VSYNCE) & 0x7f));
	}

	/* turn off both displays and the hardcursors (also disables transfers) */
	nv_crtc_dpms(false, false, false);
	nv_crtc_cursor_hide();
	if (si->ps.secondary_head)
	{
		nv_crtc2_dpms(false, false, false);
		nv_crtc2_cursor_hide();
	}

	/* execute BIOS coldstart script(s) */
	if (init1 || init2)
	{
		if (init1)
			if (exec_type1_script(rom, init1, &size) != B_OK) result = B_ERROR;
		if (init2 && (result == B_OK))
			if (exec_type1_script(rom, init2, &size) != B_OK) result = B_ERROR;

		/* now enable ROM shadow or the card will remain shut-off! */
		NV_REG32(0x00001800 + NVCFG_ROMSHADOW) |= 0x00000001;
	}
	else
	{
		result = B_ERROR;
	}

	if (result != B_OK)
		LOG(8,("INFO: coldstart failed.\n"));
	else
		LOG(8,("INFO: coldstart execution completed OK.\n"));

	return result;
}

static status_t exec_type1_script(uint8* rom, uint16 adress, int16* size)
{
	status_t result = B_OK;
	bool end = false;
	bool exec = true;
	uint8 index, byte, safe;
	uint32 reg, data, data2, and_out, or_in, safe32;

	LOG(8,("\nINFO: executing type1 script at adress $%04x...\n", adress));

	while (!end)
	{
		LOG(8,("INFO: $%04x ($%02x); ", adress, rom[adress]));

		//fixme: complete...
		switch (rom[adress])
		{
		case 0x59:
			*size -= 7;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint32*)(&(rom[adress])));
			adress += 4;
			data = *((uint16*)(&(rom[adress])));
			adress += 2;
			data2 = *((uint16*)(&(rom[data])));
			LOG(8,("cmd 'calculate indirect and set PLL 32bit REG $%08x for %.3fMHz'\n",
				reg, ((float)data2)));
			if (exec)
			{
				//fixme: setup core and RAM PLL calc routine(s), now (mis)using DAC's...
				display_mode target;
				float calced_clk;
				uint8 m, n, p;
				target.timing.pixel_clock = (data2 * 1000);
				nv_dac_pix_pll_find(target, &calced_clk, &m, &n, &p, 0);
				NV_REG32(reg) = ((p << 16) | (n << 8) | m);
//fixme?
				/* program 2nd set N and M scalers if they exist (b31=1 enables them) */
//				if ((si->ps.card_type == NV31) || (si->ps.card_type == NV36))
//					DACW(PIXPLLC2, 0x80000401);
			}
			log_pll(reg);
			break;
		case 0x5a:
			*size -= 7;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint32*)(&(rom[adress])));
			adress += 4;
			data = *((uint16*)(&(rom[adress])));
			adress += 2;
			data2 = *((uint32*)(&(rom[data])));
			LOG(8,("cmd 'WR indirect 32bit REG' $%08x = $%08x\n", reg, data2));
			if (exec) NV_REG32(reg) = data2;
			break;
		case 0x63:
			*size -= 1;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			LOG(8,("cmd 'setup RAM config'\n"));
			//fixme: setup_ram_config is also used on NV11 (at least), which means the
			//       routine should function differently there!
			if (exec) setup_ram_config(rom);
			break;
		case 0x65:
			*size -= 13;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint32*)(&(rom[adress])));
			adress += 4;
			data = *((uint32*)(&(rom[adress])));
			adress += 4;
			data2 = *((uint32*)(&(rom[adress])));
			adress += 4;
			LOG(8,("cmd 'WR 32bit REG $%08x with $%08x, then with $%08x'\n", reg, data, data2));
			if (exec)
			{
				/* alternate NV-specific access to PCI config space registers: */
				safe32 = NV_REG32(0x00001800 + NVCFG_AGPCMD);
				NV_REG32(0x00001800 + NVCFG_AGPCMD) = 0x00000000;
				NV_REG32(reg) = data;
				NV_REG32(reg) = data2;
				NV_REG32(0x00001800 + NVCFG_AGPCMD) = safe32;
				NV_REG32(0x00001800 + NVCFG_ROMSHADOW) &= 0xfffffffe;
			}
			break;
		case 0x66: /* new on NV11 */
			LOG(8,("NV11+ xxx cmd, skipping...\n"));

			*size -= 1;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			adress += 1;
			break;
		case 0x67: /* new on NV11 */
			LOG(8,("NV11+ xxx cmd, skipping...\n"));

			*size -= 1;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			adress += 1;
			break;
		case 0x68: /* new on NV11 */
			LOG(8,("NV11+ xxx cmd, skipping...\n"));

			*size -= 1;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			adress += 1;
			break;
		case 0x69:
			*size -= 5;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint16*)(&(rom[adress])));
			adress += 2;
			and_out = *((uint8*)(&(rom[adress])));
			adress += 1;
			or_in = *((uint8*)(&(rom[adress])));
			adress += 1;
			LOG(8,("cmd 'RD 8bit ISA I/O REG $%04x, AND-out = $%02x, OR-in = $%02x, WR-bk'\n",
				reg, and_out, or_in));
			//fixme? this is for ISA I/O registers. Looks like they are in mapped range
			//       as well (confirm or update code!)
			if (exec)
			{
				byte = NV_REG8(/*0x00601000 + */reg);
				byte &= (uint8)and_out;
				byte |= (uint8)or_in;
				NV_REG8(/*0x00601000 + */reg) = byte;
			}
			break;
		case 0x6e:
			*size -= 13;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint32*)(&(rom[adress])));
			adress += 4;
			and_out = *((uint32*)(&(rom[adress])));
			adress += 4;
			or_in = *((uint32*)(&(rom[adress])));
			adress += 4;
			LOG(8,("cmd 'RD 32bit REG $%08x, AND-out = $%08x, OR-in = $%08x, WR-bk'\n",
				reg, and_out, or_in));
			if (exec)
			{
				data = NV_REG32(reg);
				data &= and_out;
				data |= or_in;
				NV_REG32(reg) = data;
			}
			break;
		case 0x71:
			LOG(8,("cmd 'END', execution completed.\n\n"));
			end = true;

			*size -= 1;
			if (*size < 0)
			{
				LOG(8,("script size error!\n\n"));
				result = B_ERROR;
			}
			break;
		case 0x74:
			*size -= 3;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			data = *((uint16*)(&(rom[adress])));
			adress += 2;
			LOG(8,("cmd 'SNOOZE for %d ($%04x) microSeconds'\n", data, data));
			if (exec) snooze(data);
			break;
		case 0x77:
			*size -= 7;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint32*)(&(rom[adress])));
			adress += 4;
			data = *((uint16*)(&(rom[adress])));
			adress += 2;
			LOG(8,("cmd 'WR 32bit REG' $%08x = $%08x (b31-16 = '0', b15-0 = data)\n",
				reg, data));
			if (exec) NV_REG32(reg) = data;
			break;
		case 0x78:
			*size -= 6;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint16*)(&(rom[adress])));
			adress += 2;
			index = *((uint8*)(&(rom[adress])));
			adress += 1;
			and_out = *((uint8*)(&(rom[adress])));
			adress += 1;
			or_in = *((uint8*)(&(rom[adress])));
			adress += 1;
			LOG(8,("cmd 'RD 8bit indexed ISA I/O REG $%02x via $%04x, AND-out = $%02x, OR-in = $%02x, WR-bk'\n",
				index, reg, and_out, or_in));
			//fixme? this is for ISA I/O registers. Looks like they are in mapped range
			//       as well (confirm or update code!)
			if (exec)
			{
				safe = NV_REG8(/*0x00601000 + */reg);
				NV_REG8(/*0x00601000 + */reg) = index;
				byte = NV_REG8(/*0x00601000 + */reg + 1);
				byte &= (uint8)and_out;
				byte |= (uint8)or_in;
				NV_REG8(/*0x00601000 + */reg + 1) = byte;
				NV_REG8(/*0x00601000 + */reg) = safe;
			}
			break;
		case 0x79:
			*size -= 7;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint32*)(&(rom[adress])));
			adress += 4;
			data = *((uint16*)(&(rom[adress])));
			adress += 2;
			LOG(8,("cmd 'calculate and set PLL 32bit REG $%08x for %.3fMHz'\n", reg, (data / 100.0)));
			if (exec)
			{
				//fixme: setup core and RAM PLL calc routine(s), now (mis)using DAC's...
				display_mode target;
				float calced_clk;
				uint8 m, n, p;
				target.timing.pixel_clock = (data * 10);
				nv_dac_pix_pll_find(target, &calced_clk, &m, &n, &p, 0);
				NV_REG32(reg) = ((p << 16) | (n << 8) | m);
//fixme?
				/* program 2nd set N and M scalers if they exist (b31=1 enables them) */
//				if ((si->ps.card_type == NV31) || (si->ps.card_type == NV36))
//					DACW(PIXPLLC2, 0x80000401);
			}
			log_pll(reg);
			break;
		case 0x7a:
			*size -= 9;
			if (*size < 0)
			{
				LOG(8,("script size error, aborting!\n\n"));
				end = true;
				result = B_ERROR;
				break;
			}

			/* execute */
			adress += 1;
			reg = *((uint32*)(&(rom[adress])));
			adress += 4;
			data = *((uint32*)(&(rom[adress])));
			adress += 4;
			LOG(8,("cmd 'WR 32bit REG' $%08x = $%08x\n", reg, data));
			if (exec) NV_REG32(reg) = data;
			break;
		default:
			LOG(8,("unknown cmd, aborting!\n\n"));
			end = true;
			result = B_ERROR;
			break;
		}
	}

	return result;
}

static void log_pll(uint32 reg)
{
	if ((si->ps.card_type == NV31) || (si->ps.card_type == NV36))
		LOG(8,("INFO: ---WARNING: check/update PLL programming script code!!!\n"));
	switch (reg)
	{
	case NV32_MEMPLL:
		LOG(8,("INFO: ---Memory PLL accessed.\n"));
		break;
	case NV32_COREPLL:
		LOG(8,("INFO: ---Core PLL accessed.\n"));
		break;
	case NVDAC_PIXPLLC:
		LOG(8,("INFO: ---DAC1 PLL accessed.\n"));
		break;
	case NVDAC2_PIXPLLC:
		LOG(8,("INFO: ---DAC2 PLL accessed.\n"));
		break;
	/* unexpected cases, here for learning goals... */
	case NV32_MEMPLL2:
		LOG(8,("INFO: ---NV31/NV36 extension to memory PLL accessed only!\n"));
		break;
	case NV32_COREPLL2:
		LOG(8,("INFO: ---NV31/NV36 extension to core PLL accessed only!\n"));
		break;
	case NVDAC_PIXPLLC2:
		LOG(8,("INFO: ---NV31/NV36 extension to DAC1 PLL accessed only!\n"));
		break;
	case NVDAC2_PIXPLLC2:
		LOG(8,("INFO: ---NV31/NV36 extension to DAC2 PLL accessed only!\n"));
		break;
	default:
		LOG(8,("INFO: ---Unknown PLL accessed!\n"));
		break;
	}
}

static void	setup_ram_config(uint8* rom)
{
	uint32 ram_cfg, data;

	/* set MRS = 256 */
	NV_REG32(NV32_PFB_DEBUG_0) &= 0xffffffef;
	/* read RAM config hardware(?) strap */
	ram_cfg = ((NV_REG32(NV32_NVSTRAPINFO2) >> 2) & 0x0000000f);
	LOG(8,("INFO: ---RAM config strap is $%01x\n", ram_cfg));
	/* use it as a pointer in a BIOS table for prerecorded RAM configurations */
	//fixme?: is 0x01a8 indeed fixed or should this adress be gained via pins somehow?
	ram_cfg = *((uint16*)(&(rom[(0x01a8 + (ram_cfg * 2))])));
	/* log info */
	switch (ram_cfg & 0x00000003)
	{
	case 0:
		LOG(8,("INFO: ---32Mb RAM should be connected\n"));
		break;
	case 1:
		LOG(8,("INFO: ---4Mb RAM should be connected\n"));
		break;
	case 2:
		LOG(8,("INFO: ---8Mb RAM should be connected\n"));
		break;
	case 3:
		LOG(8,("INFO: ---16Mb RAM should be connected\n"));
		break;
	}
	if (ram_cfg & 0x00000004)
		LOG(8,("INFO: ---RAM should be 128bits wide\n"));
	else
		LOG(8,("INFO: ---RAM should be 64bits wide\n"));
	switch ((ram_cfg & 0x00000038) >> 3)
	{
	case 0:
		LOG(8,("INFO: ---RAM type: 8Mbit SGRAM\n"));
		break;
	case 1:
		LOG(8,("INFO: ---RAM type: 16Mbit SGRAM\n"));
		break;
	case 2:
		LOG(8,("INFO: ---RAM type: 4 banks of 16Mbit SGRAM\n"));
		break;
	case 3:
		LOG(8,("INFO: ---RAM type: 16Mbit SDRAM\n"));
		break;
	case 4:
		LOG(8,("INFO: ---RAM type: 64Mbit SDRAM\n"));
		break;
	case 5:
		LOG(8,("INFO: ---RAM type: 64Mbit x16 SDRAM\n"));
		break;
	}
	/* set RAM amount, width and type */
	data = (NV_REG32(NV32_NV4STRAPINFO) & 0xffffffc0);
	NV_REG32(NV32_NV4STRAPINFO) = (data | (ram_cfg & 0x0000003f));
	/* setup write to read delay (?) */
	data = (NV_REG32(NV32_PFB_CONFIG_1) & 0xff8ffffe);
	data |= ((ram_cfg & 0x00000700) << 12);
	/* force update via b0 = 0... */
	NV_REG32(NV32_PFB_CONFIG_1) = data;
	/* ... followed by b0 = 1(?) */
	NV_REG32(NV32_PFB_CONFIG_1) = (data | 0x00000001);
	//fixme?: do RAM width test
	//fixme?: do RAM size test
}

/* fake_pins presumes the card was coldstarted by it's BIOS */
void fake_pins(void)
{
	LOG(8,("INFO: faking PINS\n"));

	/* set failsave speeds */
	switch (si->ps.card_type)
	{
	case NV04:
		pinsnv4_fake();
		break;
	case NV05:
	case NV05M64:
		pinsnv5_nv5m64_fake();
		break;
	case NV06:
		pinsnv6_fake();
		break;
	default:
		switch (si->ps.card_arch)
		{
		case NV10A:
			pinsnv10_arch_fake();
			break;
		case NV20A:
			pinsnv20_arch_fake();
			break;
		case NV30A:
			pinsnv30_arch_fake();
			break;
		default:
			/* 'failsafe' values... */
			pinsnv10_arch_fake();
			break;
		}
		break;
	}

	/* detect RAM amount, reference crystal frequency and dualhead */
	switch (si->ps.card_arch)
	{
	case NV04A:
		getstrap_arch_nv4();
		break;
	default:
		getstrap_arch_nv10_20_30();
		break;
	}

	/* override memory detection if requested by user */
	if (si->settings.memory != 0)
	{
		LOG(2,("INFO: forcing memory size (specified in settings file)\n"));
		si->ps.memory_size = si->settings.memory * 1024 * 1024;
	}

	/* find out if the card has a tvout chip */
	si->ps.tvout = false;
	si->ps.tvout_chip_type = NONE;
//fixme ;-)
/*	if (i2c_maven_probe() == B_OK)
	{
		si->ps.tvout = true;
		si->ps.tvout_chip_bus = ???;
		si->ps.tvout_chip_type = ???;
	}
*/

	/* find out the BIOS preprogrammed panel use status... */
	detect_panels();

	/* determine and setup output devices and heads */
	setup_output_matrix();

	/* select other CRTC for primary head use if specified by user in settings file */
	if (si->ps.secondary_head && si->settings.switchhead)
	{
		LOG(2,("INFO: inverting head use (specified in settings file)\n"));
		si->ps.crtc2_prim = !si->ps.crtc2_prim;
	}
}

static void detect_panels()
{
	/* detect if the BIOS enabled LCD's (internal panels or DVI) or TVout */

	/* both external TMDS transmitters (used for LCD/DVI) and external TVencoders
	 * (can) use the CRTC's in slaved mode. */
	/* Note:
	 * DFP's are programmed with standard VESA modelines by the card's BIOS! */
	bool slaved_for_dev1 = false, slaved_for_dev2 = false;
	bool tvout1 = false, tvout2 = false;

	/* check primary head: */
	/* enable access to primary head */
	set_crtc_owner(0);

	/* unlock head's registers for R/W access */
	CRTCW(LOCK, 0x57);
	CRTCW(VSYNCE ,(CRTCR(VSYNCE) & 0x7f));

	LOG(2,("INFO: Dumping flatpanel related CRTC registers:\n"));
	/* related info PIXEL register:
	 * b7: 1 = slaved mode										(all cards). */
	LOG(2,("CRTC1: PIXEL register: $%02x\n", CRTCR(PIXEL)));
	/* info LCD register:
	 * b7: 1 = stereo view (shutter glasses use)				(all cards),
	 * b5: 1 = power ext. TMDS (or something)/0 = TVout	use	(?)	(confirmed NV17, NV28),
	 * b4: 1 = power ext. TMDS (or something)/0 = TVout use	(?)	(confirmed NV34),
	 * b3: 1 = ??? (not panel related probably!)				(confirmed NV34),
	 * b1: 1 = power ext. TMDS (or something) (?)				(confirmed NV05?, NV17),
	 * b0: 1 = select panel encoder / 0 = select TVout encoder	(all cards). */
	LOG(2,("CRTC1: LCD register: $%02x\n", CRTCR(LCD)));
	/* info 0x59 register:
	 * b0: 1 = enable ext. TMDS clock (DPMS)					(confirmed NV28, NV34). */
	LOG(2,("CRTC1: register $59: $%02x\n", CRTCR(0x59)));
	/* info 0x9f register:
	 * b4: 0 = TVout use (?). */
	LOG(2,("CRTC1: register $9f: $%02x\n", CRTCR(0x9f)));

	/* detect active slave device (if any) */
	slaved_for_dev1 = (CRTCR(PIXEL) & 0x80);
	if (slaved_for_dev1)
	{
		/* if the panel isn't selected, tvout is.. */
		tvout1 = !(CRTCR(LCD) & 0x01);
	}

	if (si->ps.secondary_head)
	{
		/* check secondary head: */
		/* enable access to secondary head */
		set_crtc_owner(1);
		/* unlock head's registers for R/W access */
		CRTC2W(LOCK, 0x57);
		CRTC2W(VSYNCE ,(CRTC2R(VSYNCE) & 0x7f));

		LOG(2,("CRTC2: PIXEL register: $%02x\n", CRTC2R(PIXEL)));
		LOG(2,("CRTC2: LCD register: $%02x\n", CRTC2R(LCD)));
		LOG(2,("CRTC2: register $59: $%02x\n", CRTC2R(0x59)));
		LOG(2,("CRTC2: register $9f: $%02x\n", CRTC2R(0x9f)));

		/* detect active slave device (if any) */
		slaved_for_dev2 = (CRTC2R(PIXEL) & 0x80);
		if (slaved_for_dev2)
		{
			/* if the panel isn't selected, tvout is.. */
			tvout2 = !(CRTC2R(LCD) & 0x01);
		}
	}

	LOG(2,("INFO: End flatpanel related CRTC registers dump.\n"));

	/* do some presets */
	si->ps.p1_timing.h_display = 0;
	si->ps.p1_timing.v_display = 0;
	si->ps.panel1_aspect = 0;
	si->ps.p2_timing.h_display = 0;
	si->ps.p2_timing.v_display = 0;
	si->ps.panel2_aspect = 0;
	si->ps.slaved_tmds1 = false;
	si->ps.slaved_tmds2 = false;
	si->ps.master_tmds1 = false;
	si->ps.master_tmds2 = false;
	si->ps.tmds1_active = false;
	si->ps.tmds2_active = false;
	/* determine the situation we are in... (regarding flatpanels) */
	/* fixme: add VESA DDC EDID stuff one day... */
	/* fixme: find out how to program those transmitters one day instead of
	 * relying on the cards BIOS to do it. This adds TVout options where panels
	 * are used!
	 * Currently we'd loose the panel setup while not being able to restore it. */

	/* note: (facts)
	 * -> NV11 and NV17 laptops have LVDS panels, programmed in both sets registers;
	 * -> NV34 laptops have TMDS panels, programmed in only one set of registers;
	 * -> NV11, NV25 and NV34 DVI cards, so external panels (TMDS) are programmed
	 *    in only one set of registers;
	 * -> a register-set's FP_TG_CTRL register, bit 31 tells you if a LVDS panel is
	 *    connected to the primary head (0), or to the secondary head (1) except
	 *    on some NV11's if this bit is '0' there;
	 * -> for LVDS panels both registersets are programmed identically by the card's
	 *    BIOSes;
	 * -> the programmed set of registers tells you where a TMDS (DVI) panel is
	 *    connected;
	 * -> On all cards a CRTC is used in slaved mode when a panel is connected,
	 *    except on NV11: here master mode is (might be?) detected. */
	/* note also:
	 * external TMDS encoders are only used for logic-level translation: it's 
	 * modeline registers are not used. Instead the GPU's internal modeline registers
	 * are used. The external encoder is not connected to a I2C bus (confirmed NV34). */
	if (slaved_for_dev1 && !tvout1)
	{
		uint16 width = ((DACR(FP_HDISPEND) & 0x0000ffff) + 1);
		uint16 height = ((DACR(FP_VDISPEND) & 0x0000ffff) + 1);
		if ((width >= 640) && (height >= 480))
		{
			si->ps.slaved_tmds1 = true;
			si->ps.tmds1_active = true;
			si->ps.p1_timing.h_display = width;
			si->ps.p1_timing.v_display = height;
		}
	}

	if (si->ps.secondary_head && slaved_for_dev2 && !tvout2)
	{
		uint16 width = ((DAC2R(FP_HDISPEND) & 0x0000ffff) + 1);
		uint16 height = ((DAC2R(FP_VDISPEND) & 0x0000ffff) + 1);
		if ((width >= 640) && (height >= 480))
		{
			si->ps.slaved_tmds2 = true;
			si->ps.tmds2_active = true;
			si->ps.p2_timing.h_display = width;
			si->ps.p2_timing.v_display = height;
		}
	}

	if ((si->ps.card_type == NV11) &&
		!si->ps.slaved_tmds1 && !tvout1)
	{
		uint16 width = ((DACR(FP_HDISPEND) & 0x0000ffff) + 1);
		uint16 height = ((DACR(FP_VDISPEND) & 0x0000ffff) + 1);
		if ((width >= 640) && (height >= 480))
		{
			si->ps.master_tmds1 = true;
			si->ps.tmds1_active = true;
			si->ps.p1_timing.h_display = width;
			si->ps.p1_timing.v_display = height;
		}
	}

	if ((si->ps.card_type == NV11) &&
		si->ps.secondary_head && !si->ps.slaved_tmds2 && !tvout2)
	{
		uint16 width = ((DAC2R(FP_HDISPEND) & 0x0000ffff) + 1);
		uint16 height = ((DAC2R(FP_VDISPEND) & 0x0000ffff) + 1);
		if ((width >= 640) && (height >= 480))
		{
			si->ps.master_tmds2 = true;
			si->ps.tmds2_active = true;
			si->ps.p2_timing.h_display = width;
			si->ps.p2_timing.v_display = height;
		}
	}

	//fixme...:
	//we are assuming that no DVI is used as external monitor on laptops;
	//otherwise we probably get into trouble here if the checked specs match.
	if (si->ps.laptop && si->ps.tmds1_active && si->ps.tmds2_active &&
		((DACR(FP_TG_CTRL) & 0x80000000) == (DAC2R(FP_TG_CTRL) & 0x80000000)) &&
		(si->ps.p1_timing.h_display == si->ps.p2_timing.h_display) &&
		(si->ps.p1_timing.v_display == si->ps.p2_timing.v_display))
	{
		LOG(2,("INFO: correcting double detection of single panel!\n"));

		if (si->ps.card_type == NV11)
		{
			/* LVDS panel is _always_ on CRTC2, so clear false primary detection */
			si->ps.slaved_tmds1 = false;
			si->ps.master_tmds1 = false;
			si->ps.tmds1_active = false;
			si->ps.p1_timing.h_display = 0;
			si->ps.p1_timing.v_display = 0;
		}
		else
		{
			if (DACR(FP_TG_CTRL) & 0x80000000)
			{
				/* LVDS panel is on CRTC2, so clear false primary detection */
				si->ps.slaved_tmds1 = false;
				si->ps.master_tmds1 = false;
				si->ps.tmds1_active = false;
				si->ps.p1_timing.h_display = 0;
				si->ps.p1_timing.v_display = 0;
			}
			else
			{
				/* LVDS panel is on CRTC1, so clear false secondary detection */
				si->ps.slaved_tmds2 = false;
				si->ps.master_tmds2 = false;
				si->ps.tmds2_active = false;
				si->ps.p2_timing.h_display = 0;
				si->ps.p2_timing.v_display = 0;
			}
		}
	}

	/* fetch panel(s) modeline(s) */
	if (si->ps.tmds1_active)
	{
		/* determine panel aspect ratio */
		si->ps.panel1_aspect =
			(si->ps.p1_timing.h_display / ((float)si->ps.p1_timing.v_display));
		/* horizontal timing */
		si->ps.p1_timing.h_sync_start = (DACR(FP_HSYNC_S) & 0x0000ffff) + 1;
		si->ps.p1_timing.h_sync_end = (DACR(FP_HSYNC_E) & 0x0000ffff) + 1;
		si->ps.p1_timing.h_total = (DACR(FP_HTOTAL) & 0x0000ffff) + 1;
		/* vertical timing */
		si->ps.p1_timing.v_sync_start = (DACR(FP_VSYNC_S) & 0x0000ffff) + 1;
		si->ps.p1_timing.v_sync_end = (DACR(FP_VSYNC_E) & 0x0000ffff) + 1;
		si->ps.p1_timing.v_total = (DACR(FP_VTOTAL) & 0x0000ffff) + 1;
		/* sync polarity */
		si->ps.p1_timing.flags = 0;
		if (DACR(FP_TG_CTRL) & 0x00000001) si->ps.p1_timing.flags |= B_POSITIVE_VSYNC;
		if (DACR(FP_TG_CTRL) & 0x00000010) si->ps.p1_timing.flags |= B_POSITIVE_HSYNC;
		/* refreshrate:
		 * fix a DVI or laptop flatpanel to 60Hz refresh! */
		si->ps.p1_timing.pixel_clock =
			(si->ps.p1_timing.h_total * si->ps.p1_timing.v_total * 60) / 1000;
	}
	if (si->ps.tmds2_active)
	{
		/* determine panel aspect ratio */
		si->ps.panel2_aspect =
			(si->ps.p2_timing.h_display / ((float)si->ps.p2_timing.v_display));
		/* horizontal timing */
		si->ps.p2_timing.h_sync_start = (DAC2R(FP_HSYNC_S) & 0x0000ffff) + 1;
		si->ps.p2_timing.h_sync_end = (DAC2R(FP_HSYNC_E) & 0x0000ffff) + 1;
		si->ps.p2_timing.h_total = (DAC2R(FP_HTOTAL) & 0x0000ffff) + 1;
		/* vertical timing */
		si->ps.p2_timing.v_sync_start = (DAC2R(FP_VSYNC_S) & 0x0000ffff) + 1;
		si->ps.p2_timing.v_sync_end = (DAC2R(FP_VSYNC_E) & 0x0000ffff) + 1;
		si->ps.p2_timing.v_total = (DAC2R(FP_VTOTAL) & 0x0000ffff) + 1;
		/* sync polarity */
		si->ps.p2_timing.flags = 0;
		if (DAC2R(FP_TG_CTRL) & 0x00000001) si->ps.p2_timing.flags |= B_POSITIVE_VSYNC;
		if (DAC2R(FP_TG_CTRL) & 0x00000010) si->ps.p2_timing.flags |= B_POSITIVE_HSYNC;
		/* refreshrate:
		 * fix a DVI or laptop flatpanel to 60Hz refresh! */
		si->ps.p2_timing.pixel_clock =
			(si->ps.p2_timing.h_total * si->ps.p2_timing.v_total * 60) / 1000;
	}

	/* dump some panel configuration registers... */
	LOG(2,("INFO: Dumping flatpanel registers:\n"));
	LOG(2,("DUALHEAD_CTRL: $%08x\n", NV_REG32(NV32_DUALHEAD_CTRL)));
	LOG(2,("DAC1: FP_HDISPEND: %d\n", DACR(FP_HDISPEND)));
	LOG(2,("DAC1: FP_HTOTAL: %d\n", DACR(FP_HTOTAL)));
	LOG(2,("DAC1: FP_HCRTC: %d\n", DACR(FP_HCRTC)));
	LOG(2,("DAC1: FP_HSYNC_S: %d\n", DACR(FP_HSYNC_S)));
	LOG(2,("DAC1: FP_HSYNC_E: %d\n", DACR(FP_HSYNC_E)));
	LOG(2,("DAC1: FP_HVALID_S: %d\n", DACR(FP_HVALID_S)));
	LOG(2,("DAC1: FP_HVALID_E: %d\n", DACR(FP_HVALID_E)));

	LOG(2,("DAC1: FP_VDISPEND: %d\n", DACR(FP_VDISPEND)));
	LOG(2,("DAC1: FP_VTOTAL: %d\n", DACR(FP_VTOTAL)));
	LOG(2,("DAC1: FP_VCRTC: %d\n", DACR(FP_VCRTC)));
	LOG(2,("DAC1: FP_VSYNC_S: %d\n", DACR(FP_VSYNC_S)));
	LOG(2,("DAC1: FP_VSYNC_E: %d\n", DACR(FP_VSYNC_E)));
	LOG(2,("DAC1: FP_VVALID_S: %d\n", DACR(FP_VVALID_S)));
	LOG(2,("DAC1: FP_VVALID_E: %d\n", DACR(FP_VVALID_E)));

	LOG(2,("DAC1: FP_CHKSUM: $%08x = (dec) %d\n", DACR(FP_CHKSUM),DACR(FP_CHKSUM)));
	LOG(2,("DAC1: FP_TST_CTRL: $%08x\n", DACR(FP_TST_CTRL)));
	LOG(2,("DAC1: FP_TG_CTRL: $%08x\n", DACR(FP_TG_CTRL)));
	LOG(2,("DAC1: FP_DEBUG0: $%08x\n", DACR(FP_DEBUG0)));
	LOG(2,("DAC1: FP_DEBUG1: $%08x\n", DACR(FP_DEBUG1)));
	LOG(2,("DAC1: FP_DEBUG2: $%08x\n", DACR(FP_DEBUG2)));
	LOG(2,("DAC1: FP_DEBUG3: $%08x\n", DACR(FP_DEBUG3)));

	LOG(2,("DAC1: FUNCSEL: $%08x\n", NV_REG32(NV32_FUNCSEL)));
	LOG(2,("DAC1: PANEL_PWR: $%08x\n", NV_REG32(NV32_PANEL_PWR)));

	if(si->ps.secondary_head)
	{
		LOG(2,("DAC2: FP_HDISPEND: %d\n", DAC2R(FP_HDISPEND)));
		LOG(2,("DAC2: FP_HTOTAL: %d\n", DAC2R(FP_HTOTAL)));
		LOG(2,("DAC2: FP_HCRTC: %d\n", DAC2R(FP_HCRTC)));
		LOG(2,("DAC2: FP_HSYNC_S: %d\n", DAC2R(FP_HSYNC_S)));
		LOG(2,("DAC2: FP_HSYNC_E: %d\n", DAC2R(FP_HSYNC_E)));
		LOG(2,("DAC2: FP_HVALID_S:%d\n", DAC2R(FP_HVALID_S)));
		LOG(2,("DAC2: FP_HVALID_E: %d\n", DAC2R(FP_HVALID_E)));

		LOG(2,("DAC2: FP_VDISPEND: %d\n", DAC2R(FP_VDISPEND)));
		LOG(2,("DAC2: FP_VTOTAL: %d\n", DAC2R(FP_VTOTAL)));
		LOG(2,("DAC2: FP_VCRTC: %d\n", DAC2R(FP_VCRTC)));
		LOG(2,("DAC2: FP_VSYNC_S: %d\n", DAC2R(FP_VSYNC_S)));
		LOG(2,("DAC2: FP_VSYNC_E: %d\n", DAC2R(FP_VSYNC_E)));
		LOG(2,("DAC2: FP_VVALID_S: %d\n", DAC2R(FP_VVALID_S)));
		LOG(2,("DAC2: FP_VVALID_E: %d\n", DAC2R(FP_VVALID_E)));

		LOG(2,("DAC2: FP_CHKSUM: $%08x = (dec) %d\n", DAC2R(FP_CHKSUM),DAC2R(FP_CHKSUM)));
		LOG(2,("DAC2: FP_TST_CTRL: $%08x\n", DAC2R(FP_TST_CTRL)));
		LOG(2,("DAC2: FP_TG_CTRL: $%08x\n", DAC2R(FP_TG_CTRL)));
		LOG(2,("DAC2: FP_DEBUG0: $%08x\n", DAC2R(FP_DEBUG0)));
		LOG(2,("DAC2: FP_DEBUG1: $%08x\n", DAC2R(FP_DEBUG1)));
		LOG(2,("DAC2: FP_DEBUG2: $%08x\n", DAC2R(FP_DEBUG2)));
		LOG(2,("DAC2: FP_DEBUG3: $%08x\n", DAC2R(FP_DEBUG3)));

		LOG(2,("DAC2: FUNCSEL: $%08x\n", NV_REG32(NV32_2FUNCSEL)));
		LOG(2,("DAC2: PANEL_PWR: $%08x\n", NV_REG32(NV32_2PANEL_PWR)));
	}
	LOG(2,("INFO: End flatpanel registers dump.\n"));
}

static void setup_output_matrix()
{
	/* setup defaults: */
	/* no monitors (output devices) detected */
	si->ps.monitors = 0x00;
	/* head 1 will be the primary head */
	si->ps.crtc2_prim = false;

	/* setup output devices and heads */
	if (si->ps.secondary_head)
	{
		if (si->ps.card_type != NV11)
		{
			/* setup defaults: */
			/* connect analog outputs straight through */
			nv_general_output_select(false);

			/* presetup by the card's BIOS, we can't change this (lack of info) */
			if (si->ps.tmds1_active) si->ps.monitors |= 0x01;
			if (si->ps.tmds2_active) si->ps.monitors |= 0x10;
			/* detect analog monitors (confirmed working OK on NV18, NV28 and NV34): */
			/* sense analog monitor on primary connector */
			if (nv_dac_crt_connected()) si->ps.monitors |= 0x02;
			/* sense analog monitor on secondary connector */
			if (nv_dac2_crt_connected()) si->ps.monitors |= 0x20;

			/* setup correct output and head use */
			//fixme? add TVout (only, so no CRT(s) connected) support...
			switch (si->ps.monitors)
			{
			case 0x00: /* no monitor found at all */
				LOG(2,("INFO: head 1 has nothing connected;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x01: /* digital panel on head 1, nothing on head 2 */
				LOG(2,("INFO: head 1 has a digital panel;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x02: /* analog panel or CRT on head 1, nothing on head 2 */
				LOG(2,("INFO: head 1 has an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x03: /* both types on head 1, nothing on head 2 */
				LOG(2,("INFO: head 1 has a digital panel AND an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: correcting...\n"));
				/* cross connect analog outputs so analog panel or CRT gets head 2 */
				nv_general_output_select(true);
				LOG(2,("INFO: head 1 has a digital panel;\n"));
				LOG(2,("INFO: head 2 has an analog panel or CRT:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x10: /* nothing on head 1, digital panel on head 2 */
				LOG(2,("INFO: head 1 has nothing connected;\n"));
				LOG(2,("INFO: head 2 has a digital panel:\n"));
				LOG(2,("INFO: defaulting to head 2 for primary use.\n"));
				si->ps.crtc2_prim = true;
				break;
			case 0x20: /* nothing on head 1, analog panel or CRT on head 2 */
				LOG(2,("INFO: head 1 has nothing connected;\n"));
				LOG(2,("INFO: head 2 has an analog panel or CRT:\n"));
				LOG(2,("INFO: defaulting to head 2 for primary use.\n"));
				si->ps.crtc2_prim = true;
				break;
			case 0x30: /* nothing on head 1, both types on head 2 */
				LOG(2,("INFO: head 1 has nothing connected;\n"));
				LOG(2,("INFO: head 2 has a digital panel AND an analog panel or CRT:\n"));
				LOG(2,("INFO: correcting...\n"));
				/* cross connect analog outputs so analog panel or CRT gets head 1 */
				nv_general_output_select(true);
				LOG(2,("INFO: head 1 has an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has a digital panel:\n"));
				LOG(2,("INFO: defaulting to head 2 for primary use.\n"));
				si->ps.crtc2_prim = true;
				break;
			case 0x11: /* digital panels on both heads */
				LOG(2,("INFO: head 1 has a digital panel;\n"));
				LOG(2,("INFO: head 2 has a digital panel:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x12: /* analog panel or CRT on head 1, digital panel on head 2 */
				LOG(2,("INFO: head 1 has an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has a digital panel:\n"));
				LOG(2,("INFO: defaulting to head 2 for primary use.\n"));
				si->ps.crtc2_prim = true;
				break;
			case 0x21: /* digital panel on head 1, analog panel or CRT on head 2 */
				LOG(2,("INFO: head 1 has a digital panel;\n"));
				LOG(2,("INFO: head 2 has an analog panel or CRT:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x22: /* analog panel(s) or CRT(s) on both heads */
				LOG(2,("INFO: head 1 has an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has an analog panel or CRT:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			default: /* more than two monitors connected to just two outputs: illegal! */
				LOG(2,("INFO: illegal monitor setup ($%02x):\n", si->ps.monitors));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			}
		}
		else /* dualhead NV11 cards */
		{
			/* confirmed no analog output switch-options for NV11 */
			LOG(2,("INFO: NV11 outputs are hardwired to be straight-through\n"));

			/* presetup by the card's BIOS, we can't change this (lack of info) */
			if (si->ps.tmds1_active) si->ps.monitors |= 0x01;
			if (si->ps.tmds2_active) si->ps.monitors |= 0x10;
			/* detect analog monitor (confirmed working OK on NV11): */
			/* sense analog monitor on primary connector */
			if (nv_dac_crt_connected()) si->ps.monitors |= 0x02;
			/* (sense analog monitor on secondary connector is impossible on NV11) */

			/* setup correct output and head use */
			//fixme? add TVout (only, so no CRT(s) connected) support...
			switch (si->ps.monitors)
			{
			case 0x00: /* no monitor found at all */
				LOG(2,("INFO: head 1 has nothing connected;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x01: /* digital panel on head 1, nothing on head 2 */
				LOG(2,("INFO: head 1 has a digital panel;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x02: /* analog panel or CRT on head 1, nothing on head 2 */
				LOG(2,("INFO: head 1 has an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x03: /* both types on head 1, nothing on head 2 */
				LOG(2,("INFO: head 1 has a digital panel AND an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has nothing connected:\n"));
				LOG(2,("INFO: correction not possible...\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x10: /* nothing on head 1, digital panel on head 2 */
				LOG(2,("INFO: head 1 has nothing connected;\n"));
				LOG(2,("INFO: head 2 has a digital panel:\n"));
				LOG(2,("INFO: defaulting to head 2 for primary use.\n"));
				si->ps.crtc2_prim = true;
				break;
			case 0x11: /* digital panels on both heads */
				LOG(2,("INFO: head 1 has a digital panel;\n"));
				LOG(2,("INFO: head 2 has a digital panel:\n"));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			case 0x12: /* analog panel or CRT on head 1, digital panel on head 2 */
				LOG(2,("INFO: head 1 has an analog panel or CRT;\n"));
				LOG(2,("INFO: head 2 has a digital panel:\n"));
				LOG(2,("INFO: defaulting to head 2 for primary use.\n"));
				si->ps.crtc2_prim = true;
				break;
			default: /* more than two monitors connected to just two outputs: illegal! */
				LOG(2,("INFO: illegal monitor setup ($%02x):\n", si->ps.monitors));
				LOG(2,("INFO: defaulting to head 1 for primary use.\n"));
				break;
			}
		}
	}
	else /* singlehead cards */
	{
		/* presetup by the card's BIOS, we can't change this (lack of info) */
		if (si->ps.tmds1_active) si->ps.monitors |= 0x01;
		/* detect analog monitor (confirmed working OK on all cards): */
		/* sense analog monitor on primary connector */
		if (nv_dac_crt_connected()) si->ps.monitors |= 0x02;

		//fixme? add TVout (only, so no CRT connected) support...
	}
}

void get_panel_modes(display_mode *p1, display_mode *p2, bool *pan1, bool *pan2)
{
	if (si->ps.tmds1_active)
	{
		/* timing ('modeline') */
		p1->timing = si->ps.p1_timing;
		/* setup the rest */
		p1->space = B_CMAP8;
		p1->virtual_width = p1->timing.h_display;
		p1->virtual_height = p1->timing.v_display;
		p1->h_display_start = 0;
		p1->v_display_start = 0;
		p1->flags = 0;
		*pan1 = true;
	}
	else
		*pan1 = false;

	if (si->ps.tmds2_active)
	{
		/* timing ('modeline') */
		p2->timing = si->ps.p2_timing;
		/* setup the rest */
		p2->space = B_CMAP8;
		p2->virtual_width = p2->timing.h_display;
		p2->virtual_height = p2->timing.v_display;
		p2->h_display_start = 0;
		p2->v_display_start = 0;
		p2->flags = 0;
		*pan2 = true;
	}
	else
		*pan2 = false;
}

static void pinsnv4_fake(void)
{
	/* carefull not to take to high limits, and high should be >= 2x low. */
	si->ps.max_system_vco = 256;
	si->ps.min_system_vco = 128;
	si->ps.max_pixel_vco = 256;
	si->ps.min_pixel_vco = 128;
	si->ps.max_video_vco = 0;
	si->ps.min_video_vco = 0;
	si->ps.max_dac1_clock = 250;
	si->ps.max_dac1_clock_8 = 250;
	si->ps.max_dac1_clock_16 = 250;
	/* 'failsave' values */
	si->ps.max_dac1_clock_24 = 220;
	si->ps.max_dac1_clock_32 = 180;
	si->ps.max_dac1_clock_32dh = 180;
	/* secondary head */
	si->ps.max_dac2_clock = 0;
	si->ps.max_dac2_clock_8 = 0;
	si->ps.max_dac2_clock_16 = 0;
	si->ps.max_dac2_clock_24 = 0;
	si->ps.max_dac2_clock_32 = 0;
	/* 'failsave' values */
	si->ps.max_dac2_clock_32dh = 0;
	//fixme: primary & secondary_dvi should be overrule-able via nv.settings
	si->ps.primary_dvi = false;
	si->ps.secondary_dvi = false;
	/* not used (yet) because no coldstart will be attempted (yet) */
	si->ps.std_engine_clock = 90;
	si->ps.std_memory_clock = 110;
}

static void pinsnv5_nv5m64_fake(void)
{
	/* carefull not to take to high limits, and high should be >= 2x low. */
	si->ps.max_system_vco = 300;
	si->ps.min_system_vco = 128;
	si->ps.max_pixel_vco = 300;
	si->ps.min_pixel_vco = 128;
	si->ps.max_video_vco = 0;
	si->ps.min_video_vco = 0;
	si->ps.max_dac1_clock = 300;
	si->ps.max_dac1_clock_8 = 300;
	si->ps.max_dac1_clock_16 = 300;
	/* 'failsave' values */
	si->ps.max_dac1_clock_24 = 270;
	si->ps.max_dac1_clock_32 = 230;
	si->ps.max_dac1_clock_32dh = 230;
	/* secondary head */
	si->ps.max_dac2_clock = 0;
	si->ps.max_dac2_clock_8 = 0;
	si->ps.max_dac2_clock_16 = 0;
	si->ps.max_dac2_clock_24 = 0;
	si->ps.max_dac2_clock_32 = 0;
	/* 'failsave' values */
	si->ps.max_dac2_clock_32dh = 0;
	//fixme: primary & secondary_dvi should be overrule-able via nv.settings
	si->ps.primary_dvi = false;
	si->ps.secondary_dvi = false;
	/* not used (yet) because no coldstart will be attempted (yet) */
	si->ps.std_engine_clock = 125;
	si->ps.std_memory_clock = 150;
}

static void pinsnv6_fake(void)
{
	/* carefull not to take to high limits, and high should be >= 2x low. */
	si->ps.max_system_vco = 300;
	si->ps.min_system_vco = 128;
	si->ps.max_pixel_vco = 300;
	si->ps.min_pixel_vco = 128;
	si->ps.max_video_vco = 0;
	si->ps.min_video_vco = 0;
	si->ps.max_dac1_clock = 300;
	si->ps.max_dac1_clock_8 = 300;
	si->ps.max_dac1_clock_16 = 300;
	/* 'failsave' values */
	si->ps.max_dac1_clock_24 = 270;
	si->ps.max_dac1_clock_32 = 230;
	si->ps.max_dac1_clock_32dh = 230;
	/* secondary head */
	si->ps.max_dac2_clock = 0;
	si->ps.max_dac2_clock_8 = 0;
	si->ps.max_dac2_clock_16 = 0;
	si->ps.max_dac2_clock_24 = 0;
	si->ps.max_dac2_clock_32 = 0;
	/* 'failsave' values */
	si->ps.max_dac2_clock_32dh = 0;
	//fixme: primary & secondary_dvi should be overrule-able via nv.settings
	si->ps.primary_dvi = false;
	si->ps.secondary_dvi = false;
	/* not used (yet) because no coldstart will be attempted (yet) */
	si->ps.std_engine_clock = 100;
	si->ps.std_memory_clock = 125;
}

static void pinsnv10_arch_fake(void)
{
	/* carefull not to take to high limits, and high should be >= 2x low. */
	si->ps.max_system_vco = 350;
	si->ps.min_system_vco = 128;
	si->ps.max_pixel_vco = 350;
	si->ps.min_pixel_vco = 128;
	si->ps.max_video_vco = 350;
	si->ps.min_video_vco = 128;
	si->ps.max_dac1_clock = 350;
	si->ps.max_dac1_clock_8 = 350;
	si->ps.max_dac1_clock_16 = 350;
	/* 'failsave' values */
	si->ps.max_dac1_clock_24 = 320;
	si->ps.max_dac1_clock_32 = 280;
	si->ps.max_dac1_clock_32dh = 250;
	/* secondary head */
	if (si->ps.card_type < NV17)
	{
		/* if a GeForce2 has analog VGA dualhead capability,
		 * it uses an external secondary DAC probably with limited capability. */
		/* (called twinview technology) */
		si->ps.max_dac2_clock = 200;
		si->ps.max_dac2_clock_8 = 200;
		si->ps.max_dac2_clock_16 = 200;
		si->ps.max_dac2_clock_24 = 200;
		si->ps.max_dac2_clock_32 = 200;
		/* 'failsave' values */
		si->ps.max_dac2_clock_32dh = 180;
	}
	else
	{
		/* GeForce4 cards have dual integrated DACs with identical capaability */
		/* (called nview technology) */
		si->ps.max_dac2_clock = 350;
		si->ps.max_dac2_clock_8 = 350;
		si->ps.max_dac2_clock_16 = 350;
		/* 'failsave' values */
		si->ps.max_dac2_clock_24 = 320;
		si->ps.max_dac2_clock_32 = 280;
		si->ps.max_dac2_clock_32dh = 250;
	}
	//fixme: primary & secondary_dvi should be overrule-able via nv.settings
	si->ps.primary_dvi = false;
	si->ps.secondary_dvi = false;
	/* not used (yet) because no coldstart will be attempted (yet) */
	si->ps.std_engine_clock = 120;
	si->ps.std_memory_clock = 150;
}

static void pinsnv20_arch_fake(void)
{
	/* carefull not to take to high limits, and high should be >= 2x low. */
	si->ps.max_system_vco = 350;
	si->ps.min_system_vco = 128;
	si->ps.max_pixel_vco = 350;
	si->ps.min_pixel_vco = 128;
	si->ps.max_video_vco = 350;
	si->ps.min_video_vco = 128;
	si->ps.max_dac1_clock = 350;
	si->ps.max_dac1_clock_8 = 350;
	si->ps.max_dac1_clock_16 = 350;
	/* 'failsave' values */
	si->ps.max_dac1_clock_24 = 320;
	si->ps.max_dac1_clock_32 = 280;
	si->ps.max_dac1_clock_32dh = 250;
	/* secondary head */
	/* GeForce4 cards have dual integrated DACs with identical capaability */
	/* (called nview technology) */
	si->ps.max_dac2_clock = 350;
	si->ps.max_dac2_clock_8 = 350;
	si->ps.max_dac2_clock_16 = 350;
	/* 'failsave' values */
	si->ps.max_dac2_clock_24 = 320;
	si->ps.max_dac2_clock_32 = 280;
	si->ps.max_dac2_clock_32dh = 250;
	//fixme: primary & secondary_dvi should be overrule-able via nv.settings
	si->ps.primary_dvi = false;
	si->ps.secondary_dvi = false;
	/* not used (yet) because no coldstart will be attempted (yet) */
	si->ps.std_engine_clock = 175;
	si->ps.std_memory_clock = 200;
}

static void pinsnv30_arch_fake(void)
{
	/* carefull not to take to high limits, and high should be >= 2x low. */
	si->ps.max_system_vco = 350;
	si->ps.min_system_vco = 128;
	si->ps.max_pixel_vco = 350;
	si->ps.min_pixel_vco = 128;
	si->ps.max_video_vco = 350;
	si->ps.min_video_vco = 128;
	si->ps.max_dac1_clock = 350;
	si->ps.max_dac1_clock_8 = 350;
	si->ps.max_dac1_clock_16 = 350;
	/* 'failsave' values */
	si->ps.max_dac1_clock_24 = 320;
	si->ps.max_dac1_clock_32 = 280;
	si->ps.max_dac1_clock_32dh = 250;
	/* secondary head */
	/* GeForceFX cards have dual integrated DACs with identical capaability */
	/* (called nview technology) */
	si->ps.max_dac2_clock = 350;
	si->ps.max_dac2_clock_8 = 350;
	si->ps.max_dac2_clock_16 = 350;
	/* 'failsave' values */
	si->ps.max_dac2_clock_24 = 320;
	si->ps.max_dac2_clock_32 = 280;
	si->ps.max_dac2_clock_32dh = 250;
	//fixme: primary & secondary_dvi should be overrule-able via nv.settings
	si->ps.primary_dvi = false;
	si->ps.secondary_dvi = false;
	/* not used (yet) because no coldstart will be attempted (yet) */
	si->ps.std_engine_clock = 190;
	si->ps.std_memory_clock = 190;
}

static void getstrap_arch_nv4(void)
{
	uint32 strapinfo = NV_REG32(NV32_NV4STRAPINFO);

	if (strapinfo & 0x00000100)
	{
		/* Unified memory architecture used */
		si->ps.memory_size = 1024 * 1024 *
			((((strapinfo & 0x0000f000) >> 12) * 2) + 2);

		LOG(8,("INFO: NV4 architecture chip with UMA detected\n"));
	}
	else
	{
		/* private memory architecture used */
		switch (strapinfo & 0x00000003)
		{
		case 0:
			si->ps.memory_size = 32 * 1024 * 1024;
			break;
		case 1:
			si->ps.memory_size = 4 * 1024 * 1024;
			break;
		case 2:
			si->ps.memory_size = 8 * 1024 * 1024;
			break;
		case 3:
			si->ps.memory_size = 16 * 1024 * 1024;
			break;
		}
	}

	strapinfo = NV_REG32(NV32_NVSTRAPINFO2);

	/* determine PLL reference crystal frequency */
	if (strapinfo & 0x00000040)
		si->ps.f_ref = 14.31818;
	else
		si->ps.f_ref = 13.50000;

	/* these cards are always singlehead */
	si->ps.secondary_head = false;
}

static void getstrap_arch_nv10_20_30(void)
{
	uint32 dev_manID = CFGR(DEVID);
	uint32 strapinfo = NV_REG32(NV32_NV10STRAPINFO);

	switch (dev_manID)
	{
	case 0x01a010de: /* Nvidia GeForce2 Integrated GPU */
	case 0x01f010de: /* Nvidia GeForce4 MX Integrated GPU */
		/* the kerneldriver already determined the amount of RAM these cards have at
		 * their disposal (UMA, values read from PCI config space in other device) */
		LOG(8,("INFO: nVidia GPU with UMA detected\n"));
		break;
	default:
		LOG(8,("INFO: (Memory detection) Strapinfo value is: $%08x\n", strapinfo));

		switch ((strapinfo & 0x1ff00000) >> 20)
		{
		case 2:
			si->ps.memory_size = 2 * 1024 * 1024;
			break;
		case 4:
			si->ps.memory_size = 4 * 1024 * 1024;
			break;
		case 8:
			si->ps.memory_size = 8 * 1024 * 1024;
			break;
		case 16:
			si->ps.memory_size = 16 * 1024 * 1024;
			break;
		case 32:
			si->ps.memory_size = 32 * 1024 * 1024;
			break;
		case 64:
			si->ps.memory_size = 64 * 1024 * 1024;
			break;
		case 128:
			si->ps.memory_size = 128 * 1024 * 1024;
			break;
		case 256:
			si->ps.memory_size = 256 * 1024 * 1024;
			break;
		default:
			si->ps.memory_size = 16 * 1024 * 1024;

			LOG(8,("INFO: NV10/20/30 architecture chip with unknown RAM amount detected;\n"));
			LOG(8,("INFO: Setting 16Mb\n"));
			break;
		}
	}

	strapinfo = NV_REG32(NV32_NVSTRAPINFO2);

	/* determine PLL reference crystal frequency: three types are used... */
	if (strapinfo & 0x00000040)
		si->ps.f_ref = 14.31818;
	else
		si->ps.f_ref = 13.50000;

	switch (dev_manID & 0xfff0ffff)
	{
	/* Nvidia cards: */
	case 0x017010de:
	case 0x018010de:
	case 0x01f010de:
	case 0x025010de:
	case 0x028010de:
	case 0x030010de:
	case 0x031010de:
	case 0x032010de:
	case 0x033010de:
	case 0x034010de:
	/* Varisys cards: */
	case 0x35001888:
		if (strapinfo & 0x00400000) si->ps.f_ref = 27.00000;
		break;
	default:
		break;
	}

	/* determine if we have a dualhead card */
	switch (dev_manID & 0xfff0ffff)
	{
	/* Nvidia cards: */
	case 0x011010de:
	case 0x017010de:
	case 0x018010de:
	case 0x01f010de:
	case 0x025010de:
	case 0x028010de:
	case 0x030010de:
	case 0x031010de:
	case 0x032010de:
	case 0x033010de:
	case 0x034010de:
	/* Varisys cards: */
	case 0x35001888:
		si->ps.secondary_head = true;
		break;
	default:
		si->ps.secondary_head = false;
		break;
	}
}

void dump_pins(void)
{
	char *msg = "";

	LOG(2,("INFO: pinsdump follows:\n"));
	LOG(2,("f_ref: %fMhz\n", si->ps.f_ref));
	LOG(2,("max_system_vco: %dMhz\n", si->ps.max_system_vco));
	LOG(2,("min_system_vco: %dMhz\n", si->ps.min_system_vco));
	LOG(2,("max_pixel_vco: %dMhz\n", si->ps.max_pixel_vco));
	LOG(2,("min_pixel_vco: %dMhz\n", si->ps.min_pixel_vco));
	LOG(2,("max_video_vco: %dMhz\n", si->ps.max_video_vco));
	LOG(2,("min_video_vco: %dMhz\n", si->ps.min_video_vco));
	LOG(2,("std_engine_clock: %dMhz\n", si->ps.std_engine_clock));
	LOG(2,("std_memory_clock: %dMhz\n", si->ps.std_memory_clock));
	LOG(2,("max_dac1_clock: %dMhz\n", si->ps.max_dac1_clock));
	LOG(2,("max_dac1_clock_8: %dMhz\n", si->ps.max_dac1_clock_8));
	LOG(2,("max_dac1_clock_16: %dMhz\n", si->ps.max_dac1_clock_16));
	LOG(2,("max_dac1_clock_24: %dMhz\n", si->ps.max_dac1_clock_24));
	LOG(2,("max_dac1_clock_32: %dMhz\n", si->ps.max_dac1_clock_32));
	LOG(2,("max_dac1_clock_32dh: %dMhz\n", si->ps.max_dac1_clock_32dh));
	LOG(2,("max_dac2_clock: %dMhz\n", si->ps.max_dac2_clock));
	LOG(2,("max_dac2_clock_8: %dMhz\n", si->ps.max_dac2_clock_8));
	LOG(2,("max_dac2_clock_16: %dMhz\n", si->ps.max_dac2_clock_16));
	LOG(2,("max_dac2_clock_24: %dMhz\n", si->ps.max_dac2_clock_24));
	LOG(2,("max_dac2_clock_32: %dMhz\n", si->ps.max_dac2_clock_32));
	LOG(2,("max_dac2_clock_32dh: %dMhz\n", si->ps.max_dac2_clock_32dh));
	LOG(2,("secondary_head: "));
	if (si->ps.secondary_head) LOG(2,("present\n")); else LOG(2,("absent\n"));
	LOG(2,("tvout: "));
	if (si->ps.tvout) LOG(2,("present\n")); else LOG(2,("absent\n"));
	/* setup TVout logmessage text */
	switch (si->ps.tvout_chip_type)
	{
	case NONE:
		msg = "No";
		break;
	case CH7003:
		msg = "Chrontel CH7003";
		break;
	case CH7004:
		msg = "Chrontel CH7004";
		break;
	case CH7005:
		msg = "Chrontel CH7005";
		break;
	case CH7006:
		msg = "Chrontel CH7006";
		break;
	case CH7007:
		msg = "Chrontel CH7007";
		break;
	case CH7008:
		msg = "Chrontel CH7008";
		break;
	case SAA7102:
		msg = "Philips SAA7102";
		break;
	case SAA7103:
		msg = "Philips SAA7103";
		break;
	case SAA7104:
		msg = "Philips SAA7104";
		break;
	case SAA7105:
		msg = "Philips SAA7105";
		break;
	case BT868:
		msg = "Brooktree/Conexant BT868";
		break;
	case BT869:
		msg = "Brooktree/Conexant BT869";
		break;
	case CX25870:
		msg = "Conexant CX25870";
		break;
	case CX25871:
		msg = "Conexant CX25871";
		break;
	case NVIDIA:
		msg = "Nvidia internal";
		break;
	default:
		msg = "Unknown";
		break;
	}
	LOG(2, ("%s TVout chip detected\n", msg));
//	LOG(2,("primary_dvi: "));
//	if (si->ps.primary_dvi) LOG(2,("present\n")); else LOG(2,("absent\n"));
//	LOG(2,("secondary_dvi: "));
//	if (si->ps.secondary_dvi) LOG(2,("present\n")); else LOG(2,("absent\n"));
	LOG(2,("card memory_size: %3.3fMb\n", (si->ps.memory_size / (1024.0 * 1024.0))));
	LOG(2,("laptop: "));
	if (si->ps.laptop) LOG(2,("yes\n")); else LOG(2,("no\n"));
	if (si->ps.tmds1_active)
	{
		LOG(2,("found DFP (digital flatpanel) on CRTC1; CRTC1 is "));
		if (si->ps.slaved_tmds1) LOG(2,("slaved\n")); else LOG(2,("master\n"));
		LOG(2,("panel width: %d, height: %d, aspect ratio: %1.2f\n",
			si->ps.p1_timing.h_display, si->ps.p1_timing.v_display, si->ps.panel1_aspect));
	}
	if (si->ps.tmds2_active)
	{
		LOG(2,("found DFP (digital flatpanel) on CRTC2; CRTC2 is "));
		if (si->ps.slaved_tmds2) LOG(2,("slaved\n")); else LOG(2,("master\n"));
		LOG(2,("panel width: %d, height: %d, aspect ratio: %1.2f\n",
			si->ps.p2_timing.h_display, si->ps.p2_timing.v_display, si->ps.panel2_aspect));
	}
	LOG(2,("monitor (output devices) setup matrix: $%02x\n", si->ps.monitors));
	LOG(2,("INFO: end pinsdump.\n"));
}

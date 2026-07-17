/*
 * setup_hdmi - minimal standalone ADV7513 HDMI transmitter setup for the
 * MiSTer / DE10-Nano, fixed at 1920x1080.
 *
 * Programs the ADV7513 over I2C (the HPS I2C peripheral routed through the
 * FPGA fabric to the HDMI pins, so a bitstream wiring
 * cyclonev_hps_interface_peripheral_i2c must already be loaded) for the
 * video the simplex sys_top/SPG fabric produces:
 *
 *   - 1920x1080 CEA-861 (VIC 16), 2200x1125 totals, both syncs POSITIVE
 *     (SPG.sv) at a fixed 148.5/148.352 MHz pixel clock (pll_hdmi in
 *     sys_top.v is never reconfigured)
 *   - 24-bit RGB 444 input, no pixel repetition
 *   - I2S audio, 48 kHz, 16-bit (HDMI_MCLK/SCLK/LRCLK/I2S from sys_top)
 *
 * Derived from Main_MiSTer's video.cpp (hdmi_config_init /
 * hdmi_config_set_csc / hdmi_config_set_mode)
 * (https://github.com/MiSTer-devel/Main_MiSTer).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "smbus.h"

static int hdmi_main_fd = -1;

static void write_regs(const uint8_t *data, unsigned len)
{
	for (unsigned i = 0; i < len; i += 2)
	{
		int res = i2c_smbus_write_byte_data(hdmi_main_fd, data[i], data[i + 1]);
		if (res < 0) printf("i2c: write error (%02X %02X): %d\n", data[i], data[i + 1], res);
	}
}

static void hdmi_config_set_csc(int limited)
{
	// identity matrix (full range) or the limited-range compression matrix,
	// in the ADV7513 [-2 .. 2] fixed-point CSC format (video.cpp with all
	// color controls at their defaults)
	double gain   = limited ? 0.8583984375 : 1.0;
	double offset = limited ? 0.0625       : 0.0;

	double coeffs[12] = {
		gain, 0.0,  0.0,  offset,
		0.0,  gain, 0.0,  offset,
		0.0,  0.0,  gain, offset,
	};

	int16_t csc_int16[12];
	for (unsigned i = 0; i < 12; i++) csc_int16[i] = (int16_t)(coeffs[i] * 2048.0);

	// Clamps to reinforce limited if necessary
	// 0x100 = 16/256 * 4096 (12-bit mul)
	// 0xEB0 = 235/256 * 4096
	// 0xFFF = 4095 (12-bit max)
	uint16_t clipMin = limited ? 0x100 : 0x000;
	uint16_t clipMax = limited ? 0xEB0 : 0xFFF;

	// pass to HDMI, use 0xA0 to set a mode of [-2 .. 2] per ADV7513 programming guide
	uint8_t csc_data[] = {
		0x18, (uint8_t)(0b10100000 | ((csc_int16[0] >> 8) & 0b00011111)),  // csc Coefficients, Channel A
		0x19, (uint8_t)(csc_int16[0] & 0xff),
		0x1A, (uint8_t)(csc_int16[1] >> 8),
		0x1B, (uint8_t)(csc_int16[1] & 0xff),
		0x1C, (uint8_t)(csc_int16[2] >> 8),
		0x1D, (uint8_t)(csc_int16[2] & 0xff),
		0x1E, (uint8_t)(csc_int16[3] >> 8),
		0x1F, (uint8_t)(csc_int16[3] & 0xff),

		0x20, (uint8_t)(csc_int16[4] >> 8),  // csc Coefficients, Channel B
		0x21, (uint8_t)(csc_int16[4] & 0xff),
		0x22, (uint8_t)(csc_int16[5] >> 8),
		0x23, (uint8_t)(csc_int16[5] & 0xff),
		0x24, (uint8_t)(csc_int16[6] >> 8),
		0x25, (uint8_t)(csc_int16[6] & 0xff),
		0x26, (uint8_t)(csc_int16[7] >> 8),
		0x27, (uint8_t)(csc_int16[7] & 0xff),

		0x28, (uint8_t)(csc_int16[8] >> 8),  // csc Coefficients, Channel C
		0x29, (uint8_t)(csc_int16[8] & 0xff),
		0x2A, (uint8_t)(csc_int16[9] >> 8),
		0x2B, (uint8_t)(csc_int16[9] & 0xff),
		0x2C, (uint8_t)(csc_int16[10] >> 8),
		0x2D, (uint8_t)(csc_int16[10] & 0xff),
		0x2E, (uint8_t)(csc_int16[11] >> 8),
		0x2F, (uint8_t)(csc_int16[11] & 0xff),

		0xC0, (uint8_t)(clipMin >> 8), // HDMI limited clamps
		0xC1, (uint8_t)(clipMin & 0xff),
		0xC2, (uint8_t)(clipMax >> 8),
		0xC3, (uint8_t)(clipMax & 0xff)
	};

	write_regs(csc_data, sizeof(csc_data));
}

static void hdmi_config(int dvi_mode, int limited, int game_mode)
{
	// address, value
	uint8_t init_data[] = {
		0x98, 03,				// ADI required Write.

		0xD6, 0b11000000,		// [7:6] HPD Control...
								// 00 = HPD is from both HPD pin or CDC HPD
								// 01 = HPD is from CDC HPD
								// 10 = HPD is from HPD pin
								// 11 = HPD is always high

		0x41, 0x10,				// Power Down control
		0x9A, 0x70,				// ADI required Write.
		0x9C, 0x30,				// ADI required Write.
		0x9D, 0b01100001,		// [7:4] must be b0110!.
								// [3:2] b00 = Input clock not divided. b01 = Clk divided by 2. b10 = Clk divided by 4. b11 = invalid!
								// [1:0] must be b01!
		0xA2, 0xA4,				// ADI required Write.
		0xA3, 0xA4,				// ADI required Write.
		0xE0, 0xD0,				// ADI required Write.


		0x35, 0x40,
		0x36, 0xD9,
		0x37, 0x0A,
		0x38, 0x00,
		0x39, 0x2D,
		0x3A, 0x00,

		0x16, 0b00111000,		// Output Format 444 [7]=0.
								// [6] must be 0!
								// Colour Depth for Input Video data [5:4] b11 = 8-bit.
								// Input Style [3:2] b10 = Style 1 (ignored when using 444 input).
								// DDR Input Edge falling [1]=0 (not using DDR atm).
								// Output Colour Space RGB [0]=0.

		0x17, 0b00000010,		// Aspect ratio 16:9 [1]=1. Syncs NOT inverted
								// ([6:5]=00): SPG.sv outputs both syncs
								// active high, matching CEA 1080p polarity.

		0x3B, 0b01000000,		// Manual pixel repetition (none), manual VIC
		0x3C, 16,				// VIC 16 = 1920x1080p 60Hz (59.94 at 148.352MHz)

		0x48, 0b00001000,       // [6]=0 Normal bus order!
								// [5] DDR Alignment.
								// [4:3] b01 Data right justified (for YCbCr 422 input modes).

		0x49, 0xA8,				// ADI required Write.
		0x40, 0x00,
		0x4A, 0b10000000,		//Auto-Calculate SPD checksum
		0x4C, 0x00,				// ADI required Write.

		0x55, (uint8_t)(game_mode ? 0b00010010 : 0b00010000),
								// [7] must be 0!. Set RGB444 in AVinfo Frame [6:5], Set active format [4].
								// AVI InfoFrame Valid [4].
								// Bar Info [3:2] b00 Bars invalid. b01 Bars vertical. b10 Bars horizontal. b11 Bars both.
								// Scan Info [1:0] b00 (No data). b01 TV. b10 PC. b11 None.

		0x56, 0b00001000,		// [5:4] Picture Aspect Ratio
								// [3:0] Active Portion Aspect Ratio b1000 = Same as Picture Aspect Ratio

		0x57, (uint8_t)((game_mode ? 0x80 : 0x00)				// [7] IT Content. 0 - No. 1 - Yes (type set in register 0x59).
																// [6:4] Color space (ignored for RGB)
			| (limited ? 0b0100 : 0b0001000)),					// [3:2] RGB Quantization range
																// [1:0] Non-Uniform Scaled: 00 - None. 01 - Horiz. 10 - Vert. 11 - Both.

		0x59, (uint8_t)(game_mode ? 0x30 : 0x00),				// [7:6] [YQ1 YQ0] YCC Quantization Range: b00 = Limited Range, b01 = Full Range
																// [5:4] IT Content Type b11 = Game, b00 = Graphics/None
																// [3:0] Pixel Repetition Fields b0000 = No Repetition

		0x73, 0x01,

		0x96, 0xFF,             // clear all pending interrupts
		0x94, 0x00,             // no interrupts enabled (no one is listening)
		0xC9, 0x00,             // Clear EDID request

		0x99, 0x02,				// ADI required Write.
		0x9B, 0x18,				// ADI required Write.

		0x9F, 0x00,				// ADI required Write.

		0xA1, 0b00000000,	    // [6]=1 Monitor Sense Power Down DISabled.

		0xA4, 0x08,				// ADI required Write.
		0xA5, 0x04,				// ADI required Write.
		0xA6, 0x00,				// ADI required Write.
		0xA7, 0x00,				// ADI required Write.
		0xA8, 0x00,				// ADI required Write.
		0xA9, 0x00,				// ADI required Write.
		0xAA, 0x00,				// ADI required Write.
		0xAB, 0x40,				// ADI required Write.

		0xAF, (uint8_t)(0b00000100	// [7]=0 HDCP Disabled.
								// [6:5] must be b00!
								// [4]=0 Current frame is unencrypted
								// [3:2] must be b01!
			| (dvi_mode ? 0b00 : 0b10)),	 //	[1]=1 HDMI Mode.
								// [0] must be b0!

		0xB9, 0x00,				// ADI required Write.

		0xBA, 0b01100000,		// [7:5] Input Clock delay...
								// b000 = -1.2ns.
								// b001 = -0.8ns.
								// b010 = -0.4ns.
								// b011 = No delay.
								// b100 = 0.4ns.
								// b101 = 0.8ns.
								// b110 = 1.2ns.
								// b111 = 1.6ns.

		0xBB, 0x00,				// ADI required Write.
		0xDE, 0x9C,				// ADI required Write.
		0xE2, 0x01,				// Power down the CEC.
		0xE4, 0x60,				// ADI required Write.
		0xFA, 0x7D,				// Nbr of times to search for good phase

		// (Audio stuff on Programming Guide, Page 66)...
		0x0A, 0b00000000,		// [6:4] Audio Select. b000 = I2S.
								// [3:2] Audio Mode. (HBR stuff, leave at 00!).

		0x0B, 0b00001110,		//

		0x0C, 0b00000100,		// [7] 0 = Use sampling rate from I2S stream.   1 = Use samp rate from I2C Register.
								// [6] 0 = Use Channel Status bits from stream. 1 = Use Channel Status bits from I2C register.
								// [2] 1 = I2S0 Enable.
								// [1:0] I2S Format: 00 = Standard. 01 = Right Justified. 10 = Left Justified. 11 = AES.

		0x0D, 0b00010000,		// [4:0] I2S Bit (Word) Width for Right-Justified.
		0x14, 0b00000010,		// [3:0] Audio Word Length. b0010 = 16 bits.
		0x15, 0b0100000,		// I2S Sampling Rate [7:4]. b0000 = (44.1KHz). b0010 = 48KHz.
								// Input ID [3:1] b000 (0) = 24-bit RGB 444 or YCrCb 444 with Separate Syncs.

		// Audio Clock Config
		0x01, 0x00,				//
		0x02, 0x18,				// Set N Value 6144 (48KHz)
		0x03, 0x00,				//

		0x07, 0x01,				//
		0x08, 0x22,				// Set CTS Value 74250
		0x09, 0x0A,				//
	};

	write_regs(init_data, sizeof(init_data));

	hdmi_config_set_csc(limited);
}

int main(int argc, char *argv[])
{
	int dvi_mode = 0, limited = 0, game_mode = 0;

	for (int i = 1; i < argc; i++)
	{
		if      (!strcmp(argv[i], "--dvi"))     dvi_mode = 1;
		else if (!strcmp(argv[i], "--limited")) limited = 1;
		else if (!strcmp(argv[i], "--game"))    game_mode = 1;
		else
		{
			printf("Usage: %s [--dvi] [--limited] [--game]\n", argv[0]);
			printf("Configures the ADV7513 for 1920x1080 (VIC 16) RGB444 as produced by SPG.sv.\n");
			printf("  --dvi      DVI mode instead of HDMI (no infoframes/audio on the wire)\n");
			printf("  --limited  limited range RGB (16-235) output\n");
			printf("  --game     flag content as game mode (IT content) in the AVI InfoFrame\n");
			return 1;
		}
	}

	hdmi_main_fd = i2c_open(0x39, 0);
	if (hdmi_main_fd < 0)
	{
		printf("ADV7513 not found on i2c bus! HDMI won't be available!\n");
		printf("(is a bitstream routing the HPS I2C to the HDMI pins loaded?)\n");
		return -1;
	}

	hdmi_config(dvi_mode, limited, game_mode);
	i2c_close(hdmi_main_fd);

	printf("ADV7513 configured: 1080p (VIC 16), RGB444 %s range, %s%s.\n",
	       limited ? "limited" : "full",
	       dvi_mode ? "DVI" : "HDMI",
	       game_mode ? ", game mode" : "");
	return 0;
}

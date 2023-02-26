#include <stdio.h>
#include <cstring>

#include "ppu.h"
#include "cpu.h"
#include "interrupts.h"
#include "memory.h"

uint32_t ppu_cycle_count{};

uint8_t vram[8192]{};
uint8_t OAM[160]{};
uint8_t LCDC = 0x91;
uint8_t STAT = 0x81;
uint8_t SCY{};
uint8_t SCX{};
uint8_t LY = 0x91;
uint8_t LYC = 0x00;
uint8_t DMA = 0xFF;
uint8_t BGP = 0xFC;
uint8_t OBP0{};
uint8_t OBP1{};
uint8_t WY{};
uint8_t WX{};

// Each FIFO can hold 16 pixels (2 bits per pixel)
uint16_t dot_counter{};
uint8_t bg_FIFO[4]{};
uint8_t OAM_FIFO[4]{};

uint8_t frame_buffer[160 * 144 * 4]{};

void tick_ppu(uint8_t cycles)
{
	ppu_cycle_count += cycles / 2;

	if (ppu_cycle_count > PPU_FREQ)
		ppu_cycle_count %= PPU_FREQ;

	if (LY == LYC)
	{
		STAT |= LYC_COMPARE_FLAG;
		if (STAT & LYC_INTERRUPT_ENABLED) { interrupt_request(INTERRUPT_LCD_STAT); }
	}
	else { STAT &= ~LYC_COMPARE_FLAG; }

	step_ppu(cycles);
}

void step_ppu(uint8_t cycles)
{
	for (uint8_t i = 0; i < cycles; i += 4)
	{
		dot_counter += 4;

		if (LCDC & 0x80) // PPU is enabled
		{
			if (LY < 144)
			{
				if (dot_counter < 80) // OAM Scan
				{
					if (ppu_mode() != LCD_MODE_2)
					{
						STAT = (STAT & 0xFC) | LCD_MODE_2;
						if (STAT & OAM_INTERRUPT_ENABLED) { interrupt_request(INTERRUPT_LCD_STAT); }
					}
				}
				else if (dot_counter < 252) // Drawing
				{
					if (ppu_mode() != LCD_MODE_3)
					{
						STAT = (STAT & 0xFC) | LCD_MODE_3;

						memset(bg_FIFO, 0, sizeof(bg_FIFO));
						memset(OAM_FIFO, 0, sizeof(OAM_FIFO));
						draw_scanline();
					}
						
				}
				else if (dot_counter < 456) // HBLANK
				{
					if (ppu_mode() != LCD_MODE_0)
					{
						STAT = (STAT & 0xFC) | LCD_MODE_0;
						if (STAT & HBLANK_INTERRUPT_ENABLED) { interrupt_request(INTERRUPT_LCD_STAT); }
					}
				}
				else
				{
					dot_counter = 0;
					LY++;
				}
			}
			else if (LY < 153) // VBLANK
			{
				if (ppu_mode() != LCD_MODE_1)
				{
					STAT = (STAT & 0xFC) | LCD_MODE_1;
					if (STAT & VBLANK_INTERRUPT_ENABLED) { interrupt_request(INTERRUPT_VBLANK); }
				}

				if (dot_counter >= 456)
				{
					dot_counter %= 456;
					LY++;
				}
			}
			else // Frame finished drawing, reset scanline counter
			{
				LY = 0;
				dot_counter %= 456;
			}
		}
	}
}

uint8_t ppu_mode()
{
	return STAT & 0x03;
}

void draw_scanline()
{
	uint16_t tile_data{};
	uint16_t background_memory{};
	bool unsig = true;

	uint8_t win_x = WX - 7;

	bool using_window = false;

	if (LCDC & WINDOW_ENABLE) // Draw window
	{
		if (WY <= LY) // If current scanline is within window Y pos
			using_window = true;
	}

	if (LCDC & TILE_DATA_AREA)
	{
		tile_data = 0x8000;
	}
	else
	{
		tile_data = 0x8800;
		unsig = false;
	}

	if (using_window)
	{
		if (LCDC & WINDOW_DATA_AREA)
			background_memory = 0x9C00;
		else
			background_memory = 0x9800;
	}
	else
	{
		if (LCDC & BG_DATA_AREA)
			background_memory = 0x9C00;
		else
			background_memory = 0x9800;
	}

	uint8_t y_pos{};

	// y_pos is used to calculate which of 32 vertical tiles the current
	// scanline is drawing
	if (!using_window)
		y_pos = SCY + LY;
	else
		y_pos = LY - WY;

	// Which of the 8 vertical pixels of the current tile is the scanline on?
	uint16_t tile_row = (y_pos / 8) * 32;

	// Draw the 160 horizontal pixels for this scanline
	for (int pixel = 0; pixel < 160; pixel++)
	{
		uint8_t x_pos = pixel + SCX;

		// Translate current x position to window space if necessary
		if (using_window)
		{
			if (pixel >= win_x)
				x_pos = pixel - win_x;
		}

		// Which of the 32 horizontal lines does this x pos fall within?
		uint16_t tile_col = x_pos / 8;
		int16_t tile_num;

		// Get the tile identity number (can be signed/unsigned)
		uint16_t tile_address = background_memory + tile_row + tile_col;
		if (unsig)
			tile_num = (uint8_t)vram[tile_address - 0x8000];
		else
			tile_num = (int8_t)vram[tile_address - 0x8000];

		// Deduce where this tile identifier is in memory
		uint16_t tile_location = tile_data;
		if (unsig)
			tile_location += (tile_num * 16);
		else
			tile_location += ((tile_num + 128) * 16);

		// Find correct vertical line we're on of the tile to get tile data in memory
		uint8_t line = y_pos % 8;
		line *= 2; // Each line takes up 2 bytes of memory
		uint8_t data_1 = vram[tile_location + line - 0x8000];
		uint8_t data_2 = vram[tile_location + line + 1 - 0x8000];

		// pixel 0 : bit 7 of data_1 and data_2
		// pixel 1 : bit 6 etc.
		int color_bit = x_pos % 8;
		color_bit -= 7;
		color_bit *= -1;

		// Combine the two to get colour id for this pixel in the tile
		uint8_t color_mask = 1 << color_bit;
		uint8_t first_bit = (data_1 & color_mask) >> color_bit;
		uint8_t second_bit = (data_2 & color_mask) >> color_bit;
		uint8_t color_num = first_bit | (second_bit << 1);

		uint8_t color{};

		// Get actual color from palette as 2 bit value
		switch (color_num)
		{
			case 0x0:
				color = BGP & 0x03;
				break;

			case 0x1:
				color = (BGP & 0x0C) >> 2;
				break;

			case 0x2:
				color = (BGP & 0x30) >> 4;
				break;

			case 0x3:
				color = (BGP & 0xC0) >> 6;
				break;
		}
		
		uint8_t buffer_color{};

		// Get corresponding RGBA color from the 2 bit value
		switch (color)
		{
			case 0x0: // White
				buffer_color = 0xFF;
				break;

			case 0x1: // Light gray
				buffer_color = 0xAA;
				break;

			case 0x2: // Dark gray
				buffer_color = 0x55;
				break;

			case 0x3: // Black
				buffer_color = 0x00;
				break;
		}

		frame_buffer[LY * 160 * 4 + (pixel * 4)] = buffer_color;
		frame_buffer[LY * 160 * 4 + (pixel * 4) + 1] = buffer_color;
		frame_buffer[LY * 160 * 4 + (pixel * 4) + 2] = buffer_color;
		frame_buffer[LY * 160 * 4 + (pixel * 4) + 3] = 0xFF;
	}
}

uint8_t read_vram(uint16_t address)
{
	if ((STAT & 0x03) == 0x03) // VRAM inaccessible during mode 3
		return 0xFF;
	else
		return vram[address];
}

void write_vram(uint16_t address, uint8_t value)
{
	if ((STAT & 0x03) == 0x03) // VRAM inaccessible during mode 3, ignore writes
		return;
	else
		vram[address] = value;
}

uint8_t read_oam(uint16_t address)
{
	// OAM inaccessible during mode 2 and 3
	if (((STAT & 0x03) == 0x03) || ((STAT & 0x03) == 0x02))
		return 0xFF;
	else
		return OAM[address];
}

void write_oam(uint16_t address, uint8_t value)
{
	// OAM inaccessible during mode 2 and 3
	if (((STAT & 0x03) == 0x03) || ((STAT & 0x03) == 0x02))
		return;
	else
		OAM[address] = value;
}

uint8_t read_ppu(uint16_t address)
{
	if (address == 0xFF40)
		return LCDC;

	if (address == 0xFF41)
		return STAT;

	if (address == 0xFF42)
		return SCY;

	if (address == 0xFF43)
		return SCY;

	if (address == 0xFF44)
		return LY;

	if (address == 0xFF45)
		return LYC;

	if (address == 0xFF46)
		return DMA;

	if (address == 0xFF47)
		return BGP;

	if (address == 0xFF48)
		return OBP0;

	if (address == 0xFF49)
		return OBP1;

	if (address == 0xFF4A)
		return WY;

	if (address == 0xFF4B)
		return WX;
}

void write_ppu(uint16_t address, uint8_t value)
{
	if (address == 0xFF40)
		LCDC = value;

	if (address == 0xFF41) // Lower 2 bits are read only
		STAT = (value & 0xFC) | (STAT & 0x03);

	if (address == 0xFF42)
		SCY = value;

	if (address == 0xFF43)
		SCX = value;

	if (address == 0xFF44) // LY is read only
		return;

	if (address == 0xFF45)
		LYC = value;

	if (address == 0xFF46)
	{
		DMA = value;
		dma_transfer();
	}

	if (address == 0xFF47)
		BGP = value;

	if (address == 0xFF48)
		OBP0 = value;

	if (address == 0xFF49)
		OBP1 = value;

	if (address == 0xFF4A)
		WY = value;

	if (address == 0xFF4B)
		WX = value;
}
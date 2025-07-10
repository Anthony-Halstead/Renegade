// Gateware Research file. Will be used to test drawing text using GBlitter.
#pragma once

// Author: Lari Norri 1/10/2025
// This is a prototype class for drawing text using the GBlitter class.
// It is a pretty simple class that uses a font atlas and meta data to draw
// If you want to use a different font, you can generate your own font atlas at:
// https://evanw.github.io/font-texture-generator/ and download the image.
// Select "Data Format" to be "C" and copy the generated code to a header file.

// This software is in beta stage and is provided as-is with no warranty.
// The software is offered as Public Domain. A similar version may be incorporated
// into the Gateware API in the future, but will likely use an internal font format.
// You could extend this class to support multiple fonts, sizes, and styles.
// You could also support transformations and transparency in the DrawText functions.
class BLIT_Font
{
	// internal variables
	GW::GRAPHICS::GBlitter blitter;
	Font font = {};

	// GBlitter variables
	unsigned short font_atlas_id = -1;
	std::vector<GW::GRAPHICS::GBlitter::TileDefinition> letters;
	std::vector<unsigned int> tile_ids;

public:
	// Constructor
	BLIT_Font(GW::GRAPHICS::GBlitter _blitter, const char* _tgaFontAtlas, Font _font);
	// Destructor
	~BLIT_Font();
	// Draw text using the GBlitter class
	void DrawTextDeferred(int _x, int _y, const char* _text, std::size_t _len);
	void DrawTextImmediate(int _x, int _y, const char* _text, std::size_t _len);
};

// implementation

// Constructor
BLIT_Font::BLIT_Font(GW::GRAPHICS::GBlitter _blitter, const char* _tgaFontAtlas, Font _font)
{
	blitter = _blitter;
	font = _font;
	// Load the font atlas
	if (+blitter.LoadSource(_tgaFontAtlas, nullptr, nullptr, font_atlas_id)) {
		// Create the font tiles
		letters.resize(font.characterCount);
		for (int i = 0; i < font.characterCount; ++i) {
			letters[i] = { 
				static_cast<unsigned short>(font_atlas_id),
				static_cast<unsigned short>(font.characters[i].x), 
				static_cast<unsigned short>(font.characters[i].y),
				static_cast<unsigned short>(font.characters[i].width), 
				static_cast<unsigned short>(font.characters[i].height), 
				0xFF000000, 0, 0 
			};
		}
		// Define the font tiles
		tile_ids.resize(font.characterCount);
		if (-blitter.DefineTiles(letters.data(), font.characterCount, tile_ids.data())) {
			// error handling
			std::throw_with_nested(std::runtime_error("Failed to define font tiles"));
		}
	}
	else {
		// error handling
		std::throw_with_nested(std::runtime_error("Failed to load font atlas"));
	}
}

// Destructor
BLIT_Font::~BLIT_Font()
{
}

// Draw text using the GBlitter class
void BLIT_Font::DrawTextDeferred(int _x, int _y, const char* _text, std::size_t _len)
{
	//// Draw the text using the GBlitter class
	thread_local static std::vector<GW::GRAPHICS::GBlitter::DrawInstruction> draw_instructions;
	draw_instructions.resize(_len);
	// compute the starting position
	float start_x = static_cast<float>(_x);
	float start_y = static_cast<float>(_y);

	for (std::size_t i = 0; i < _len; ++i) {

		// skip spaces
		if (_text[i] == ' ') {
			start_x += static_cast<float>(font.size / 4);
			continue;
		}

		draw_instructions[i] = {
			tile_ids[_text[i] - 32],
			GW::GRAPHICS::GBlitter::DrawOptions::USE_MASKING,
			0, 0, start_x, start_y, 0, // rest unused
		};
		draw_instructions[i].t[0] -= static_cast<float>(font.characters[_text[i] - 32].originX);
		draw_instructions[i].t[1] -= static_cast<float>(font.characters[_text[i] - 32].originY);

		start_x += static_cast<float>(font.characters[_text[i] - 32].width);
		start_x -= static_cast<float>(font.characters[_text[i] - 32].originX);
	}

	// Draw the text using the GBlitter class
	if (-blitter.DrawDeferred(draw_instructions.data(), _len)) {
		// error handling
		std::throw_with_nested(std::runtime_error("Failed to draw text"));
	}
}

void BLIT_Font::DrawTextImmediate(int _x, int _y, const char* _text, std::size_t _len)
{
	// Draw the text using the GBlitter class
	thread_local static std::vector<GW::GRAPHICS::GBlitter::DrawInstruction> draw_instructions;
	draw_instructions.resize(_len);
	// compute the starting position
	float start_x = static_cast<float>(_x);
	float start_y = static_cast<float>(_y);

	for (std::size_t i = 0; i < _len; ++i) {

		// skip spaces
		if (_text[i] == ' ') {
			start_x += static_cast<float>(font.size / 4);
			continue;
		}

		draw_instructions[i] = {
			tile_ids[_text[i] - 32],
			GW::GRAPHICS::GBlitter::DrawOptions::USE_MASKING,
			0, 0, start_x, start_y, 0, // rest unused
		};
		draw_instructions[i].t[0] -= static_cast<float>(font.characters[_text[i] - 32].originX);
		draw_instructions[i].t[1] -= static_cast<float>(font.characters[_text[i] - 32].originY);

		start_x += static_cast<float>(font.characters[_text[i] - 32].width);
		start_x -= static_cast<float>(font.characters[_text[i] - 32].originX);
	}

	// Draw the text using the GBlitter class
	if (-blitter.DrawImmediate(draw_instructions.data(), _len)) {
		// error handling
		std::throw_with_nested(std::runtime_error("Failed to draw text"));
	}
}


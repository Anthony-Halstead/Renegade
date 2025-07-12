#ifndef UI_COMPONENTS_H_
#define UI_COMPONENTS_H_

class Overlay;
class BLIT_Font;

namespace UI
{
	struct UIManager 
	{
		GW::GRAPHICS::GBlitter* blitter;
		Overlay* overlay;
		BLIT_Font* font;
	};

	struct TitleScreen 
	{
		bool start = 1;
	};

	struct WinLoseScreen
	{
		bool restart = 1;
	};

	struct PauseScreen
	{
		bool pauseContinue = 1;
	};
}

#endif 
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
}

#endif 
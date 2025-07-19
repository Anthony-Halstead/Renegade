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

	struct SplashScreen
	{
		float splashTime;
		float fadeDuration;
		int splash = 0;
		int maxSplashScreens = 3;
		bool fadingIn = true;
		bool fadingOut = false;
		float splashDuration = 0.f;
		float fadeAlpha = 0.f;
	};

	struct Point2D
	{
		int x;
		int y;
	};

	struct Rect
	{
		int x;
		int y;
		unsigned width;
		unsigned height;
	};

}

#endif 
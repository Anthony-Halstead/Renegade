#include "../CCL.h"
#include "Font.h"
#include "BLIT_Font.h"
#include "../DRAW/DrawComponents.h"
#include "../DRAW/Utility/FileIntoString.h"
#include "../APP/Window.hpp"
#include "../GAME/GameComponents.h"
#include "../UTIL/Utilities.h"
#include "shaderc/shaderc.h"
#include "Overlay.h"
#include "UIComponents.h"
#include "UIAssets.h"

namespace UI
{
	// helper functions
	void DisplayScores(entt::registry& registry, UIManager& ui)
	{
		if (!registry.valid(registry.view<GAME::Score>().front())) return; // Ensure score entity exists
		auto& scoreComponent = registry.get<GAME::Score>(registry.view<GAME::Score>().front());
		std::string scoreText = "Score " + std::to_string(scoreComponent.score);
		ui.font->DrawTextImmediate(0, 24, scoreText.c_str(), scoreText.size());

		std::string highScoreText = "High Score " + std::to_string(scoreComponent.highScore);
		ui.font->DrawTextImmediate(0, 48, highScoreText.c_str(), highScoreText.size());
	}

	void DisplayPlayerHealth(entt::registry& registry, UIManager& ui, APP::Window& window)
	{
		if (!registry.valid(registry.view<GAME::Player>().front())) return; // Ensure player entity exists
		if (!registry.any_of<GAME::Health>(registry.view<GAME::Player>().front())) return; // Ensure player has health component
		auto& healthComponent = registry.get<GAME::Health>(registry.view<GAME::Player>().front());
		std::string healthText = "Lives: " + std::to_string(healthComponent.health);
		ui.font->DrawTextImmediate(0, window.height - 5, healthText.c_str(), healthText.size());
	}

	// Get gamepad state inputs
	void GetGamepadState(entt::registry& registry, float& startBtn, float& leftStickY, float& upBtn, float& downBtn, float& sBtn) {
		UTIL::Input input = registry.ctx().get<UTIL::Input>();
		if (input.connectedControllers > 0)
		{
			input.gamePads.GetState(0, G_START_BTN, startBtn);
			input.gamePads.GetState(0, G_LY_AXIS, leftStickY);
			input.gamePads.GetState(0, G_DPAD_DOWN_BTN, downBtn);
			input.gamePads.GetState(0, G_DPAD_UP_BTN, upBtn);
			input.gamePads.GetState(0, G_SOUTH_BTN, sBtn);
		}
		else
		{
			// No controller connected, set controller-specific inputs to zero
			startBtn = 0.0f;
			leftStickY = 0.0f;
			upBtn = 0.0f;
			downBtn = 0.0f;
			sBtn = 0.0f;
		}
	}

	int Convert2Dto1D(const int& x, const int& y, const unsigned& width) {
		return y * width + x;
	};

	unsigned BGRAtoARGB(const unsigned& color)
	{
		const unsigned alpha = (color & 0x000000FF) << 24;
		const unsigned red = (color & 0x0000FF00) << 8;
		const unsigned green = (color & 0x00FF0000) >> 8;
		const unsigned blue = (color & 0xFF000000) >> 24;

		return (alpha | red | green | blue);
	}

	unsigned LerpColor(unsigned source, unsigned destination, float alpha)
	{
		unsigned sA = (source >> 24) & 0xFF;
		unsigned sR = (source >> 16) & 0xFF;
		unsigned sG = (source >> 8) & 0xFF;
		unsigned sB = source & 0xFF;

		unsigned dA = (destination >> 24) & 0xFF;
		unsigned dR = (destination >> 16) & 0xFF;
		unsigned dG = (destination >> 8) & 0xFF;
		unsigned dB = destination & 0xFF;

		unsigned rA = (unsigned)(sA * (1 - alpha) + dA * alpha);
		unsigned rR = (unsigned)(sR * (1 - alpha) + dR * alpha);
		unsigned rG = (unsigned)(sG * (1 - alpha) + dG * alpha);
		unsigned rB = (unsigned)(sB * (1 - alpha) + dB * alpha);

		return (rA << 24) | (rR << 16) | (rG << 8) | rB;
	}

	static void BlockImageTransfer(const Rect& source, const Point2D& pos, const int& sourceWidth, const unsigned* sourceArray, unsigned* color_pix, unsigned width, unsigned height) {
		for (unsigned y = 0; y < source.height; ++y)
		{
			if (pos.y + y > height - 1)
				break;
			for (unsigned x = 0; x < source.width; ++x)
			{
				if (pos.x + x > width - 1)
					break;
				color_pix[Convert2Dto1D(pos.x + x, pos.y + y, width)] = BGRAtoARGB(sourceArray[Convert2Dto1D(source.x + x, source.y + y, sourceWidth)]);
			}
		}
	};

	void DisplaySplashScreen(entt::registry& registry, UIManager& ui, APP::Window& window, unsigned* color_pix)
	{
		auto& splashScreen = registry.get<UI::SplashScreen>(registry.view<UI::UIManager>().front());
		auto& deltaTime = registry.ctx().get<UTIL::DeltaTime>().dtSec;

		if (splashScreen.fadingIn)
		{
			splashScreen.fadeAlpha += deltaTime / splashScreen.fadeDuration;
			if (splashScreen.fadeAlpha >= 1.f)
			{
				splashScreen.fadeAlpha = 1.f;
				splashScreen.fadingIn = false;
			}
		}
		else if (splashScreen.fadingOut)
		{
			splashScreen.fadeAlpha -= deltaTime / splashScreen.fadeDuration;
			if (splashScreen.fadeAlpha <= 0.f)
			{
				splashScreen.fadeAlpha = 0.f;
				splashScreen.fadingOut = false;

				++splashScreen.splash;

				if (splashScreen.splash >= splashScreen.maxSplashScreens)
				{
					for (int i = 0; i < window.width * window.height; ++i) color_pix[i] = 0xFF000000;
					registry.remove<UI::SplashScreen>(registry.view<UI::UIManager>().front());
					return;
				}
				splashScreen.fadingIn = true;
			}
		}
		else
		{
			splashScreen.splashDuration += deltaTime;

			if (splashScreen.splashDuration >= splashScreen.splashTime)
			{
				splashScreen.splashDuration = 0.f;
				splashScreen.fadingOut = true;
			}
		}

		const unsigned* img = nullptr;

		switch (splashScreen.splash)
		{
		case 0:
			img = EssentialGreensLogo_pixels;
			break;
		case 1:
			img = GatewareLogo_pixels;
			break;
		case 2:
			img = Vulkan_logo_pixels;
			break;
		}

		if (!img)
		{
			registry.remove<UI::SplashScreen>(registry.view<UI::UIManager>().front());
			return;
		}

		for (int i = 0; i < window.width * window.height; ++i)
		{
			unsigned splash = BGRAtoARGB(img[i]);
			unsigned black = 0xFF000000;

			color_pix[i] = LerpColor(black, splash, splashScreen.fadeAlpha);
		}
	}

	void DisplayTitleScreen(entt::registry& registry, UIManager& ui, APP::Window& window, unsigned* color_pix)
	{
		// TODO: make this more versatile; maybe needs to be refactored later
		if (registry.any_of<UI::TitleScreen>(registry.view<UI::UIManager>().front()))
		{
			float startBtn, leftStickY, upBtn, downBtn, sBtn;
			GetGamepadState(registry, startBtn, leftStickY, upBtn, downBtn, sBtn);

			auto& title = registry.get<UI::TitleScreen>(registry.view<UI::UIManager>().front());

			if (GetAsyncKeyState(0x26) & 0x8001 || leftStickY > 0 || upBtn)
				title.start = 1;
			else if (GetAsyncKeyState(0x28) & 0x8001 || leftStickY < 0 || downBtn)
				title.start = 0;

			if (GetAsyncKeyState(VK_RETURN) & 0x01 || sBtn > 0)
			{
				if (title.start)
					registry.remove<UI::TitleScreen>(registry.view<UI::UIManager>().front());
				else
					registry.emplace<APP::WindowClosed>(registry.view<APP::Window>().front());
			}

			const unsigned* img = title.start ? RENEGADE_TitleStart_pixels : RENEGADE_TitleQuit_pixels;

			for (int i = 0; i < window.width * window.height; ++i)
				color_pix[i] = BGRAtoARGB(img[i]);
		}
	}

	void DisplayWinLoseScreen(entt::registry& registry, UIManager& ui, APP::Window& window, unsigned* color_pix)
	{
		if (registry.any_of<UI::WinLoseScreen>(registry.view<UI::UIManager>().front()))
		{
			auto& winLoseScreen = registry.get<UI::WinLoseScreen>(registry.view<UI::UIManager>().front());

			float startBtn, leftStickY, upBtn, downBtn, sBtn;
			GetGamepadState(registry, startBtn, leftStickY, upBtn, downBtn, sBtn);


			if (GetAsyncKeyState(0x26) & 0x8001 || leftStickY > 0 || upBtn)
				winLoseScreen.restart = 1;
			else if (GetAsyncKeyState(0x28) & 0x8001 || leftStickY < 0 || downBtn)
				winLoseScreen.restart = 0;

			if (GetAsyncKeyState(VK_RETURN) & 0x01 || sBtn > 0)
			{
				if (winLoseScreen.restart)
				{
					UTIL::ClearScene(registry);
					UTIL::RestartScene(registry);

					registry.remove<UI::WinLoseScreen>(registry.view<UI::UIManager>().front());
				}
				else
				{
					registry.emplace_or_replace<UI::TitleScreen>(registry.view<UI::UIManager>().front());
					registry.remove<UI::WinLoseScreen>(registry.view<UI::UIManager>().front());
				}
			}

			const unsigned* img;

			// win/lose check
			if (registry.any_of<GAME::Health>(registry.view<GAME::Player>().front()))
				img = registry.get<GAME::Health>(registry.view<GAME::Player>().front()).health <= 0 ? winLoseScreen.restart ? RENEGADE_LoseRestart_pixels : RENEGADE_LoseQuit_pixels : winLoseScreen.restart ? RENEGADE_WinRestart_pixels : RENEGADE_WinQuit_pixels;

			for (int i = 0; i < window.width * window.height; ++i)
				color_pix[i] = BGRAtoARGB(img[i]);
		}
	}

	void DisplayPauseScreen(entt::registry& registry, UIManager& ui, APP::Window& window, unsigned* color_pix)
	{
		if (!registry.any_of<UI::WinLoseScreen>(registry.view<UI::UIManager>().front()) && !registry.any_of<UI::TitleScreen>(registry.view<UI::UIManager>().front()))
		{
			float startBtn, leftStickY, upBtn, downBtn, sBtn;
			GetGamepadState(registry, startBtn, leftStickY, upBtn, downBtn, sBtn);

			if (!registry.any_of<UI::PauseScreen>(registry.view<UI::UIManager>().front()))
			{
				if (GetAsyncKeyState(VK_ESCAPE) & 0x8000 || startBtn)
					registry.emplace_or_replace<UI::PauseScreen>(registry.view<UI::UIManager>().front());
				return;
			}
			auto& pauseScreen = registry.get<UI::PauseScreen>(registry.view<UI::UIManager>().front());

			if (GetAsyncKeyState(0x26) & 0x8001 || leftStickY > 0 || upBtn)
				pauseScreen.pauseContinue = 1;
			else if (GetAsyncKeyState(0x28) & 0x8001 || leftStickY < 0 || downBtn)
				pauseScreen.pauseContinue = 0;

			if (GetAsyncKeyState(VK_RETURN) & 0x01 || sBtn)
			{
				if (!pauseScreen.pauseContinue)
				{
					UTIL::ClearScene(registry);
					UTIL::RestartScene(registry);
					registry.emplace<UI::TitleScreen>(registry.view<UI::UIManager>().front());
				}
				registry.remove<UI::PauseScreen>(registry.view<UI::UIManager>().front());
				return;
			}

			const unsigned* img = pauseScreen.pauseContinue ? RENEGADE_PauseContinue_pixels : RENEGADE_PauseQuit_pixels;

			Rect r
			{
				259, 228,
				505, 306,
			};

			Point2D p
			{
				window.width / 2 - r.width / 2,
				window.height / 2 - r.height / 2,
			};

			BlockImageTransfer(r, p, RENEGADE_PauseContinue_width, img, color_pix, window.width, window.height);

			//for (int i = 0; i < window.width * window.height; ++i)
			//	color_pix[i] = BGRAtoARGB(img[i]);
		}
	}
	void Construct_UI(entt::registry& registry, entt::entity entity)
	{
		auto& win = registry.get<GW::SYSTEM::GWindow>(entity);
		auto& window = registry.get<APP::Window>(entity);
		auto& ui = registry.get<UIManager>(entity);

		ui.overlay = new Overlay(window.width, window.height, win, registry.get<DRAW::VulkanRenderer>(entity).vlkSurface, 0);
		ui.blitter = new GW::GRAPHICS::GBlitter();
		ui.blitter->Create(window.width, window.height);
		ui.font = new BLIT_Font(*ui.blitter, "../font.tga", font_Arial);
	}

	void Update_UI(entt::registry& registry, entt::entity entity)
	{
		auto& window = registry.get<APP::Window>(entity);
		auto& ui = registry.get<UIManager>(entity);

		ui.blitter->ClearColor(0x0);

		DisplayScores(registry, ui);
		DisplayPlayerHealth(registry, ui, window);

		unsigned int* color_pix;

		ui.overlay->LockForUpdate(window.width * window.height, &color_pix);
		ui.blitter->ExportResult(false, window.width, window.height, 0, 0, color_pix, nullptr, nullptr);

		if (!registry.any_of<UI::SplashScreen>(registry.view<UI::UIManager>().front()))
		{
			DisplayTitleScreen(registry, ui, window, color_pix);
			DisplayWinLoseScreen(registry, ui, window, color_pix);
			DisplayPauseScreen(registry, ui, window, color_pix);
		}
		else
			DisplaySplashScreen(registry, ui, window, color_pix);

		ui.overlay->Unlock();
		ui.overlay->TransferOverlay();
		ui.overlay->RenderOverlay();
	}

	void Destroy_UI(entt::registry& registry, entt::entity entity)
	{
		auto& ui = registry.get<UIManager>(entity);

		delete ui.blitter;
		delete ui.font;
		delete ui.overlay;
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<UIManager>().connect<Construct_UI>();
		registry.on_update<UIManager>().connect<Update_UI>();
		registry.on_destroy<UIManager>().connect<Destroy_UI>();
	}
}
#include "../CCL.h"
#include "Font.h"
#include "BLIT_Font.h"
#include "../DRAW/DrawComponents.h"
#include "../APP/Window.hpp"
#include "../GAME/GameComponents.h"
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif
#include "Overlay.h"
#include "UIComponents.h"
#include "img/RENEGADE_TitleStart.h"
#include "img/RENEGADE_TitleQuit.h"
#include "img/RENEGADE_LoseRestart.h"
#include "img/RENEGADE_LoseQuit.h"
#include "img/RENEGADE_WinRestart.h"
#include "img/RENEGADE_WinQuit.h"
#include "img/RENEGADE_PauseContinue.h"
#include "img/RENEGADE_PauseQuit.h"

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

	unsigned BGRAtoARGB(const unsigned& color) {
		const unsigned alpha = (color & 0x000000FF) << 24;
		const unsigned red = (color & 0x0000FF00) << 8;
		const unsigned green = (color & 0x00FF0000) >> 8;
		const unsigned blue = (color & 0xFF000000) >> 24;

		return (alpha | red | green | blue);
	}

	void DisplayTitleScreen(entt::registry& registry, UIManager& ui, APP::Window& window, unsigned int* color_pix)
	{
		// TODO: make this more versatile; definitely needs to be refactored later
		if (registry.any_of<UI::TitleScreen>(registry.view<UI::UIManager>().front()))
		{
			auto& title = registry.get<UI::TitleScreen>(registry.view<UI::UIManager>().front());

			if (GetAsyncKeyState(0x26) & 0x01)
				title.start = 1;
			else if (GetAsyncKeyState(0x28) & 0x01)
				title.start = 0;
			if (GetAsyncKeyState(VK_RETURN) & 0x01)
			{
				if (title.start)
					registry.remove<UI::TitleScreen>(registry.view<UI::UIManager>().front());
				else
					registry.emplace<APP::WindowClosed>(registry.view<APP::Window>().front());
			}

			if (title.start)
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_TitleStart_pixels[i]);
			else
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_TitleQuit_pixels[i]);
		}
	}

	void DisplayWinLoseScreen_Helper(UI::WinLoseScreen& winLoseScreen, APP::Window& window, unsigned int* color_pix, bool W)
	{
		if (W)
		{
			if (winLoseScreen.restart)
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_WinRestart_pixels[i]);
			else
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_WinQuit_pixels[i]);
		}
		else
		{
			if (winLoseScreen.restart)
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_LoseRestart_pixels[i]);
			else
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_LoseQuit_pixels[i]);
		}
	}

	void DisplayWinLoseScreen(entt::registry& registry, UIManager& ui, APP::Window& window, unsigned int* color_pix)
	{
		if (registry.any_of<UI::WinLoseScreen>(registry.view<UI::UIManager>().front()))
		{
			auto& winLoseScreen = registry.get<UI::WinLoseScreen>(registry.view<UI::UIManager>().front());

			if (GetAsyncKeyState(0x26) & 0x01)
				winLoseScreen.restart = 1;
			else if (GetAsyncKeyState(0x28) & 0x01)
				winLoseScreen.restart = 0;
			if (GetAsyncKeyState(VK_RETURN) & 0x01)
			{
				// TODO: write restart logic
				if (winLoseScreen.restart)
				{
					registry.emplace_or_replace<UI::TitleScreen>(registry.view<UI::UIManager>().front());
					registry.remove<UI::WinLoseScreen>(registry.view<UI::UIManager>().front());
				}
				else
				{
					registry.emplace_or_replace<UI::TitleScreen>(registry.view<UI::UIManager>().front());
					registry.remove<UI::WinLoseScreen>(registry.view<UI::UIManager>().front());
				}
			}

			// loss
			if (!registry.valid(registry.view<GAME::Player>().front()))
			{
				DisplayWinLoseScreen_Helper(winLoseScreen, window, color_pix, false);
			}
			else
			{
				if (registry.any_of<GAME::Health>(registry.view<GAME::Player>().front()))
				{
					// loss
					if (registry.get<GAME::Health>(registry.view<GAME::Player>().front()).health <= 0)
						DisplayWinLoseScreen_Helper(winLoseScreen, window, color_pix, false);
					else // win
						DisplayWinLoseScreen_Helper(winLoseScreen, window, color_pix, true);
				}
				else
				{
					// if player has no health component, assume they lost
					DisplayWinLoseScreen_Helper(winLoseScreen, window, color_pix, false);
				}
			}
		}
	}
	// TODO: fix bug with pressing esc multiple times: adds multiplee pause screens(?)
	void DisplayPauseScreen(entt::registry& registry, UIManager& ui, APP::Window& window, unsigned int* color_pix)
	{
		if (!registry.any_of<UI::WinLoseScreen>(registry.view<UI::UIManager>().front()) && !registry.any_of<UI::TitleScreen>(registry.view<UI::UIManager>().front()))
		{
			if (!registry.any_of<UI::PauseScreen>(registry.view<UI::UIManager>().front()))
			{
				if (GetAsyncKeyState(VK_ESCAPE) & 0x01)
					registry.emplace_or_replace<UI::PauseScreen>(registry.view<UI::UIManager>().front());
				return;
			}
			auto& pauseScreen = registry.get<UI::PauseScreen>(registry.view<UI::UIManager>().front());

			if (GetAsyncKeyState(0x26) & 0x01)
				pauseScreen.pauseContinue = 1;
			else if (GetAsyncKeyState(0x28) & 0x01)
				pauseScreen.pauseContinue = 0;
			if (GetAsyncKeyState(VK_RETURN) & 0x01)
			{
				if (pauseScreen.pauseContinue)
					registry.remove<UI::PauseScreen>(registry.view<UI::UIManager>().front());
				else
				{
					registry.emplace_or_replace<UI::TitleScreen>(registry.view<UI::UIManager>().front());
					registry.remove<UI::PauseScreen>(registry.view<UI::UIManager>().front());
				}
			}

			if (pauseScreen.pauseContinue)
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_PauseContinue_pixels[i]);
			else
				for (int i = 0; i < window.width * window.height; ++i)
					color_pix[i] = BGRAtoARGB(RENEGADE_PauseQuit_pixels[i]);
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

		DisplayTitleScreen(registry, ui, window, color_pix);
		DisplayWinLoseScreen(registry, ui, window, color_pix);
		DisplayPauseScreen(registry, ui, window, color_pix);

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
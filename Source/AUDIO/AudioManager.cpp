#include "../CCL.h"
#include "AudioComponents.h"
#include "../UTIL/Utilities.h"
#include "AudioSystem.h"
#include "../GAME/GameComponents.h"
namespace AUDIO
{

	void Construct_AudioDevice(entt::registry& reg, entt::entity ent)
	{
		auto& device = reg.get<AudioDevice>(ent);
		if (-device.engine.Create()) {
			std::cerr << "Failed to init Gateware audio\n";
			std::abort();
		}

		GW::CORE::GEventResponder shutdown;
		shutdown.Create([&](const GW::GEvent& e) {
			GW::AUDIO::GAudio::Events ev;
			if (+e.Read(ev) && ev == GW::AUDIO::GAudio::Events::DESTROY)
				reg.clear<AudioDevice>();
			});
		device.engine.Register(shutdown);
		reg.emplace<GW::CORE::GEventResponder>(ent, shutdown.Relinquish());

		const auto& utilConfig = reg.ctx().get<UTIL::Config>();
		if (!utilConfig.gameConfig) {
			std::cerr << "[AUDIO] utilConfig.gameConfig is null!\n";
			return;
		}
		std::shared_ptr<const GameConfig> config = utilConfig.gameConfig;

		// --------------------
		// Music Initialization
		// --------------------
		auto& musicHandle = reg.emplace<MusicHandle>(ent);
		const std::vector<std::string> trackKeys = { "boss", "menu", "intro", "win", "lose","bossOne","bossTwo","bossThree" };

		float musicVolume = (*config).at("AudioSettings").at("musicVolume").as<float>();
		float minRadius = (*config).at("AudioSettings").at("attenuationMinRadius").as<float>();
		float maxRadius = (*config).at("AudioSettings").at("attenuationMaxRadius").as<float>();
		GW::AUDIO::GATTENUATION attenuation = GW::AUDIO::GATTENUATION::LINEAR;

		for (const auto& key : trackKeys) {
			try {
				std::string filePath = (*config).at("Music").at(key).as<std::string>();
				GW::AUDIO::GMusic3D music;
				if (-music.Create(filePath.c_str(), minRadius, maxRadius, attenuation, device.engine, musicVolume)) {
					std::cerr << "[AUDIO] Failed to load music: " << filePath << "\n";
					continue;
				}
				music.UpdatePosition(GW::MATH::GIdentityVectorF);
				musicHandle.tracks.emplace(key, std::move(music));
			}
			catch (...) {
				std::cerr << "[AUDIO] Missing or invalid music track key: '" << key << "'\n";
			}
		}

		AudioSystem::SetPlaybackMode(PlaybackMode::MANUAL);

		const std::string defaultKey = "intro";
		auto found = musicHandle.tracks.find(defaultKey);
		if (found != musicHandle.tracks.end()) {
			if (-found->second.Play(true)) {
				std::cerr << "[AUDIO] Failed to play default track: " << defaultKey << "\n";
			}
			else {
				musicHandle.currentTrack = defaultKey;
			}
		}
		else {
			std::cerr << "[AUDIO] Default track key '" << defaultKey << "' not found!\n";
		}

		// ---------------------
		// SFX Initialization
		// ---------------------
		SoundLibrary sfxLibrary;

		const std::vector<std::string> sfx2DKeys = { "menuClick", "pickup", "error", "confirm", "shoot" };
		for (const auto& key : sfx2DKeys) {
			try {
				std::string path = (*config).at("SFX").at(key).as<std::string>();
				GW::AUDIO::GSound3D sound;
				if (-sound.Create(path.c_str(), minRadius, maxRadius, GW::AUDIO::GATTENUATION::LINEAR, device.engine)) {
					std::cerr << "[AUDIO] Failed to load 2D SFX: " << path << "\n";
					continue;
				}
				sound.UpdatePosition(GW::MATH::GIdentityVectorF);
				sfxLibrary.sfx.emplace(key, std::move(sound));
			}
			catch (...) {
				std::cerr << "[AUDIO] Missing or invalid 2D SFX key: '" << key << "'\n";
			}
		}

		reg.emplace<SoundLibrary>(ent, std::move(sfxLibrary));
	}
	void Update_MusicHandle(entt::registry& reg, entt::entity ent) {
		auto& handle = reg.get<AUDIO::MusicHandle>(ent);

		if (handle.currentTrack.empty()) return;

		auto it = handle.tracks.find(handle.currentTrack);
		if (it == handle.tracks.end()) return;

		bool isPlaying = false;
		if (-it->second.isPlaying(isPlaying) || isPlaying) return;

		switch (handle.mode) {
		case AUDIO::PlaybackMode::SEQUENTIAL: {
			std::vector<std::string> keys;
			for (const auto& [k, _] : handle.tracks)
				keys.push_back(k);

			if (keys.empty()) return;

			handle.lastPlayedIndex = (handle.lastPlayedIndex + 1) % keys.size();
			AudioSystem::PlayMusicTrack(keys[handle.lastPlayedIndex]);
			break;
		}
		case AUDIO::PlaybackMode::RANDOM: {
			std::vector<std::string> keys;
			for (const auto& [k, _] : handle.tracks)
				keys.push_back(k);

			if (keys.empty()) return;

			size_t idx = rand() % keys.size();
			handle.lastPlayedIndex = idx;
			AudioSystem::PlayMusicTrack(keys[idx]);
			break;
		}
		case AUDIO::PlaybackMode::MANUAL:
		default:
			break;
		}
	}

	void Update(entt::registry& reg, entt::entity ent)
	{
		Update_MusicHandle(reg, ent);
	}


	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<AudioDevice>().connect<Construct_AudioDevice>();
		registry.on_update<MusicHandle>().connect<Update>();
	}
}
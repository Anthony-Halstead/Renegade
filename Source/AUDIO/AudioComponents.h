#ifndef AUDIO_COMPONENTS_H_
#define AUDIO_COMPONENTS_H_


namespace AUDIO
{

	enum class PlaybackMode {
		NONE,
		SEQUENTIAL,
		RANDOM,
		MANUAL
	};

	struct AudioDevice { GW::AUDIO::GAudio3D engine; };
	struct MusicHandle {
		std::unordered_map<std::string, GW::AUDIO::GMusic3D> tracks;
		std::string currentTrack;
		PlaybackMode mode = PlaybackMode::NONE;
		size_t lastPlayedIndex = 0;
		float volume = 1.0f;
	};

	struct SoundLibrary {
		std::unordered_map<std::string, GW::AUDIO::GSound3D> sfx;
	};


}
#endif 
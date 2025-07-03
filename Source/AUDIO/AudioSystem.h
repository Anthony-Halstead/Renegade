#include <string>
#include "AudioComponents.h"
#include <iostream>
#include "../../entt-3.13.1/single_include/entt/entt.hpp"

#ifndef AUDIO_SYSTEM_H_
#define AUDIO_SYSTEM_H_

namespace AUDIO {

	class AudioSystem {
	public:


		static void Initialize(entt::registry* reg, entt::entity audioEnt) {
			registry = reg;
			entity = audioEnt;
		}

		static bool PlayMusicTrack(const std::string& key) {
			if (!registry || !registry->valid(entity)) return false;
			auto& handle = registry->get<MusicHandle>(entity);

			if (!handle.currentTrack.empty()) {
				auto it = handle.tracks.find(handle.currentTrack);
				if (it != handle.tracks.end()) {
					it->second.Stop();
				}
			}

			auto it = handle.tracks.find(key);
			if (it == handle.tracks.end()) {
				std::cerr << "[AUDIO] Track not found: " << key << "\n";
				return false;
			}

			if (-it->second.Play(false)) {
				std::cerr << "[AUDIO] Failed to play track: " << key << "\n";
				return false;
			}

			handle.currentTrack = key;
			return true;
		}

		static void PlaySFX(const std::string& key, GW::MATH::GVECTORF pos = GW::MATH::GIdentityVectorF, float minRadius = 1.0f, float maxRadius = 1000.0f) {
			if (!registry || !registry->valid(entity)) return;

			auto& library = registry->get<SoundLibrary>(entity);
			auto it = library.sfx.find(key);
			if (it == library.sfx.end()) return;
			it->second.UpdatePosition(pos);
			GW::AUDIO::GATTENUATION attenuation = GW::AUDIO::GATTENUATION::LINEAR;
			it->second.UpdateAttenuation(minRadius, maxRadius, attenuation);
			it->second.Play();
		}

		static void SetPlaybackMode(PlaybackMode mode) {
			if (!registry || !registry->valid(entity)) return;
			registry->patch<MusicHandle>(entity, [mode](auto& h) {
				h.mode = mode;
				});
		}

		static std::string GetCurrentTrack() {
			if (!registry || !registry->valid(entity)) return {};
			return registry->get<MusicHandle>(entity).currentTrack;
		}

		static void SetMasterVolume(float volume) {
			if (!registry || !registry->valid(entity)) return;
			auto& audio = registry->get<AudioDevice>(entity).engine;
			audio.SetMasterVolume(volume);
		}

		static void StopAllSounds() {
			if (!registry || !registry->valid(entity)) return;
			registry->get<AudioDevice>(entity).engine.StopSounds();
		}

		static void PauseAllMusic() {
			if (!registry || !registry->valid(entity)) return;
			registry->get<AudioDevice>(entity).engine.PauseMusic();
		}

		static void UpdateListener(GW::MATH::GVECTORF position, GW::MATH::GQUATERNIONF orientation) {
			if (!registry || !registry->valid(entity)) return;
			registry->get<AudioDevice>(entity).engine.Update3DListener(position, orientation);
		}
	private:
		static inline entt::registry* registry = nullptr;
		static inline entt::entity entity = entt::null;
	};

}
#endif 
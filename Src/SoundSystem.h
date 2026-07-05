#pragma once

#include "Base.h"
#include "fmod.hpp"
#include <string>

struct Sound {
        FMOD::Sound* sound;
};

struct SoundSystem {
        void Init();
        void Shutdown();
        void Update();

        Sound LoadSound(const std::string& path, bool looping = false);
        void  PlaySound(const Sound& sound);
        void  PlayMusic(const Sound& sound);

        FMOD::System* system;

        FMOD::ChannelGroup* music_channel;
        FMOD::ChannelGroup* ui_channel;
};
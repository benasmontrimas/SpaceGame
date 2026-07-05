#include "SoundSystem.h"

#include <print>

void SoundSystem::Init() {
        FMOD_RESULT res;

        res = FMOD::System_Create(&system);
        if (res != FMOD_OK) {
                std::println("Failed creating sound system: {}", (u32)res);
                exit(2);
        }

        res = system->init(512, FMOD_INIT_NORMAL, 0);
        if (res != FMOD_OK) {
                std::println("Failed initialising sound system: {}", (u32)res);
                exit(2);
        }

        // ===== Create Channel Groups ===== //

        system->createChannelGroup("Music", &music_channel);
        music_channel->setMode(FMOD_LOOP_NORMAL | FMOD_2D);

        system->createChannelGroup("UI", &music_channel);
}

void SoundSystem::Shutdown() {
        system->release();
}

void SoundSystem::Update() {
        system->update();
}

Sound SoundSystem::LoadSound(const std::string& path, bool looping) {
        FMOD::Sound* sound;
        FMOD_MODE    mode = FMOD_CREATESAMPLE | FMOD_2D;
        if (looping) mode |= FMOD_LOOP_NORMAL;
        else mode |= FMOD_LOOP_OFF;
        system->createSound(path.c_str(), mode, {}, &sound);

        return Sound{ sound };
}

void SoundSystem::PlaySound(const Sound& sound) {
        system->playSound(sound.sound, ui_channel, false, nullptr);
}

void SoundSystem::PlayMusic(const Sound& sound) {
        system->playSound(sound.sound, music_channel, false, nullptr);
}
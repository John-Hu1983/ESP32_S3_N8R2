#pragma once

#ifndef TEST_SPEAKER
#define TEST_SPEAKER 0
#endif

class AudioService;

#if TEST_SPEAKER
void StartSpeakerSelfTestTask(AudioService* audio_service);
#else
inline void StartSpeakerSelfTestTask(AudioService*) {}
#endif

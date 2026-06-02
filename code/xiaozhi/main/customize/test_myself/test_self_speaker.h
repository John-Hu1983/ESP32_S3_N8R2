#pragma once

#define TEST_SPEAKER 1

class AudioService;

#if TEST_SPEAKER
void StartSpeakerSelfTestTask(AudioService* audio_service);
#else
inline void StartSpeakerSelfTestTask(AudioService*) {}
#endif

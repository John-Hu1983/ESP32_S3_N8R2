#pragma once

#define TEST_MIC 1

#if TEST_MIC
void StartMicSelfTestTask();
#else
inline void StartMicSelfTestTask() {}
#endif

#pragma once

#define TEST_MIC 0

#if TEST_MIC
void StartMicSelfTestTask();
#else
inline void StartMicSelfTestTask() {}
#endif

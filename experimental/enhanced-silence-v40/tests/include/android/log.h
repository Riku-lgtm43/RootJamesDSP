#pragma once

enum { ANDROID_LOG_INFO = 4, ANDROID_LOG_ERROR = 6 };
int __android_log_print(int priority, const char *tag, const char *format, ...);

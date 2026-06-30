#pragma once

typealias u32 = uint;
typealias i32 = int;

enum LogLevel {
        Info = 0,
        Warning = 1,
        Error = 2,
}

void Log<each T>(NativeString format, const T args) {
        printf(format, args);
}

#define LogInfo(format, ...)  Log("[SHADER] [INFO] " format "\n", __VA_ARGS__)
#define LogWarn(format, ...)  Log("[SHADER] [WARN] " format "\n", __VA_ARGS__)
#define LogError(format, ...) Log("[SHADER] [ERRO] " format "\n", __VA_ARGS__)
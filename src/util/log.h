// UniversalVisuals — tiny file logger (useful when the overlay is loaded by a game
// and there is no console). Log lives in %TEMP%\UniversalVisuals.log.
#pragma once

namespace uv {
void Log(const char* fmt, ...);
}

#pragma once

#include <string>
#include <string_view>

namespace dpsf {

// Convertit un chemin ANSI (page de code du système, comme ceux passés à CreateFileA par le jeu).
std::wstring AnsiToWide(const char* text);

// Le journal est écrit en UTF-8.
std::string WideToUtf8(std::wstring_view text);

bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept;

// Horodatage triable, utilisable dans un nom de fichier : 20261003-140501-123.
std::wstring FileTimestamp();

}  // namespace dpsf

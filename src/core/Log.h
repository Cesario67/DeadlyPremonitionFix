#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace dpsf::log {

enum class Level { Info, Warn, Error };

// Ouvre un nouveau fichier de journal pour la session et supprime les plus anciens au-delà de keepCount.
bool Open(const std::wstring& logsDir, int keepCount);
void Close() noexcept;

// Écriture synchrone d'une ligne horodatée. Ne lance jamais d'exception.
void Write(Level level, std::string_view message) noexcept;

// Variante pour le chemin de plantage : n'attend pas indéfiniment le verrou, au cas où le thread
// fautif le détiendrait.
void WriteEmergency(Level level, std::string_view message) noexcept;

template <typename... Args>
void Info(std::format_string<Args...> format, Args&&... args) noexcept {
    try {
        Write(Level::Info, std::format(format, std::forward<Args>(args)...));
    } catch (...) {
    }
}

template <typename... Args>
void Warn(std::format_string<Args...> format, Args&&... args) noexcept {
    try {
        Write(Level::Warn, std::format(format, std::forward<Args>(args)...));
    } catch (...) {
    }
}

template <typename... Args>
void Error(std::format_string<Args...> format, Args&&... args) noexcept {
    try {
        Write(Level::Error, std::format(format, std::forward<Args>(args)...));
    } catch (...) {
    }
}

template <typename... Args>
void Crash(std::format_string<Args...> format, Args&&... args) noexcept {
    try {
        WriteEmergency(Level::Error, std::format(format, std::forward<Args>(args)...));
    } catch (...) {
    }
}

}  // namespace dpsf::log

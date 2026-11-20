#pragma once
#include <cctype>
#include <string>

namespace nova
{
// Dateiname -> MIDI-Note. Cubase-/Yamaha-Konvention: C3 = 60 (Mittel-C), C-2 = 0, C8 = 120.
// Akzeptiert: "C3", "C#3", "Db3", "F#-1", "60", sowie Anhaenge nach '_' oder ' ' ("C3_vel100").
// Rueckgabe: 0..127 oder -1 wenn nicht erkannt.
inline int parseNoteName (const std::string& name)
{
    const size_t end = name.find_first_of ("_ ");
    const std::string t = name.substr (0, end);
    if (t.empty()) return -1;

    bool allDigits = true;
    for (char c : t) allDigits = allDigits && std::isdigit ((unsigned char) c);
    if (allDigits)
    {
        if (t.size() > 3) return -1;
        const int v = std::stoi (t);
        return v <= 127 ? v : -1;
    }

    int semi = 0;
    switch (std::toupper ((unsigned char) t[0]))
    {
        case 'C': semi = 0; break;  case 'D': semi = 2; break;  case 'E': semi = 4; break;
        case 'F': semi = 5; break;  case 'G': semi = 7; break;  case 'A': semi = 9; break;
        case 'B': semi = 11; break; default: return -1;
    }
    size_t i = 1;
    if (i < t.size() && t[i] == '#') { ++semi; ++i; }
    else if (i + 1 < t.size() && t[i] == 'b' && (std::isdigit ((unsigned char) t[i + 1]) || t[i + 1] == '-')) { --semi; ++i; }

    if (i >= t.size()) return -1;
    size_t k = i;
    if (t[k] == '-') ++k;
    if (k >= t.size()) return -1;
    for (size_t j = k; j < t.size(); ++j) if (! std::isdigit ((unsigned char) t[j])) return -1;
    const int oct = std::stoi (t.substr (i));
    const int midi = (oct + 2) * 12 + semi;
    return (midi >= 0 && midi <= 127) ? midi : -1;
}
} // namespace nova

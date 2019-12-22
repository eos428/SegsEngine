#pragma once
#include "core/hashfuncs.h"
#include "core/ustring.h"
#include "core/se_string.h"

namespace StringUtils {
GODOT_EXPORT double to_double(const CharType *p_str, const CharType **r_end = nullptr);
[[nodiscard]] inline bool is_subsequence_ofi(const String &str,const String &p_string) {
    return is_subsequence_of(str,p_string, CaseInsensitive);
}
[[nodiscard]] inline bool is_subsequence_ofi(se_string_view str,se_string_view p_string) {
    return is_subsequence_of(str,p_string, CaseInsensitive);
}
[[nodiscard]] inline se_string to_utf8(const String &s) {
    return se_string(s.toUtf8().data());
}
[[nodiscard]] inline WString to_wstring(const String &s) {
    return s.toStdWString();
}
/* hash the string */
static inline uint32_t hash(const String &s) {
    return hash_djb2_buffer((const uint16_t *)s.constData(),s.length());
}
/* hash the string */
static inline uint64_t hash64(const String &s) {
    return hash_djb2_buffer64((const uint16_t *)s.constData(),s.length());
}
static inline uint64_t hash64(se_string_view s) {
    return hash_djb2_buffer64((const uint8_t *)s.data(),s.length());
}
}
template<>
struct Hasher<String> {
    _FORCE_INLINE_ uint32_t operator()(const String &p_string) const { return StringUtils::hash(p_string); }
};

template<>
struct Hasher<QChar> {
    uint32_t operator()(QChar c) {
        return Hasher<uint16_t>()(c.unicode());
    }
};
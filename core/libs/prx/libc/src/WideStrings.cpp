#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <stdexcept>
#include <string>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"

// The guest wchar_t is 16 bits wide (UTF-16), unlike the 32-bit host wchar_t on Linux,
// so none of these functions may forward to the host wide-character routines.
using GuestWchar = char16_t;

namespace {

std::size_t length(const GuestWchar* text) {
    std::size_t size = 0;
    while (text[size] != 0) ++size;
    return size;
}

// Narrows the numeric prefix of a wide string for the host strto* parsers; parsing stops at the
// first non-ASCII character, which no numeric syntax accepts.
std::string narrowNumber(const GuestWchar* text) {
    std::string result;
    for (; *text != 0 && *text < 0x80; ++text) result.push_back(static_cast<char>(*text));
    return result;
}

template<typename TResult, typename TParse>
TResult parseNumber(const GuestWchar* text, GuestWchar** end, TParse parse) {
    if (!text) throw std::invalid_argument("wcsto*: null string");
    const std::string narrow = narrowNumber(text);
    char* narrowEnd = nullptr;
    const TResult value = parse(narrow.c_str(), &narrowEnd);
    if (end) *end = const_cast<GuestWchar*>(text + (narrowEnd - narrow.c_str()));
    return value;
}

void appendUtf16(std::u16string& out, char32_t codePoint) {
    if (codePoint >= 0x10000) {
        codePoint -= 0x10000;
        out.push_back(static_cast<char16_t>(0xD800 + (codePoint >> 10)));
        out.push_back(static_cast<char16_t>(0xDC00 + (codePoint & 0x3FF)));
    } else {
        out.push_back(static_cast<char16_t>(codePoint));
    }
}

// Decodes UTF-8; bytes that do not form a valid sequence are kept as Latin-1 code points.
std::u16string widen(const char* text, std::size_t limit) {
    std::u16string out;
    const auto* bytes = reinterpret_cast<const unsigned char*>(text);
    std::size_t index = 0;
    while (index < limit && bytes[index] != 0) {
        const unsigned char lead = bytes[index];
        std::size_t extra = lead >= 0xF0 && lead < 0xF5 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC2 && lead < 0xE0 ? 1 : 0;
        char32_t codePoint = extra == 3 ? lead & 0x07 : extra == 2 ? lead & 0x0F : extra == 1 ? lead & 0x1F : lead;
        bool valid = lead < 0x80 || extra != 0;
        for (std::size_t offset = 1; valid && offset <= extra; ++offset) {
            if (index + offset >= limit || (bytes[index + offset] & 0xC0) != 0x80) valid = false;
            else codePoint = (codePoint << 6) | (bytes[index + offset] & 0x3F);
        }
        if (!valid) {
            out.push_back(lead);
            ++index;
            continue;
        }
        appendUtf16(out, codePoint);
        index += extra + 1;
    }
    return out;
}

struct Conversion {
    std::string flags;
    int width = -1;
    int precision = -1;
    std::string lengthModifier;
    char specifier = 0;

    bool LeftAligned() const { return flags.find('-') != std::string::npos; }

    std::string NarrowSpec(const char* lengthOverride) const {
        std::string spec = "%" + flags;
        if (width >= 0) spec += std::to_string(width);
        if (precision >= 0) spec += "." + std::to_string(precision);
        return spec + lengthOverride + specifier;
    }
};

void pad(std::u16string& out, const Conversion& conversion, const std::u16string& text) {
    const std::size_t width = conversion.width > 0 ? static_cast<std::size_t>(conversion.width) : 0;
    const std::size_t fill = width > text.size() ? width - text.size() : 0;
    if (!conversion.LeftAligned()) out.append(fill, u' ');
    out += text;
    if (conversion.LeftAligned()) out.append(fill, u' ');
}

template<typename TValue>
void appendNarrow(std::u16string& out, const std::string& spec, TValue value) {
    const int size = std::snprintf(nullptr, 0, spec.c_str(), value);
    if (size < 0) throw std::runtime_error("vswprintf: conversion failed");
    std::string text(static_cast<std::size_t>(size) + 1, '\0');
    std::snprintf(text.data(), text.size(), spec.c_str(), value);
    for (int index = 0; index < size; ++index) out.push_back(static_cast<unsigned char>(text[static_cast<std::size_t>(index)]));
}

long long nextSigned(LibcDetail::FormatArguments& args, const std::string& lengthModifier) {
    if (lengthModifier == "hh") return static_cast<signed char>(args.Next<int>());
    if (lengthModifier == "h") return static_cast<short>(args.Next<int>());
    if (lengthModifier.empty()) return args.Next<int>();
    return args.Next<long long>();
}

unsigned long long nextUnsigned(LibcDetail::FormatArguments& args, const std::string& lengthModifier) {
    if (lengthModifier == "hh") return static_cast<unsigned char>(args.Next<unsigned int>());
    if (lengthModifier == "h") return static_cast<unsigned short>(args.Next<unsigned int>());
    if (lengthModifier.empty()) return args.Next<unsigned int>();
    return args.Next<unsigned long long>();
}

std::u16string format(const GuestWchar* format, const void* sourceArgs) {
    LibcDetail::FormatArguments args(sourceArgs);
    std::u16string out;
    for (const GuestWchar* cursor = format; *cursor != 0; ++cursor) {
        if (*cursor != u'%') {
            out.push_back(*cursor);
            continue;
        }
        ++cursor;
        if (*cursor == u'%') {
            out.push_back(u'%');
            continue;
        }
        Conversion conversion;
        while (*cursor == u'-' || *cursor == u'+' || *cursor == u' ' || *cursor == u'#' || *cursor == u'0') conversion.flags.push_back(static_cast<char>(*cursor++));
        if (*cursor == u'*') {
            conversion.width = args.Next<int>();
            if (conversion.width < 0) {
                conversion.flags.push_back('-');
                conversion.width = -conversion.width;
            }
            ++cursor;
        } else {
            for (; *cursor >= u'0' && *cursor <= u'9'; ++cursor) conversion.width = std::max(conversion.width, 0) * 10 + (*cursor - u'0');
        }
        if (*cursor == u'.') {
            ++cursor;
            conversion.precision = 0;
            if (*cursor == u'*') {
                conversion.precision = args.Next<int>();
                ++cursor;
            } else {
                for (; *cursor >= u'0' && *cursor <= u'9'; ++cursor) conversion.precision = conversion.precision * 10 + (*cursor - u'0');
            }
        }
        while (*cursor == u'h' || *cursor == u'l' || *cursor == u'L' || *cursor == u'j' || *cursor == u'z' || *cursor == u't' || *cursor == u'q') conversion.lengthModifier.push_back(static_cast<char>(*cursor++));
        if (*cursor == 0 || *cursor >= 0x80) throw std::runtime_error("vswprintf: malformed conversion specification");
        conversion.specifier = static_cast<char>(*cursor);
        switch (conversion.specifier) {
        case 'd':
        case 'i':
            appendNarrow(out, conversion.NarrowSpec("ll"), nextSigned(args, conversion.lengthModifier));
            break;
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            appendNarrow(out, conversion.NarrowSpec("ll"), nextUnsigned(args, conversion.lengthModifier));
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            if (conversion.lengthModifier == "L") appendNarrow(out, conversion.NarrowSpec("L"), args.Next<long double>());
            else appendNarrow(out, conversion.NarrowSpec(""), args.Next<double>());
            break;
        case 'p':
            appendNarrow(out, conversion.NarrowSpec(""), args.Next<void*>());
            break;
        case 'c': {
            const auto value = args.Next<unsigned int>();
            const std::u16string text = conversion.lengthModifier == "l" ? std::u16string(1, static_cast<GuestWchar>(value)) : widen(reinterpret_cast<const char*>(&value), 1);
            pad(out, conversion, text);
            break;
        }
        case 's': {
            const std::size_t limit = conversion.precision >= 0 ? static_cast<std::size_t>(conversion.precision) : SIZE_MAX;
            std::u16string text;
            if (conversion.lengthModifier == "l") {
                const auto* value = args.Next<const GuestWchar*>();
                if (!value) value = u"(null)";
                for (std::size_t index = 0; index < limit && value[index] != 0; ++index) text.push_back(value[index]);
            } else {
                const auto* value = args.Next<const char*>();
                text = widen(value ? value : "(null)", limit);
            }
            pad(out, conversion, text);
            break;
        }
        default:
            throw std::runtime_error(std::string("vswprintf: unsupported conversion %") + conversion.specifier);
        }
    }
    return out;
}

int classify(std::uint32_t character, int type) {
    const auto value = static_cast<wint_t>(character);
    switch (type) {
    case 1: return std::iswalnum(value);
    case 2: return std::iswalpha(value);
    case 3: return std::iswcntrl(value);
    case 4: return std::iswdigit(value);
    case 5: return std::iswgraph(value);
    case 6: return std::iswlower(value);
    case 7: return std::iswprint(value);
    case 8: return std::iswpunct(value);
    case 9: return std::iswspace(value);
    case 10: return std::iswupper(value);
    case 11: return std::iswxdigit(value);
    case 12: return std::iswblank(value);
    default: throw std::invalid_argument("_Iswctype: unknown character class " + std::to_string(type));
    }
}

}

extern "C" {

const GuestWchar* APS5_VABI wmemchr_nid_postfix(const GuestWchar* s, GuestWchar c, size_t n) {
    for (; n != 0; --n, ++s) {
        if (*s == c) return s;
    }
    return nullptr;
}

int APS5_VABI wmemcmp_nid_postfix(const GuestWchar* s1, const GuestWchar* s2, size_t n) {
    for (; n != 0; --n, ++s1, ++s2) {
        if (*s1 != *s2) return *s1 < *s2 ? -1 : 1;
    }
    return 0;
}

GuestWchar* APS5_VABI wmemcpy_nid_postfix(GuestWchar* dest, const GuestWchar* src, size_t n) {
    return static_cast<GuestWchar*>(std::memcpy(dest, src, n * sizeof(GuestWchar)));
}

GuestWchar* APS5_VABI wmemmove_nid_postfix(GuestWchar* dest, const GuestWchar* src, size_t n) {
    return static_cast<GuestWchar*>(std::memmove(dest, src, n * sizeof(GuestWchar)));
}

size_t APS5_VABI wcslen_nid_postfix(const GuestWchar* s) {
    return length(s);
}

GuestWchar* APS5_VABI wcscpy_nid_postfix(GuestWchar* dest, const GuestWchar* src) {
    return static_cast<GuestWchar*>(std::memmove(dest, src, (length(src) + 1) * sizeof(GuestWchar)));
}

GuestWchar* APS5_VABI wcsncpy_nid_postfix(GuestWchar* dest, const GuestWchar* src, size_t n) {
    std::size_t index = 0;
    for (; index < n && src[index] != 0; ++index) dest[index] = src[index];
    for (; index < n; ++index) dest[index] = 0;
    return dest;
}

GuestWchar* APS5_VABI wcscat_nid_postfix(GuestWchar* dest, const GuestWchar* src) {
    wcscpy_nid_postfix(dest + length(dest), src);
    return dest;
}

int APS5_VABI wcscmp_nid_postfix(const GuestWchar* s1, const GuestWchar* s2) {
    for (; *s1 == *s2; ++s1, ++s2) {
        if (*s1 == 0) return 0;
    }
    return *s1 < *s2 ? -1 : 1;
}

int APS5_VABI wcsncmp_nid_postfix(const GuestWchar* s1, const GuestWchar* s2, size_t n) {
    for (; n != 0; --n, ++s1, ++s2) {
        if (*s1 != *s2) return *s1 < *s2 ? -1 : 1;
        if (*s1 == 0) return 0;
    }
    return 0;
}

const GuestWchar* APS5_VABI wcschr_nid_postfix(const GuestWchar* s, GuestWchar c) {
    for (;; ++s) {
        if (*s == c) return s;
        if (*s == 0) return nullptr;
    }
}

const GuestWchar* APS5_VABI wcsrchr_nid_postfix(const GuestWchar* s, GuestWchar c) {
    const GuestWchar* found = nullptr;
    for (;; ++s) {
        if (*s == c) found = s;
        if (*s == 0) return found;
    }
}

const GuestWchar* APS5_VABI wcsstr_nid_postfix(const GuestWchar* haystack, const GuestWchar* needle) {
    const std::size_t needleLength = length(needle);
    if (needleLength == 0) return haystack;
    for (; *haystack != 0; ++haystack) {
        if (wcsncmp_nid_postfix(haystack, needle, needleLength) == 0) return haystack;
    }
    return nullptr;
}

long APS5_VABI wcstol_nid_postfix(const GuestWchar* s, GuestWchar** end, int base) {
    return parseNumber<long>(s, end, [base](const char* text, char** narrowEnd) { return std::strtol(text, narrowEnd, base); });
}

long long APS5_VABI wcstoll_nid_postfix(const GuestWchar* s, GuestWchar** end, int base) {
    return parseNumber<long long>(s, end, [base](const char* text, char** narrowEnd) { return std::strtoll(text, narrowEnd, base); });
}

unsigned long long APS5_VABI wcstoull_nid_postfix(const GuestWchar* s, GuestWchar** end, int base) {
    return parseNumber<unsigned long long>(s, end, [base](const char* text, char** narrowEnd) { return std::strtoull(text, narrowEnd, base); });
}

double APS5_VABI wcstod_nid_postfix(const GuestWchar* s, GuestWchar** end) {
    return parseNumber<double>(s, end, [](const char* text, char** narrowEnd) { return std::strtod(text, narrowEnd); });
}

float APS5_VABI wcstof_nid_postfix(const GuestWchar* s, GuestWchar** end) {
    return parseNumber<float>(s, end, [](const char* text, char** narrowEnd) { return std::strtof(text, narrowEnd); });
}

int APS5_VABI vswprintf_nid_postfix(GuestWchar* buffer, size_t size, const GuestWchar* formatText, VaList* args) {
    if (!formatText || !args) throw std::invalid_argument("vswprintf: null argument");
    if (size != 0 && !buffer) throw std::invalid_argument("vswprintf: null buffer");
    const std::u16string text = format(formatText, args);
    if (size == 0) return -1;
    const std::size_t copied = std::min(text.size(), size - 1);
    std::memcpy(buffer, text.data(), copied * sizeof(GuestWchar));
    buffer[copied] = 0;
    return text.size() < size ? static_cast<int>(text.size()) : -1;
}

int APS5_VABI _Iswctype_nid_postfix(std::uint32_t character, int type) {
    if (character == 0xFFFFFFFFu) return 0;
    return classify(character, type);
}

}

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// sce::Json classes. Every guest object (String, Value, Object, Array) is a single pointer to a
// host-side representation; the game allocates at least that much for each of them. Containers
// hold guest-layout handles so references returned to the guest stay valid until modified.

namespace {

constexpr int SCE_JSON_ERROR_PARSE_INVALID_CHAR = static_cast<int>(0x80920101);

enum ValueType : int {
    Null = 0,
    Boolean = 1,
    Integer = 2,
    UInteger = 3,
    Real = 4,
    String = 5,
    Array = 6,
    Object = 7,
};

struct ValueData;
struct ObjectData;
struct ArrayData;

struct StringHandle {
    std::string* text;
};

struct ValueHandle {
    ValueData* data;
};

struct ObjectHandle {
    ObjectData* data;
};

struct ArrayHandle {
    ArrayData* data;
};

struct ObjectData {
    std::deque<std::pair<StringHandle, ValueHandle>> entries;
};

struct ArrayData {
    std::deque<ValueHandle> items;
};

struct ValueData {
    ValueType type = Null;
    bool boolean = false;
    std::int64_t integer = 0;
    std::uint64_t uinteger = 0;
    double real = 0;
    StringHandle string{nullptr};
    ArrayHandle array{nullptr};
    ObjectHandle object{nullptr};
};

void destroyValue(ValueHandle& value);

StringHandle makeString(std::string text) {
    return StringHandle{new std::string(std::move(text))};
}

void destroyObject(ObjectHandle& object) {
    if (!object.data) return;
    for (auto& [key, value] : object.data->entries) {
        delete key.text;
        destroyValue(value);
    }
    delete object.data;
    object.data = nullptr;
}

void destroyArray(ArrayHandle& array) {
    if (!array.data) return;
    for (auto& item : array.data->items) destroyValue(item);
    delete array.data;
    array.data = nullptr;
}

void clearValue(ValueData& data) {
    delete data.string.text;
    data.string.text = nullptr;
    destroyArray(data.array);
    destroyObject(data.object);
    data = ValueData{};
}

void destroyValue(ValueHandle& value) {
    if (!value.data) return;
    clearValue(*value.data);
    delete value.data;
    value.data = nullptr;
}

ValueHandle makeValue(ValueType type = Null) {
    auto* data = new ValueData();
    data->type = type;
    if (type == String) data->string = makeString("");
    if (type == Array) data->array.data = new ArrayData();
    if (type == Object) data->object.data = new ObjectData();
    return ValueHandle{data};
}

ValueHandle copyValue(const ValueHandle& source);

ObjectData* copyObject(const ObjectData* source) {
    auto* copy = new ObjectData();
    if (source) {
        for (const auto& [key, value] : source->entries) copy->entries.emplace_back(makeString(*key.text), copyValue(value));
    }
    return copy;
}

ArrayData* copyArray(const ArrayData* source) {
    auto* copy = new ArrayData();
    if (source) {
        for (const auto& item : source->items) copy->items.push_back(copyValue(item));
    }
    return copy;
}

ValueHandle copyValue(const ValueHandle& source) {
    ValueHandle copy = makeValue();
    if (!source.data) return copy;
    const ValueData& from = *source.data;
    ValueData& to = *copy.data;
    to.type = from.type;
    to.boolean = from.boolean;
    to.integer = from.integer;
    to.uinteger = from.uinteger;
    to.real = from.real;
    if (from.string.text) to.string = makeString(*from.string.text);
    if (from.array.data) to.array.data = copyArray(from.array.data);
    if (from.object.data) to.object.data = copyObject(from.object.data);
    return copy;
}

void assignValue(ValueHandle& target, const ValueHandle& source) {
    if (&target == &source) return;
    ValueHandle copy = copyValue(source);
    destroyValue(target);
    target = copy;
}

ValueData& data(ValueHandle* value) {
    if (!value->data) *value = makeValue();
    return *value->data;
}

const ValueHandle& nullValue() {
    static const ValueHandle value = makeValue();
    return value;
}

const StringHandle& emptyString() {
    static const StringHandle text = makeString("");
    return text;
}

const ArrayHandle& emptyArray() {
    static const ArrayHandle array{new ArrayData()};
    return array;
}

const ObjectHandle& emptyObject() {
    static const ObjectHandle object{new ObjectData()};
    return object;
}

ValueHandle& objectEntry(ObjectData& object, const std::string& key) {
    for (auto& [name, value] : object.entries) {
        if (*name.text == key) return value;
    }
    object.entries.emplace_back(makeString(key), makeValue());
    return object.entries.back().second;
}

void appendEscaped(std::string& out, const std::string& text) {
    out.push_back('"');
    for (const unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char escaped[7];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                out += escaped;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

void serialize(const ValueHandle& value, std::string& out) {
    if (!value.data) {
        out += "null";
        return;
    }
    const ValueData& v = *value.data;
    switch (v.type) {
    case Null: out += "null"; break;
    case Boolean: out += v.boolean ? "true" : "false"; break;
    case Integer: out += std::to_string(v.integer); break;
    case UInteger: out += std::to_string(v.uinteger); break;
    case Real: {
        char text[32];
        std::snprintf(text, sizeof(text), "%.17g", v.real);
        out += text;
        break;
    }
    case String: appendEscaped(out, v.string.text ? *v.string.text : std::string()); break;
    case Array: {
        out.push_back('[');
        bool first = true;
        if (v.array.data) {
            for (const auto& item : v.array.data->items) {
                if (!first) out.push_back(',');
                first = false;
                serialize(item, out);
            }
        }
        out.push_back(']');
        break;
    }
    case Object: {
        out.push_back('{');
        bool first = true;
        if (v.object.data) {
            for (const auto& [key, item] : v.object.data->entries) {
                if (!first) out.push_back(',');
                first = false;
                appendEscaped(out, *key.text);
                out.push_back(':');
                serialize(item, out);
            }
        }
        out.push_back('}');
        break;
    }
    }
}

class JsonParser {
public:
    JsonParser(const char* text, std::size_t size) : cursor(text), end(text + size) {}

    bool Parse(ValueHandle& out) {
        if (!ParseValue(out)) return false;
        SkipWhitespace();
        return cursor == end || *cursor == 0;
    }

private:
    const char* cursor;
    const char* end;

    void SkipWhitespace() {
        while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')) ++cursor;
    }

    bool Literal(const char* word) {
        const std::size_t length = std::strlen(word);
        if (static_cast<std::size_t>(end - cursor) < length || std::strncmp(cursor, word, length) != 0) return false;
        cursor += length;
        return true;
    }

    static void AppendUtf8(std::string& out, std::uint32_t codePoint) {
        if (codePoint < 0x80) {
            out.push_back(static_cast<char>(codePoint));
        } else if (codePoint < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    bool Hex4(std::uint32_t& value) {
        if (end - cursor < 4) return false;
        value = 0;
        for (int index = 0; index < 4; ++index, ++cursor) {
            const char c = *cursor;
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool ParseString(std::string& out) {
        if (cursor >= end || *cursor != '"') return false;
        ++cursor;
        while (cursor < end && *cursor != '"') {
            if (*cursor != '\\') {
                out.push_back(*cursor++);
                continue;
            }
            if (++cursor >= end) return false;
            const char escape = *cursor++;
            switch (escape) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                std::uint32_t codePoint = 0;
                if (!Hex4(codePoint)) return false;
                if (codePoint >= 0xD800 && codePoint < 0xDC00 && end - cursor >= 6 && cursor[0] == '\\' && cursor[1] == 'u') {
                    cursor += 2;
                    std::uint32_t low = 0;
                    if (!Hex4(low) || low < 0xDC00 || low >= 0xE000) return false;
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                }
                AppendUtf8(out, codePoint);
                break;
            }
            default: return false;
            }
        }
        if (cursor >= end) return false;
        ++cursor;
        return true;
    }

    bool ParseNumber(ValueData& out) {
        const char* start = cursor;
        if (cursor < end && *cursor == '-') ++cursor;
        bool isReal = false;
        while (cursor < end && ((*cursor >= '0' && *cursor <= '9') || *cursor == '.' || *cursor == 'e' || *cursor == 'E' || *cursor == '+' || *cursor == '-')) {
            if (*cursor == '.' || *cursor == 'e' || *cursor == 'E') isReal = true;
            ++cursor;
        }
        const std::string text(start, cursor);
        if (text.empty() || text == "-") return false;
        char* parsedEnd = nullptr;
        errno = 0;
        if (!isReal && text[0] == '-') {
            out.type = Integer;
            out.integer = std::strtoll(text.c_str(), &parsedEnd, 10);
        } else if (!isReal) {
            const unsigned long long value = std::strtoull(text.c_str(), &parsedEnd, 10);
            if (value <= static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max())) {
                out.type = Integer;
                out.integer = static_cast<std::int64_t>(value);
            } else {
                out.type = UInteger;
                out.uinteger = value;
            }
        } else {
            out.type = Real;
            out.real = std::strtod(text.c_str(), &parsedEnd);
        }
        return parsedEnd && *parsedEnd == 0 && errno == 0;
    }

    bool ParseValue(ValueHandle& out) {
        SkipWhitespace();
        if (cursor >= end) return false;
        ValueData& value = data(&out);
        clearValue(value);
        switch (*cursor) {
        case 'n': return Literal("null");
        case 't': value.type = Boolean; value.boolean = true; return Literal("true");
        case 'f': value.type = Boolean; value.boolean = false; return Literal("false");
        case '"': {
            std::string text;
            if (!ParseString(text)) return false;
            value.type = String;
            value.string = makeString(std::move(text));
            return true;
        }
        case '[': {
            ++cursor;
            value.type = Array;
            value.array.data = new ArrayData();
            SkipWhitespace();
            if (cursor < end && *cursor == ']') {
                ++cursor;
                return true;
            }
            while (true) {
                value.array.data->items.push_back(makeValue());
                if (!ParseValue(value.array.data->items.back())) return false;
                SkipWhitespace();
                if (cursor < end && *cursor == ',') {
                    ++cursor;
                    continue;
                }
                if (cursor < end && *cursor == ']') {
                    ++cursor;
                    return true;
                }
                return false;
            }
        }
        case '{': {
            ++cursor;
            value.type = Object;
            value.object.data = new ObjectData();
            SkipWhitespace();
            if (cursor < end && *cursor == '}') {
                ++cursor;
                return true;
            }
            while (true) {
                SkipWhitespace();
                std::string key;
                if (!ParseString(key)) return false;
                SkipWhitespace();
                if (cursor >= end || *cursor != ':') return false;
                ++cursor;
                ValueHandle& entry = objectEntry(*value.object.data, key);
                if (!ParseValue(entry)) return false;
                SkipWhitespace();
                if (cursor < end && *cursor == ',') {
                    ++cursor;
                    continue;
                }
                if (cursor < end && *cursor == '}') {
                    ++cursor;
                    return true;
                }
                return false;
            }
        }
        default:
            return ParseNumber(value);
        }
    }
};

struct InitParameter2 {
    void* allocator;
    void* userData;
    std::size_t fileBufferSize;
};

[[noreturn]] void APS5_VABI pureVirtualCall(void*) {
    throw std::runtime_error("sce::Json::MemAllocator: pure virtual function called");
}

void APS5_VABI memAllocatorDestructor(void*) {}

void APS5_VABI memAllocatorNotifyError(void*, int, std::size_t, void*) {}

// Base-class vtable of sce::Json::MemAllocator: complete dtor, deleting dtor, allocate,
// deallocate, notifyError. Derived allocators install their own vtable after this constructor.
void* const memAllocatorVtable[] = {
    reinterpret_cast<void*>(&memAllocatorDestructor),
    reinterpret_cast<void*>(&pureVirtualCall),
    reinterpret_cast<void*>(&pureVirtualCall),
    reinterpret_cast<void*>(&pureVirtualCall),
    reinterpret_cast<void*>(&memAllocatorNotifyError),
};

}

extern "C" {

void APS5_VABI _ZN3sce4Json12MemAllocatorC2Ev_nid_postfix(void* self) {
    *static_cast<void* const**>(self) = memAllocatorVtable;
}

void APS5_VABI _ZN3sce4Json12MemAllocatorD2Ev_nid_postfix(void* self) {
    *static_cast<void* const**>(self) = memAllocatorVtable;
}

void APS5_VABI _ZN3sce4Json14InitParameter2C1Ev_nid_postfix(InitParameter2* self) {
    *self = InitParameter2{nullptr, nullptr, 0};
}

void APS5_VABI _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv_nid_postfix(InitParameter2* self, void* allocator, void* userData) {
    self->allocator = allocator;
    self->userData = userData;
}

void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm_nid_postfix(InitParameter2* self, std::size_t size) {
    self->fileBufferSize = size;
}

// The guest Initializer object is a single byte, so it carries no state.
void APS5_VABI _ZN3sce4Json11InitializerC1Ev_nid_postfix(void* self) {
    (void)self;
}

void APS5_VABI _ZN3sce4Json11InitializerD1Ev_nid_postfix(void* self) {
    (void)self;
}

int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E_nid_postfix(void* self, const InitParameter2* parameter) {
    (void)self;
    return parameter ? 0 : SCE_JSON_ERROR_PARSE_INVALID_CHAR;
}

int APS5_VABI _ZN3sce4Json11Initializer9terminateEv_nid_postfix(void* self) {
    (void)self;
    return 0;
}

void APS5_VABI _ZN3sce4Json6StringC1Ev_nid_postfix(StringHandle* self) {
    new (self) StringHandle{makeString("")};
}

void APS5_VABI _ZN3sce4Json6StringC1EPKc_nid_postfix(StringHandle* self, const char* text) {
    new (self) StringHandle{makeString(text ? text : "")};
}

void APS5_VABI _ZN3sce4Json6StringD1Ev_nid_postfix(StringHandle* self) {
    delete self->text;
    self->text = nullptr;
}

StringHandle* APS5_VABI _ZN3sce4Json6StringaSERKS1__nid_postfix(StringHandle* self, const StringHandle* other) {
    if (self != other) *self->text = other->text ? *other->text : std::string();
    return self;
}

const char* APS5_VABI _ZNK3sce4Json6String5c_strEv_nid_postfix(const StringHandle* self) {
    return self->text ? self->text->c_str() : "";
}

std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv_nid_postfix(const StringHandle* self) {
    return self->text ? self->text->size() : 0;
}

void APS5_VABI _ZN3sce4Json5ValueC1Ev_nid_postfix(ValueHandle* self) {
    new (self) ValueHandle{makeValue()};
}

void APS5_VABI _ZN3sce4Json5ValueC1Eb_nid_postfix(ValueHandle* self, bool value) {
    new (self) ValueHandle{makeValue(Boolean)};
    self->data->boolean = value;
}

void APS5_VABI _ZN3sce4Json5ValueC1El_nid_postfix(ValueHandle* self, long value) {
    new (self) ValueHandle{makeValue(Integer)};
    self->data->integer = value;
}

void APS5_VABI _ZN3sce4Json5ValueC1Ed_nid_postfix(ValueHandle* self, double value) {
    new (self) ValueHandle{makeValue(Real)};
    self->data->real = value;
}

void APS5_VABI _ZN3sce4Json5ValueC1EPKc_nid_postfix(ValueHandle* self, const char* value) {
    new (self) ValueHandle{makeValue()};
    self->data->type = String;
    self->data->string = makeString(value ? value : "");
}

void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE_nid_postfix(ValueHandle* self, const StringHandle* value) {
    _ZN3sce4Json5ValueC1EPKc_nid_postfix(self, value->text ? value->text->c_str() : "");
}

void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE_nid_postfix(ValueHandle* self, const ObjectHandle* value) {
    new (self) ValueHandle{makeValue()};
    self->data->type = Object;
    self->data->object.data = copyObject(value->data);
}

void APS5_VABI _ZN3sce4Json5ValueD1Ev_nid_postfix(ValueHandle* self) {
    destroyValue(*self);
}

ValueHandle* APS5_VABI _ZN3sce4Json5ValueaSERKS1__nid_postfix(ValueHandle* self, const ValueHandle* other) {
    assignValue(*self, *other);
    return self;
}

int APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE_nid_postfix(ValueHandle* self, int type) {
    if (type < Null || type > Object) throw std::invalid_argument("sce::Json::Value::set: invalid value type " + std::to_string(type));
    destroyValue(*self);
    *self = makeValue(static_cast<ValueType>(type));
    return 0;
}

int APS5_VABI _ZNK3sce4Json5Value7getTypeEv_nid_postfix(const ValueHandle* self) {
    return self->data ? self->data->type : Null;
}

bool APS5_VABI _ZNK3sce4Json5Value10getBooleanEv_nid_postfix(const ValueHandle* self) {
    return self->data && self->data->type == Boolean && self->data->boolean;
}

std::int64_t APS5_VABI _ZNK3sce4Json5Value10getIntegerEv_nid_postfix(const ValueHandle* self) {
    if (!self->data) return 0;
    if (self->data->type == Integer) return self->data->integer;
    if (self->data->type == UInteger) return static_cast<std::int64_t>(self->data->uinteger);
    return 0;
}

double APS5_VABI _ZNK3sce4Json5Value7getRealEv_nid_postfix(const ValueHandle* self) {
    return self->data && self->data->type == Real ? self->data->real : 0.0;
}

const StringHandle* APS5_VABI _ZNK3sce4Json5Value9getStringEv_nid_postfix(const ValueHandle* self) {
    return self->data && self->data->type == String ? &self->data->string : &emptyString();
}

const ArrayHandle* APS5_VABI _ZNK3sce4Json5Value8getArrayEv_nid_postfix(const ValueHandle* self) {
    return self->data && self->data->type == Array ? &self->data->array : &emptyArray();
}

const ObjectHandle* APS5_VABI _ZNK3sce4Json5Value9getObjectEv_nid_postfix(const ValueHandle* self) {
    return self->data && self->data->type == Object ? &self->data->object : &emptyObject();
}

ArrayHandle* APS5_VABI _ZN3sce4Json5Value10referArrayEv_nid_postfix(ValueHandle* self) {
    return self->data && self->data->type == Array ? &self->data->array : nullptr;
}

ObjectHandle* APS5_VABI _ZN3sce4Json5Value11referObjectEv_nid_postfix(ValueHandle* self) {
    return self->data && self->data->type == Object ? &self->data->object : nullptr;
}

const ValueHandle* APS5_VABI _ZNK3sce4Json5ValueixEPKc_nid_postfix(const ValueHandle* self, const char* key) {
    if (!key || !self->data || self->data->type != Object) return &nullValue();
    for (const auto& [name, value] : self->data->object.data->entries) {
        if (*name.text == key) return &value;
    }
    return &nullValue();
}

const ValueHandle* APS5_VABI _ZNK3sce4Json5ValueixEm_nid_postfix(const ValueHandle* self, std::size_t index) {
    if (!self->data || self->data->type != Array || index >= self->data->array.data->items.size()) return &nullValue();
    return &self->data->array.data->items[index];
}

int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE_nid_postfix(const ValueHandle* self, StringHandle* out) {
    std::string text;
    serialize(*self, text);
    *out->text = std::move(text);
    return 0;
}

void APS5_VABI _ZN3sce4Json6ObjectC1Ev_nid_postfix(ObjectHandle* self) {
    new (self) ObjectHandle{new ObjectData()};
}

void APS5_VABI _ZN3sce4Json6ObjectC1ERKS1__nid_postfix(ObjectHandle* self, const ObjectHandle* other) {
    new (self) ObjectHandle{copyObject(other->data)};
}

void APS5_VABI _ZN3sce4Json6ObjectD1Ev_nid_postfix(ObjectHandle* self) {
    destroyObject(*self);
}

ObjectHandle* APS5_VABI _ZN3sce4Json6ObjectaSERKS1__nid_postfix(ObjectHandle* self, const ObjectHandle* other) {
    if (self == other) return self;
    ObjectData* copy = copyObject(other->data);
    destroyObject(*self);
    self->data = copy;
    return self;
}

void APS5_VABI _ZN3sce4Json6Object5clearEv_nid_postfix(ObjectHandle* self) {
    destroyObject(*self);
    self->data = new ObjectData();
}

ValueHandle* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE_nid_postfix(ObjectHandle* self, const StringHandle* key) {
    if (!self->data) self->data = new ObjectData();
    return &objectEntry(*self->data, key->text ? *key->text : std::string());
}

int APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE_nid_postfix(ArrayHandle* self, const ValueHandle* value) {
    if (!self->data) self->data = new ArrayData();
    self->data->items.push_back(copyValue(*value));
    return 0;
}

const ValueHandle* APS5_VABI _ZNK3sce4Json5Array4backEv_nid_postfix(const ArrayHandle* self) {
    if (!self->data || self->data->items.empty()) return &nullValue();
    return &self->data->items.back();
}

std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv_nid_postfix(const ArrayHandle* self) {
    return self->data ? self->data->items.size() : 0;
}

int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm_nid_postfix(ValueHandle* value, const char* text, std::size_t size) {
    if (!value || !text) return SCE_JSON_ERROR_PARSE_INVALID_CHAR;
    ValueHandle parsed = makeValue();
    if (!JsonParser(text, size).Parse(parsed)) {
        destroyValue(parsed);
        return SCE_JSON_ERROR_PARSE_INVALID_CHAR;
    }
    destroyValue(*value);
    *value = parsed;
    return 0;
}


// Initialization succeeds as on an offline console: the title tears its whole web API layer down on
// an init failure and later dereferences it anyway. Requests made through it fail as offline.
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* self, const void* parameter) {
    (void)self;
    (void)parameter;
    return 0;
}
}

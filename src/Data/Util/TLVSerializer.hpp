#pragma once

namespace Serialization {

    // -------------------- compile-time FNV-1a (32-bit) --------------------
    consteval uint32_t fnv1a32_cstr(std::string_view s) noexcept {
        uint32_t h = 0x811c9dc5u;
        for (char c : s) {
            h ^= static_cast<uint8_t>(c);
            h = h * 0x01000193u;
        }
        return h;
    }

    // -------------------- compile-time collision check --------------------
    namespace detail {

        template<class T>
        consteval std::size_t reflect_member_count() {
            return static_cast<std::size_t>(reflect::size<T>());
        }

        template<class T, std::size_t... Is>
        consteval auto member_tags(std::index_sequence<Is...>) {
            return std::array<std::uint32_t, sizeof...(Is)>{
                fnv1a32_cstr(reflect::member_name < std::integral_constant<std::size_t, Is>{}, T > ())...
            };
        }

        template<class T>
        consteval bool has_hash_collisions() {
            constexpr std::size_t N = reflect_member_count<T>();
            constexpr auto tags = member_tags<T>(std::make_index_sequence<N>{});

            for (std::size_t i = 0; i < N; ++i) {
                for (std::size_t j = i + 1; j < N; ++j) {
                    if (tags[i] == tags[j]) {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    template<typename T>
    consteval bool check_member_hash_collisions() {
        return !detail::has_hash_collisions<T>();
    }

    // -------------------- type codes --------------------
    enum : uint8_t {
        TC_INVALID = 0,
        TC_INT8    = 1,
        TC_INT16   = 2,
        TC_INT32   = 3,
        TC_INT64   = 4,
        TC_UINT8   = 5,
        TC_UINT16  = 6,
        TC_UINT32  = 7,
        TC_UINT64  = 8,
        TC_FLOAT   = 9,
        TC_DOUBLE  = 10,
        TC_BOOL    = 11,
        TC_STRING  = 12,
        TC_VECTOR  = 13,
        TC_OBJECT  = 14
    };

    template<typename T> struct type_code { static constexpr uint8_t value = TC_INVALID; };

#define TC_MAP(type, code) template<> struct type_code<type> { static constexpr uint8_t value = code; }
    TC_MAP(int8_t,      TC_INT8);
    TC_MAP(int16_t,     TC_INT16);
    TC_MAP(int32_t,     TC_INT32);
    TC_MAP(int64_t,     TC_INT64);
    TC_MAP(uint8_t,     TC_UINT8);
    TC_MAP(uint16_t,    TC_UINT16);
    TC_MAP(uint32_t,    TC_UINT32);
    TC_MAP(uint64_t,    TC_UINT64);
    TC_MAP(float,       TC_FLOAT);
    TC_MAP(double,      TC_DOUBLE);
    TC_MAP(bool,        TC_BOOL);
    TC_MAP(std::string, TC_STRING);
#undef TC_MAP

    template<typename T>
    using decay_noref_t = std::remove_cv_t<std::remove_reference_t<T>>;

    // -------------------- supported basics --------------------
    template<typename T>
    inline constexpr bool supported_basic_v =
        (type_code<decay_noref_t<T>>::value != TC_INVALID) &&
        std::is_trivially_copyable_v<decay_noref_t<T>>;

    // -------------------- std::vector detection --------------------
    template<class T>
    struct is_std_vector : std::false_type {};
    template<class T, class A>
    struct is_std_vector<std::vector<T, A>> : std::true_type {};

    template<class T>
    inline constexpr bool is_std_vector_v = is_std_vector<decay_noref_t<T>>::value;

    template<class V>
    using vector_value_t = typename decay_noref_t<V>::value_type;

    // -------------------- string-like --------------------

    template<class S>
    concept char_data_range =
        requires(const S & s) {
            { s.data() };
            { s.size() } -> std::convertible_to<std::size_t>;
    } && std::is_pointer_v<decltype(std::declval<const S&>().data())> && std::is_convertible_v<decltype(std::declval<const S&>().data()), const char*>;

    template<class S>
    concept string_like_cstr = requires(const S & s) {
        { s.c_str() } -> std::convertible_to<const char*>;
    };

    template<class S>
    constexpr std::string_view to_string_view(const S& s) {
        using D = decay_noref_t<S>;

        if constexpr (std::is_same_v<D, std::string>) {
            return std::string_view{ s };
        }
        else if constexpr (char_data_range<S>) {
            return std::string_view{
                static_cast<const char*>(s.data()),
                static_cast<std::size_t>(s.size())
            };
        }
        else if constexpr (string_like_cstr<S>) {
            const char* p = s.c_str();
            return std::string_view{ p ? p : "", p ? std::char_traits<char>::length(p) : 0u };
        }
        else {
            return {};
        }
    }

    template<class S>
    concept non_std_string_like = (!std::is_same_v<decay_noref_t<S>, std::string>) && (string_like_cstr<S> || char_data_range<S>);

    // -------------------- reflectable aggregate --------------------
    template<class T>
    concept reflectable_object = requires(T & t) {
        reflect::for_each([](const auto) {}, t);
    };

    // Dispatch order in Serialize is string -> vector -> object -> basic, so a scalar must not
    // satisfy reflectable_object or it would be written as TC_OBJECT and silently change the
    // on-disk format of every existing record.
    static_assert(!reflectable_object<float>,         "float must take the basic path");
    static_assert(!reflectable_object<int>,           "int must take the basic path");
    static_assert(!reflectable_object<bool>,          "bool must take the basic path");
    static_assert(!reflectable_object<std::uint32_t>, "uint32 must take the basic path");
    static_assert(!is_std_vector_v<std::string>,      "string must not take the vector path");

    // -------------------- supported members --------------------
    template<typename T>
    inline constexpr bool supported_member_v =
        supported_basic_v<T>                          ||
        std::is_same_v<decay_noref_t<T>, std::string> ||
        non_std_string_like<decay_noref_t<T>>         ||
        is_std_vector_v<T>                            ||
        reflectable_object<decay_noref_t<T>>;

    namespace detail {

        // Folds supported_member_v over every reflected member, so a type can be rejected by a
        // concept instead of failing on a static_assert deep inside TLVSerializer::Serialize.
        template<class T, std::size_t... Is>
        consteval bool all_members_supported(std::index_sequence<Is...>) {
            return (supported_member_v<decltype(reflect::get<Is>(std::declval<T&>()))> && ...);
        }

        template<class T>
        consteval bool members_supported() {
            return all_members_supported<T>(std::make_index_sequence<reflect_member_count<T>()>{});
        }
    }

    // -------------------- TLVSerializer --------------------
    class TLVSerializer {
    public:
        template<typename T>
        static std::vector<uint8_t> Serialize(const T& obj) {
            static_assert(std::is_standard_layout_v<T> || std::is_aggregate_v<T>, "Only standard layout or aggregate structs supported");
            static_assert(check_member_hash_collisions<T>(), "Hash collision detected between member names in struct T");

            std::vector<uint8_t> out;

            reflect::for_each([&](const auto I) {
                constexpr std::string_view name = reflect::member_name<I, T>();
                constexpr uint32_t tag = fnv1a32_cstr(name);

                using MemberT = decltype(reflect::get<I>(obj));
                static_assert(supported_member_v<MemberT>, "Member type not supported by TLVSerializer");

                const auto& val_ref = reflect::get<I>(obj);
                using D = decay_noref_t<MemberT>;

                append_le(out, tag);

                if constexpr (std::is_same_v<D, std::string> || non_std_string_like<D>) {
                    out.push_back(TC_STRING);

                    const std::string_view sv = to_string_view(val_ref);
                    if (sv.size() > 0xFFFFu) {
                        throw std::runtime_error("TLVSerializer: string too large (max 65535)");
                    }

                    const uint16_t len = static_cast<uint16_t>(sv.size());
                    append_le(out, len);

                    const uint8_t* p = reinterpret_cast<const uint8_t*>(sv.data());
                    out.insert(out.end(), p, p + len);
                }
                else if constexpr (is_std_vector_v<D>) {
                    out.push_back(TC_VECTOR);

                    using Elem = decay_noref_t<vector_value_t<D>>;
                    static_assert(
                        supported_basic_v<Elem>           ||
                        std::is_same_v<Elem, std::string> ||
                        non_std_string_like<Elem>         ||
                        reflectable_object<Elem>,
                        "TLVSerializer: vector element type not supported"
                    );

                    std::vector<uint8_t> payload;
                    const uint32_t count = static_cast<uint32_t>(val_ref.size());
                    append_le(payload, count);

                    for (const auto& e : val_ref) {
                        std::vector<uint8_t> eb;

                        if constexpr (std::is_same_v<Elem, std::string> || non_std_string_like<Elem>) {
                            const std::string_view sv = to_string_view(e);
                            if (sv.size() > 0xFFFFu) {
                                throw std::runtime_error("TLVSerializer: vector string element too large (max 65535)");
                            }
                            const uint16_t slen = static_cast<uint16_t>(sv.size());
                            append_le(eb, slen);
                            const uint8_t* p = reinterpret_cast<const uint8_t*>(sv.data());
                            eb.insert(eb.end(), p, p + slen);
                        }
                        else if constexpr (supported_basic_v<Elem>) {
                            const uint8_t* p = reinterpret_cast<const uint8_t*>(std::addressof(e));
                            eb.insert(eb.end(), p, p + sizeof(Elem));
                        }
                        else if constexpr (reflectable_object<Elem>) {
                            eb = Serialize(e);
                        }

                        const uint32_t elen = static_cast<uint32_t>(eb.size());
                        append_le(payload, elen);
                        payload.insert(payload.end(), eb.begin(), eb.end());
                    }

                    if (payload.size() > 0xFFFFu) {
                        throw std::runtime_error("TLVSerializer: vector payload too large (max 65535)");
                    }

                    const uint16_t len = static_cast<uint16_t>(payload.size());
                    append_le(out, len);
                    out.insert(out.end(), payload.begin(), payload.end());
                }
                else if constexpr (reflectable_object<D>) {
                    out.push_back(TC_OBJECT);

                    auto payload = Serialize(val_ref);
                    if (payload.size() > 0xFFFFu) {
                        throw std::runtime_error("TLVSerializer: object payload too large (max 65535)");
                    }

                    const uint16_t len = static_cast<uint16_t>(payload.size());
                    append_le(out, len);
                    out.insert(out.end(), payload.begin(), payload.end());
                }
                else {
                    uint8_t tcode = type_code<D>::value;
                    out.push_back(tcode);

                    const uint16_t len = static_cast<uint16_t>(sizeof(MemberT));
                    append_le(out, len);

                    const uint8_t* p = reinterpret_cast<const uint8_t*>(std::addressof(val_ref));
                    out.insert(out.end(), p, p + len);
                }
            }, obj);

            return out;
        }

        template<typename T>
        static void Deserialize(T& obj, absl::Span<const uint8_t> data) {
            static_assert(std::is_standard_layout_v<T> || std::is_aggregate_v<T>, "Only standard layout or aggregate structs supported");
            static_assert(check_member_hash_collisions<T>(), "Hash collision detected between member names in struct T");

            absl::flat_hash_map<uint32_t, std::pair<uint8_t, absl::Span<const uint8_t>>> map;

            size_t i = 0;
            while (i + 4 + 1 + 2 <= data.size()) {
                uint32_t tag = read_le<uint32_t>(data.data() + i); i += 4;
                uint8_t tcode = data[i++];
                uint16_t len = read_le<uint16_t>(data.data() + i); i += 2;

                if (i + len > data.size()) {
                    throw std::runtime_error("Corrupt TLV buffer");
                }

                map.emplace(tag, std::make_pair(tcode, absl::Span<const uint8_t>(data.data() + i, len)));
                i += len;
            }

            reflect::for_each([&](const auto I) {
                constexpr std::string_view name = reflect::member_name<I, T>();
                constexpr uint32_t tag = fnv1a32_cstr(name);

                using MemberT = decltype(reflect::get<I>(obj));
                static_assert(supported_member_v<MemberT>, "Member type not supported by TLVSerializer");

                if (auto it = map.find(tag); it != map.end()) {
                    auto [tcode, spanv] = it->second;

                    auto& member_ref = reflect::get<I>(obj);
                    using D = decay_noref_t<MemberT>;

                    if constexpr (std::is_same_v<D, std::string> || non_std_string_like<D>) {
                        if (tcode == TC_STRING) {
                            const char* p = reinterpret_cast<const char*>(spanv.data());
                            const size_t n = spanv.size();

                            if constexpr (std::is_same_v<D, std::string>) {
                                member_ref.assign(p, n);
                            }
                            else if constexpr (std::is_constructible_v<D, std::string_view>) {
                                member_ref = D(std::string_view{ p, n });
                            }
                            else if constexpr (std::is_constructible_v<D, const char*>) {
                                std::string tmp(p, n);
                                member_ref = D(tmp.c_str());
                            }
                            else if constexpr (requires(D & d, const char* s) { d = s; }) {
                                std::string tmp(p, n);
                                member_ref = tmp.c_str();
                            }
                        }
                    }
                    else if constexpr (is_std_vector_v<D>) {
                        if (tcode == TC_VECTOR) {
                            using Elem = decay_noref_t<vector_value_t<D>>;

                            size_t off = 0;
                            if (spanv.size() < 4) {
                                return;
                            }
                            const uint32_t count = read_le<uint32_t>(spanv.data() + off);
                            off += 4;

                            auto& vec = member_ref;
                            vec.clear();
                            vec.reserve(count);

                            for (uint32_t idx = 0; idx < count; ++idx) {
                                if (off + 4 > spanv.size()) {
                                    break;
                                }
                                const uint32_t elen = read_le<uint32_t>(spanv.data() + off);
                                off += 4;

                                if (off + elen > spanv.size()) {
                                    break;
                                }

                                const uint8_t* ep = spanv.data() + off;
                                off += elen;

                                if constexpr (std::is_same_v<Elem, std::string> || non_std_string_like<Elem>) {
                                    if (elen < 2) {
                                        vec.emplace_back();
                                        continue;
                                    }
                                    const uint16_t slen = read_le<uint16_t>(ep);
                                    if (2u + slen > elen) {
                                        vec.emplace_back();
                                        continue;
                                    }
                                    const char* sp = reinterpret_cast<const char*>(ep + 2);

                                    if constexpr (std::is_same_v<Elem, std::string>) {
                                        vec.emplace_back(std::string{ sp, slen });
                                    }
                                    else if constexpr (std::is_constructible_v<Elem, std::string_view>) {
                                        vec.emplace_back(Elem(std::string_view{ sp, slen }));
                                    }
                                    else if constexpr (std::is_constructible_v<Elem, const char*>) {
                                        std::string tmp(sp, slen);
                                        vec.emplace_back(Elem(tmp.c_str()));
                                    }
                                    else if constexpr (requires(Elem & d, const char* s) { d = s; }) {
                                        std::string tmp(sp, slen);
                                        Elem v{};
                                        v = tmp.c_str();
                                        vec.emplace_back(std::move(v));
                                    }
                                    else {
                                        vec.emplace_back();
                                    }
                                }
                                else if constexpr (supported_basic_v<Elem>) {
                                    Elem v{};
                                    if (elen == sizeof(Elem)) {
                                        std::memcpy(std::addressof(v), ep, sizeof(Elem));
                                    }
                                    vec.emplace_back(std::move(v));
                                }
                                else if constexpr (reflectable_object<Elem>) {
                                    Elem v{};
                                    Deserialize(v, absl::Span<const uint8_t>(ep, elen));
                                    vec.emplace_back(std::move(v));
                                }
                            }
                        }
                    }
                    else if constexpr (reflectable_object<D>) {
                        if (tcode == TC_OBJECT) {
                            Deserialize(member_ref, spanv);
                        }
                    }
                    else {
                        const uint8_t tcode_expected = type_code<D>::value;
                        if (tcode == tcode_expected && spanv.size() == sizeof(MemberT)) {
                            std::memcpy(std::addressof(member_ref), spanv.data(), spanv.size());
                        }
                    }
                }
            }, obj);
        }

		private:
        template<typename U>
        static void append_le(std::vector<uint8_t>& out, U value) {
            static_assert(std::is_integral_v<U>, "append_le requires integral");
            for (size_t b = 0; b < sizeof(U); ++b)
                out.push_back(static_cast<uint8_t>((value >> (8 * b)) & 0xFF));
        }

        template<typename U>
        static U read_le(const uint8_t* p) {
            static_assert(std::is_integral_v<U>, "read_le requires integral");
            U v = 0;
            for (size_t b = 0; b < sizeof(U); ++b)
                v |= (U(p[b]) << (8 * b));
            return v;
        }
    };
}
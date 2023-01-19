#pragma once

#include "short_alloc.h"
#include "mmap.hpp"

#if (IS_CLANG==0)
#ifdef __has_include
    #if __has_include(<memory_resource>)
        #include <memory_resource>
    #endif
#endif
#endif

#if (IS_CLANG==0)
    #ifdef __cpp_lib_memory_resource
    #endif
#endif

#ifdef __cpp_lib_coroutine
    #include <coroutine>
#else
    #if defined (__cpp_impl_coroutine)
        #include <coroutine>
    #else
        #error "Your compiler must have full coroutine support."
    #endif
#endif

#include <optional>
#include <functional>
#include <filesystem>
#include <concepts>
#include <variant>

template <std::size_t N = 1000>
constexpr const std::size_t coroutine_arena_max_alloc = N;
static arena<coroutine_arena_max_alloc<>, alignof(std::max_align_t)> coroutine_arena;

static void* coro_alloc(size_t sz) noexcept {
    return coroutine_arena.template allocate<alignof(std::max_align_t)>(sz * sizeof(char));
}

static void coro_deallocate(void* ptr, size_t sz) noexcept {
    coroutine_arena.deallocate(reinterpret_cast<char*>(ptr), sz * sizeof(char));
}

namespace csv_co {

    using cell_string = std::basic_string<char, std::char_traits<char>,
#if (IS_MSVC==1)
            //std::pmr::polymorphic_allocator<char>
            std::allocator<char>
#else
            std::allocator<char>
#endif
    >;


    namespace trim_policy {
        struct no_trimming {
        public:
            static void trim (cell_string const &) {}
        };

        template <char const * list>
        struct trimming {
        public:
            static void trim (cell_string & s) {
                s.erase(0,s.find_first_not_of(list));
                s.erase(s.find_last_not_of(list)+1);
            }
        };
        static char const chars [] = " \t\r";
        using alltrim = trimming<chars>;
    }
    template <class T>
    concept TrimPolicyConcept = requires (T, cell_string s) {
        { T::trim(s) } -> std::convertible_to<void>; // TODO: more precise check required
    };

    template <char ch> struct quote_char {
        constexpr static char value = ch;
    };

    using double_quotes = quote_char<'"'>;

    template <class T>
    concept QuoteConcept = requires (T t) {
        { T::value } -> std::convertible_to<char>;
        { t } -> std::convertible_to<quote_char<T::value>>;
    };

    template <char ch> struct delimiter {
        constexpr static char value = ch;
    };

    using comma_delimiter = delimiter<','>;

    template <class T>
    concept DelimiterConcept = requires (T t) {
        { T::value } -> std::convertible_to<char>;
        { t } -> std::convertible_to<delimiter<T::value>>;
    };

    namespace string_functions {

        inline auto devastated(auto const & s) {
            return (s.find_first_not_of(" \n\r\t") == std::string::npos);
        }

        inline auto begins_with(auto const & s, char ch='"' ) -> std::pair<bool,std::size_t>
        {
            auto const d = s.find_first_not_of(" \n\r\t");
            return {(d != std::string::npos && s[d] == ch), d};
        }

        inline auto del_last (auto & source, char ch='"') {
            auto const pos = source.find_last_of(ch);
            assert (pos != std::string::npos);
            auto const sv = std::string_view (source.begin() + pos + 1, source.end());
            return devastated(std::decay_t<decltype(source)>{sv.begin(), sv.end()}) && (source.erase(pos, 1), true);
        }

        inline auto unquote (cell_string &s, char ch) {
            auto const [ret,pos] = begins_with(s, ch);
            if (ret && del_last(s, ch)) {
                s.erase(pos, 1);
            }
        }

        inline auto unique_quote (auto & s, char q) {
            auto const last = unique(s.begin(), s.end(), [q](auto const& first, auto const& second) {
                return first==q && second==q;
            });
            s.erase(last, s.end());
        }
    }

    template <TrimPolicyConcept TrimPolicy = trim_policy::no_trimming
            , QuoteConcept Quote = double_quotes
            , DelimiterConcept Delimiter = comma_delimiter>
    class reader {
        template<typename T, typename G, typename... Bases>
        struct promise_type_base : public Bases... {
            T mValue;

            auto yield_value(T value) {
                mValue = value;
                return std::suspend_always{};
            }

            G get_return_object() { return G{this}; };

            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void                return_void() {}
            void                unhandled_exception();

            void* operator new(size_t size) noexcept {
                return coro_alloc(size);
            }

            void operator delete(void* ptr, size_t size) {
                coro_deallocate(ptr, size);
            }

            static auto get_return_object_on_allocation_failure() {
                return G{nullptr};
            }
        };

        template<typename PT>
        struct coro_iterator {
            using coro_handle = std::coroutine_handle<PT>;

            coro_handle mCoroHdl{nullptr};
            bool        mDone{true};

            using RetType = decltype(mCoroHdl.promise().mValue);

            void resume() {
                mCoroHdl.resume();
                mDone = mCoroHdl.done();
            }

            coro_iterator() = default;

            coro_iterator(coro_handle hco) : mCoroHdl{hco} {
                resume();
            }

            coro_iterator& operator++() {
                resume();
                return *this;
            }

            bool operator==(const coro_iterator& o) const { return mDone == o.mDone; }
            const RetType& operator*() const { return mCoroHdl.promise().mValue; }
        };

        template<typename T>
        struct awaitable_promise_type_base {
            std::optional<T> mRecentSignal;

            struct awaiter {
                std::optional<T>& mRecentSignal;

                [[nodiscard]] bool await_ready() const { return mRecentSignal.has_value(); }
                void await_suspend(std::coroutine_handle<>) {}

                T await_resume() {
                    assert(mRecentSignal.has_value());
                    auto tmp = *mRecentSignal;
                    mRecentSignal.reset();
                    return tmp;
                }
            };

            [[nodiscard]] awaiter await_transform(T)
            { return awaiter{mRecentSignal}; }
        };

        template<typename T, typename U>
        struct [[nodiscard]] async_generator {
            using promise_type = promise_type_base<T,
                    async_generator,
                    awaitable_promise_type_base<U>>;
            using PromiseTypeHandle = std::coroutine_handle<promise_type>;

            T operator()() {
                auto tmp{std::move(mCoroHdl.promise().mValue)};
                if constexpr (
                        std::is_same_v<decltype(mCoroHdl.promise().mValue), std::optional<std::size_t>> ||
                        std::is_same_v<decltype(mCoroHdl.promise().mValue), std::optional<bool>> )
                    mCoroHdl.promise().mValue = std::nullopt;
                else
                    mCoroHdl.promise().mValue.clear();
                return tmp;
            }

            void send(U signal) {
                mCoroHdl.promise().mRecentSignal = signal;
                if(not mCoroHdl.done()) { mCoroHdl.resume(); }
            }

            async_generator(const async_generator&) = delete;
            async_generator(async_generator&& rhs) noexcept
                    : mCoroHdl{std::exchange(rhs.mCoroHdl, nullptr)} {}

            ~async_generator() {
                if(mCoroHdl) { mCoroHdl.destroy(); }
            }

        private:
            friend promise_type;
            explicit async_generator(promise_type* p) : mCoroHdl(PromiseTypeHandle::from_promise(*p)) {}

            PromiseTypeHandle mCoroHdl;
        };

        template<typename T>
        struct generator {
            using promise_type      = promise_type_base<T, generator>;
            using PromiseTypeHandle = std::coroutine_handle<promise_type>;
            using iterator          = coro_iterator<promise_type>;

            auto begin() -> iterator { return {mCoroHdl}; }
            auto end() -> iterator { return {}; }

            generator(generator const&) = delete;
            generator (generator&& rhs) noexcept : mCoroHdl(rhs.mCoroHdl) {
                rhs.mCoroHdl = nullptr;
            }

            ~generator() {
                if(mCoroHdl) { mCoroHdl.destroy(); }
            }

        private:
            friend promise_type;
            explicit generator(promise_type* p)
                    : mCoroHdl(PromiseTypeHandle::from_promise(*p)) {}

            PromiseTypeHandle mCoroHdl;
        };

        // Parsing State Machines:
        using FSM = async_generator<cell_string, char>;
        using FSM_cols = async_generator<std::optional<std::size_t>, char>;
        using FSM_rows = async_generator<std::optional<bool>, char>;
        class cell_span;
        using FSM_cell_span = async_generator<cell_span, char>;

        // Callback Types:
        using header_field_cb_t = std::function <void (std::string_view value)>;
        using value_field_cb_t = std::function <void (std::string_view value)>;
        using header_field_span_cb_t = std::function <void (cell_span const & span)>;
        using value_field_span_cb_t = std::function <void (cell_span const & span)>;
        using new_row_cb_t = std::function <void ()>;

        class cell_span {
        private:
            typename cell_string::const_pointer b = nullptr;
            typename cell_string::const_pointer e = nullptr;

            inline auto operator()() const noexcept -> bool { return (b != nullptr); }
            inline void clear () {b = nullptr;}

            friend auto reader::parse_cell_span() const noexcept -> FSM_cell_span;
            friend void reader::run_span(value_field_span_cb_t, new_row_cb_t) const;
            friend void reader::run_span(header_field_span_cb_t, value_field_span_cb_t, new_row_cb_t) const;
            template<typename T, typename U>
            friend struct async_generator;
        public:
            void read_value(auto & s) const {
                assert(b!=nullptr && e!=nullptr);
                using namespace string_functions;
                // A mangled result string in its guaranteed sufficient space
                s = std::decay_t<decltype(s)> { b,e };
                // If the field was (completely) quoted -> it must be unquoted
                unquote(s, Quote::value);
                // Fields partly quoted and not-quoted at all: must be spared from double quoting
                unique_quote(s, Quote::value);
                TrimPolicy::trim(s);
            }
        };

        static constexpr char LF{'\n'};

        [[nodiscard]] inline auto limiter(char b) const noexcept -> bool {
            return Delimiter::value == b || LF == b;
        }

        #define finalize_field(f) TrimPolicy::trim(f); \
                                  field.push_back(b);  \
                                  co_yield field;      \
                                  field.clear();

        auto parse() const -> FSM {
            cell_string field;
            for(;;) {
                if (auto b = co_await char{}; !limiter(b) && Quote::value != b) {
                    field += b;
                } else
                if (limiter(b)) {
                    finalize_field(field)
                } else {
                    using namespace string_functions;
                    bool was_devastated = devastated(field);
                    if (!was_devastated) {
                        // Extension: we allow partly double-quoted fields.
                        // So we leave these double quotes.
                        field += b;
                    }
                    unsigned quote_counter{1};
                    for(;;) {
                        b = co_await char{};
                        if (limiter(b) && !(quote_counter & 1)) {
                            if (was_devastated) {
                                del_last(field, Quote::value);
                            }
                            unique_quote(field, Quote::value);
                            finalize_field(field)
                            break;
                        }
                        quote_counter += (Quote::value == b) ? 1 : 0;
                        field += b;
                    }
                }
            }
        }

        auto parse_cell_span() const noexcept -> FSM_cell_span {
            cell_span noopt_span;

            std::visit([&](auto&& arg) {
                noopt_span.b = noopt_span.e = std::addressof(arg[0]);
            }, src );

            for(;;) {
                auto b = co_await char{};
                noopt_span.e++;
                if (limiter(b)) {
                    co_yield noopt_span;
                    noopt_span.b = noopt_span.e;
                    continue;
                }
                if (Quote::value == b) {
                    unsigned quote_counter {1};
                    for(;;) {
                        b = co_await char{};
                        noopt_span.e++;
                        if (limiter(b) && !(quote_counter & 1)) {
                            co_yield noopt_span;
                            noopt_span.b = noopt_span.e;
                            break;
                        }
                        quote_counter += (Quote::value == b) ? 1 : 0;
                    }
                }
            }
        }

        auto parse_cols() const noexcept -> FSM_cols {
            std::optional<std::size_t> cols = 0;
            for(;;) {
                auto b = co_await char{};
                if (limiter(b)) {
                    cols = cols.value() + 1;
                    if (LF == b) {
                        co_yield cols;
                        cols = 0;
                    }
                    continue;
                }
                if (Quote::value == b) {
                    unsigned quote_counter = 1;
                    for(;;) {
                        b = co_await char{};
                        if (limiter(b) && !(quote_counter & 1)) {
                            cols = cols.value() + 1;
                            if (LF == b) {
                                co_yield cols;
                                cols = 0;
                                break;
                            }
                        }
                        quote_counter += (Quote::value == b) ? 1 : 0;
                    }
                }
            }
        }

        auto parse_rows() const noexcept -> FSM_rows {
            std::optional<bool> line_end;
            for (;;) {
                auto b = co_await char{};
                if (limiter(b)) {
                    if (LF == b) {
                        line_end = true;
                        co_yield line_end;
                    }
                    continue;
                }
                if (Quote::value == b) {
                    unsigned quote_counter = 1;
                    for (;;) {
                        b = co_await char{};
                        if (limiter(b) && !(quote_counter & 1)) {
                            if (LF == b) {
                                line_end = true;
                                co_yield line_end;
                            }
                            break;
                        }
                        quote_counter += (Quote::value == b) ? 1 : 0;
                    }
                }
            }
        }

        using coroutine_stream_type = mio::ro_mmap::value_type;

        template <typename Range>
        auto sender(Range const & r) const -> generator<coroutine_stream_type> {
            for (auto e : r) {
                co_yield e;
            }
#if 0
            // TODO: after moving this is false:
            // But! object in "move-from state" - should not use it at all!
            assert(!r.empty());
#else
            if (!r.empty())
#endif
                if (LF != r.back()) {
                    co_yield '\n';
                }
        }

        template <typename Range>
        auto sender_span(Range const & r) const -> generator<coroutine_stream_type> {
            for (auto e : r) {
                co_yield e;
            }
        }
        template <typename Range>
        auto sender_span_LF(Range const &) const -> generator<coroutine_stream_type> {
            co_yield '\n';
        }

        std::variant<mio::ro_mmap, cell_string> src;

        // nullptr by default, or user-defined by run(). UB if nullptr
        mutable header_field_cb_t hf_cb;
        // always user-defined: by run() or UB if user-defined nullptr
        mutable value_field_cb_t  vf_cb;
        // defaulted or user-defined by run(), or UB if user-defined nullptr
        mutable new_row_cb_t new_row_cb;

        mutable header_field_span_cb_t hfcs_cb;
        mutable value_field_span_cb_t  vfcs_cb;

    public:
        using trim_policy_type = TrimPolicy;
        using quote_type = Quote;
        using delimiter_type = Delimiter;

        // TODO: stop calling for rvalue string...
        explicit reader(std::filesystem::path const & csv_src) : src {mio::ro_mmap {}} {
            std::error_code mmap_error;
            std::get<0>(src).map(csv_src.string().c_str(), mmap_error);
            if (mmap_error) {
                throw exception (mmap_error.message(), " : ", csv_src.string());
            }
        }

        template <template<class> class Alloc=std::allocator>
        explicit reader (std::basic_string<char, std::char_traits<char>, Alloc<char>> const & csv_src) : src {csv_src} {
            if (std::get<1>(src).empty()) {
                throw exception ("Argument cannot be empty");
            }
        }

        // let us express C-style string parameter constructor via usual string parameter constructor
        explicit reader (const char * csv_src) : reader(cell_string(csv_src)) {}

        reader (reader && other) noexcept = default;
        auto operator=(reader && other) noexcept -> reader & = default;

        [[nodiscard]] auto cols() const noexcept -> std::size_t {
            auto result {0};
            std::visit([&](auto&& arg) {
                auto source = sender(arg);
                auto p = parse_cols();
                for(const auto& b : source) {
                    p.send(b);
                    if (const auto& res = p(); res.has_value()) {
                        result = res.value();
                        return;
                    }
                }
            }, src);
            return result;
        }

        [[nodiscard]] auto rows() const noexcept -> std::size_t {
            auto rows {0};

            std::visit([&](auto&& arg) {
                auto source = sender(arg);
                auto p = parse_rows();

                for(const auto& b : source) {
                    p.send(b);
                    if (const auto& res = p(); res.has_value()) {
                        rows++;
                    }
                }
            }, src);
            return rows;
        }

        [[nodiscard]] auto valid() -> reader& {
            std::visit([&](auto&& arg) {
                auto result {false};
                std::optional<std::size_t> curr_cols;
                auto source = sender(arg);
                auto p = parse_cols();
                for(const auto& b : source) {
                    p.send(b);
                    if (const auto& res = p(); res.has_value()) {
                        if (curr_cols == std::nullopt) {
                            curr_cols = res.value();
                            result = true; // if no more lines but this - stay valid!
                        } else {
                            if (!(result = (curr_cols.value() == res.value()))) {
                                throw exception ("Incorrect CSV source format");
                            }
                        }
                    }
                }
                if (!result) {
                    throw exception ("Use of Move-From state object");
                }
            }, src);

            return *this;
        }

        void run(value_field_cb_t fcb, new_row_cb_t nrc=[]{}) const {
            vf_cb = std::move(fcb);
            new_row_cb = std::move(nrc);
            std::visit([&](auto&& arg) {
                auto source = sender(arg);
                auto p = parse();
                for (const auto &b: source) {
                    p.send(b);
                    if (const auto &res = p(); !res.empty()) {
                        vf_cb(std::string_view{res.begin(),res.end()-1});
                        if (LF == res.back()) {
                            new_row_cb();
                        }
                    }
                }
            }, src);
        }

        void run_span(value_field_span_cb_t fcb, new_row_cb_t nrc= [] {}) const {
            vfcs_cb = std::move(fcb);
            new_row_cb = std::move(nrc);
            std::visit([this](auto&& arg) noexcept {
                auto const range_end = std::addressof(arg[arg.size()]);
                auto source = sender_span(arg);
                auto p = parse_cell_span();
                for (auto const & b: source) {
                    p.send(b);
                    if (const auto & r = p(); r()) {
                        auto res = r;
                        res.e--;
                        vfcs_cb(res);
                        if (*res.e == LF) {
                            new_row_cb();
                        }
                    }
                }

                // In spanning mode last LF (if not in source) - gives no chance to dereference the source.
                // Because dereference would come to non-existent position: the end().
                // So we have to go for the trick. Otherwise, we would have to double-check for
                // every one field in the cycle above. (See revision history)

                if (arg.back() != LF) {
                    auto src_ = sender_span_LF(arg);
                    p.send(*(src_.begin()));
                    if (const auto &r = p(); r()) {
                        auto res = r;
                        res.e--;
                        vfcs_cb(res);
                        new_row_cb(); // Unconditionally
                    }
                }
            }, src);
        }

        void run(header_field_cb_t hfcb, value_field_cb_t fcb, new_row_cb_t nrc=[]{}) const {
            hf_cb = std::move(hfcb);
            vf_cb = std::move(fcb);
            new_row_cb = std::move(nrc);
            std::visit([&](auto&& arg) {
                auto columns = cols();
                auto source = sender(arg);
                auto p = parse();
                auto b = source.begin();
                while(columns) {
                    p.send(*b);
                    ++b;
                    if (const auto &res = p(); !res.empty()) {
                        hf_cb(std::string_view{res.begin(),res.end()-1});
                        columns--;
                    }
                }
                new_row_cb();

                while (b != source.end()) {
                    p.send(*b);
                    ++b;
                    if (const auto &res = p(); !res.empty()) {
                        vf_cb(std::string_view{res.begin(),res.end()-1});
                        if (LF == res.back()) {
                            new_row_cb();
                        }
                    }
                }
            }, src);
        }

        void run_span(header_field_span_cb_t hfcb, value_field_span_cb_t fcb, new_row_cb_t nrc= [] {}) const {
            hfcs_cb = std::move(hfcb);
            vfcs_cb = std::move(fcb);
            new_row_cb = std::move(nrc);
            std::visit([&](auto&& arg) {
                auto columns = cols();
                auto source = sender_span(arg);
                auto p = parse_cell_span();
                auto b = source.begin();
                while(columns) {
                    p.send(*b);
                    ++b;
                    if (auto res = p(); res()) {
                        res.e--;
                        hfcs_cb(res);
                        columns--;
                    }
                }
                new_row_cb();
                while (b != source.end()) {
                    p.send(*b);
                    ++b;
                    if (const auto &r = p(); r()) {
                        auto res = r;
                        res.e--;
                        vfcs_cb(res);
                        if(*(res.e) == LF) {
                            new_row_cb();
                        }
                    }
                }

                // In spanning mode last LF (if not in source) - gives no chance to dereference the source.
                // Because dereference would come to non-existent position: the end().
                // So we have to go for the trick. Otherwise, we would have to double-check for
                // every one field in the cycle above. (See revision history)

                if (arg.back() != LF) {
                    auto src_ = sender_span_LF(arg);
                    p.send(*src_.begin());
                    if (const auto &r = p(); r()) {
                        auto res = r;
                        res.e--;
                        vfcs_cb(res);
                        new_row_cb(); // Unconditionally
                    }
                }
            },src);
        }

        struct exception : public std::runtime_error {
            template <typename ... Types>
            explicit constexpr exception(Types ... args) : std::runtime_error("") {
                save_details(args...);
            }

            [[nodiscard]] constexpr auto what() const noexcept -> char const* override {
                return msg.c_str();
            }

        private:
            std::string msg;

            void save_details() noexcept {}

            template <class First, class ... Rest>
            void save_details(First && first, Rest && ... rest) {
                save_detail(std::forward<First>(first));
                save_details(std::forward<Rest>(rest)...);
            }

            template <typename T>
            void save_detail(T && v) {
                if constexpr(std::is_arithmetic_v<T>)
                    msg += std::to_string(v);
                else
                    msg += v;
            }
        };
    };

    template <TrimPolicyConcept TrimPolicy, QuoteConcept Quote, DelimiterConcept Delimiter>
    template<typename T, typename G, class ... Bases>
    void reader<TrimPolicy, Quote, Delimiter>::promise_type_base<T, G, Bases...>::
    unhandled_exception() {
        std::terminate();
    }

    static_assert(!std::is_copy_constructible<reader<>>::value);
    static_assert(std::is_move_constructible<reader<>>::value);

} // namespace

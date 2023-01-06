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

static void* coro_alloc(size_t sz) noexcept
{
    return coroutine_arena.template allocate<alignof(std::max_align_t)>(sz * sizeof(char));
}

static void coro_deallocate(void* ptr, size_t sz) noexcept
{
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
    concept TrimPolicyConcept = requires (T, cell_string s)
    {
        { T::trim(s) } -> std::convertible_to<void>; // TODO: more precise check required
    };

    template <char ch> struct quote_char {
        constexpr static char value = ch;
    };
    using quote = quote_char<'"'>;
    template <class T>
    concept QuoteConcept = requires (T t)
    {
        { T::value } -> std::convertible_to<char>;
        { t } -> std::convertible_to<quote_char<T::value>>;
    };

    template <char ch> struct delimiter {
        constexpr static char value = ch;
    };
    using comma_delimiter = delimiter<','>;
    template <class T>
    concept DelimiterConcept = requires (T t)
    {
        { T::value } -> std::convertible_to<char>;
        { t } -> std::convertible_to<delimiter<T::value>>;
    };

    namespace string_functions
    {

        inline bool devastated(auto const & s)
        {
            return (s.find_first_not_of(" \n\r\t") == std::string::npos);
        }

        inline std::pair<bool,std::size_t> begins_with(auto const & s, char ch='"' )
        {
            auto const d = s.find_first_not_of(" \n\r\t");
            return {(d != std::string::npos && s[d] == ch), d};
        }

        inline bool del_last (auto & source, char ch='"')
        {
            auto const pos = source.find_last_of(ch);
            assert (pos != std::string::npos);
            auto const sv = std::string_view (source.begin() + pos + 1, source.end());
            if (devastated(std::decay_t<decltype(source)>{sv.begin(), sv.end()}))
            {
                source.erase(pos,1);
                return true;
            }
            return false;
        }

        inline void unquote(cell_string &s, char ch)
        {
            auto const [ret,pos] = begins_with(s, ch);
            if ((ret) && del_last(s, ch))
            {
                s.erase(pos, 1);
            }
        }

        inline void unique_quote (auto & s, char q)
        {
            auto const last = unique(s.begin(), s.end(), [q](auto const& first, auto const& second)
            {
                return first==q && second==q;
            });
            s.erase(last, s.end());
        }

    }

    template <TrimPolicyConcept TrimPolicy = trim_policy::no_trimming
            , QuoteConcept Quote = quote, DelimiterConcept Delimiter = comma_delimiter>
    class reader
    {

        template<typename T, typename G,
                typename... Bases>
        struct promise_type_base : public Bases... {
            T mValue;

            auto yield_value(T value)
            {
                mValue = value;
                return std::suspend_always{};
            }

            G get_return_object() { return G{this}; };

            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void                return_void() {}
            void                unhandled_exception();


            void* operator new(size_t size) noexcept
            {
                return coro_alloc(size);
            }

            void operator delete(void* ptr, size_t size)
            {
                coro_deallocate(ptr, size);
            }

            static auto get_return_object_on_allocation_failure()
            {
                return G{nullptr};
            }
        };

        template<typename PT>
        struct coro_iterator {
            using coro_handle = std::coroutine_handle<PT>;

            coro_handle mCoroHdl{nullptr};
            bool        mDone{true};

            using RetType = decltype(mCoroHdl.promise().mValue);

            void resume()
            {
                mCoroHdl.resume();
                mDone = mCoroHdl.done();
            }

            coro_iterator() = default;

            coro_iterator(coro_handle hco)
                    : mCoroHdl{hco}
            {
                resume();
            }

            coro_iterator& operator++()
            {
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

                T await_resume()
                {
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
        struct [[nodiscard]] async_generator
        {
            using promise_type = promise_type_base<T,
                    async_generator,
                    awaitable_promise_type_base<U>>;
            using PromiseTypeHandle = std::coroutine_handle<promise_type>;

            T operator()()
            {
                auto tmp{std::move(mCoroHdl.promise().mValue)};
                if constexpr (
                        std::is_same_v<decltype(mCoroHdl.promise().mValue), std::optional<std::size_t>> ||
                        std::is_same_v<decltype(mCoroHdl.promise().mValue), std::optional<bool>> ||
                        std::is_same_v<decltype(mCoroHdl.promise().mValue), std::optional<cell_span>> )
                    mCoroHdl.promise().mValue = std::nullopt;
                else
                    mCoroHdl.promise().mValue.clear();
                return tmp;
            }

            void send(U signal)
            {
                mCoroHdl.promise().mRecentSignal = signal;
                if(not mCoroHdl.done()) { mCoroHdl.resume(); }
            }

            async_generator(const async_generator&) = delete;
            async_generator(async_generator&& rhs) noexcept
                    : mCoroHdl{std::exchange(rhs.mCoroHdl, nullptr)}
            {}

            ~async_generator()
            {
                if(mCoroHdl) { mCoroHdl.destroy(); }
            }

        private:
            friend promise_type;
            explicit async_generator(promise_type* p)
                    : mCoroHdl(PromiseTypeHandle::from_promise(*p))
            {}

            PromiseTypeHandle mCoroHdl;
        };

        template<typename T>
        struct generator {
            using promise_type      = promise_type_base<T, generator>;
            using PromiseTypeHandle = std::coroutine_handle<promise_type>;
            using iterator          = coro_iterator<promise_type>;

            iterator begin() { return {mCoroHdl}; }
            iterator end() { return {}; }

            generator(generator const&) = delete;
            generator (generator&& rhs) noexcept
                    : mCoroHdl(rhs.mCoroHdl)
            {
                rhs.mCoroHdl = nullptr;
            }

            ~generator()
            {
                if(mCoroHdl) { mCoroHdl.destroy(); }
            }

        private:
            friend promise_type; 
            explicit generator(promise_type* p)
                    : mCoroHdl(PromiseTypeHandle::from_promise(*p))
            {}

            PromiseTypeHandle mCoroHdl;
        };

        using FSM = async_generator<cell_string, char>;
        using FSM_cols = async_generator<std::optional<std::size_t>, char>;
        using FSM_rows = async_generator<std::optional<bool>, char>;

        class cell_span;
        using FSM_cell_span = async_generator<std::optional<cell_span>, char>;

        using header_field_callback_type = std::function <void (cell_string const & frame)>;
        using value_field_callback_type = std::function <void (cell_string const & frame)>;

        using header_field_cell_span_callback_type = std::function <void (cell_span const & frame)>;
        using value_field_cell_span_callback_type = std::function <void (cell_span const & frame)>;

        using new_row_callback_type = std::function <void ()>;

        class cell_span
        {
        private:
            typename cell_string::const_pointer b;
            typename cell_string::const_pointer e;
        public:
            cell_span (auto b, auto e) : b(b), e(e) {}
            void read_value(cell_string & s) const
            {
                using namespace string_functions;
                s = cell_string {b,e};
                std::cout <<  s << std::endl;
                // if field was quoted completely: it must be unquoted
                unquote(s, Quote::value);
                std::cout << s << std::endl;
                // fields partly quoted or not-quoted at all
                // (all must be spared from double quoting)
                unique_quote(s, Quote::value);
                std::cout << s << std::endl;
            }

            friend FSM_cell_span reader::parse_cell_span() const ;
            friend void reader::run_lazy(value_field_cell_span_callback_type, new_row_callback_type);

            cell_span (cell_span const &) = default;
            cell_span& operator= (cell_span const &) = default;
        };

        static constexpr char LF{'\n'};
        static constexpr char special{'\0'};

        [[nodiscard]] inline bool limiter(char b) const
        {
            return (Delimiter::value == b) || (LF == b);
        }

        inline void dirty_trick(auto & s) const
        {
            if (s.empty())
            {
                s.push_back(special);
            }
        }

        FSM parse() const
        {
            cell_string field;
            for(;;)
            {
                auto b = co_await char{};
                if (limiter(b))
                {
                    TrimPolicy::trim(field);
                    dirty_trick(field);
                    if (LF == b)
                    {
                        field.push_back(LF);
                    }
                    co_yield field;
                    field.clear();
                    continue;
                }
                if (Quote::value == b)
                {
                    using namespace string_functions;

                    bool was_devastated = devastated(field);
                    if (!was_devastated)
                    {
                        field.push_back(b);
                    }
                    std::size_t quote_counter = 1;
                    for(;;)
                    {
                        b = co_await char{};
                        if (!limiter(b))
                        {
                            field.push_back(b);
                            quote_counter += (Quote::value == b) ? 1 : 0;
                            continue;
                        }
                        if (quote_counter % 2)
                        {
                            field.push_back(b);
                            continue;
                        }
                        if (was_devastated)
                        {
                            del_last(field, Quote::value);
                        }
                        unique_quote(field, Quote::value);
                        TrimPolicy::trim(field);
                        dirty_trick(field);
                        if (LF == b)
                        {
                            field.push_back(LF);
                        }
                        co_yield field;
                        field.clear();
                        break;
                    }
                    continue;
                }
                field.push_back(b);
            }
        }

        FSM_cell_span parse_cell_span() const
        {
            std::optional<cell_span> span;

            std::visit([&](auto&& arg)
            {
                span = cell_span {std::addressof(arg[0]), std::addressof(arg[0])};
            },src );

            for(;;)
            {
                auto b = co_await char{};
                ++span->e;
                if (limiter(b))
                {
                    co_yield span;
                    span->b = span->e;
                    continue;
                }
                if (Quote::value == b)
                {
                    std::size_t quote_counter = 1;
                    for(;;)
                    {
                        b = co_await char{};
                        ++span->e;
                        if (!limiter(b))
                        {
                            quote_counter += (Quote::value == b) ? 1 : 0;
                            continue;
                        }
                        if (quote_counter % 2)
                        {
                            continue;
                        }
                        co_yield span;
                        span->b = span->e;
                        break;
                    }
                    continue;
                }
            }
        }

        FSM_cols parse_cols() const
        {
            std::optional<std::size_t> cols = 0;
            for(;;)
            {
                auto b = co_await char{};
                if (limiter(b))
                {
                    cols = cols.value() + 1;
                    if (LF == b)
                    {
                        co_yield cols;
                        cols = 0;
                    }
                    continue;
                }
                if (Quote::value == b)
                {
                    std::size_t quote_counter = 1;
                    for(;;)
                    {
                        b = co_await char{};
                        if (!limiter(b))
                        {
                            quote_counter += (Quote::value == b) ? 1 : 0;
                            continue;
                        }
                        if (quote_counter % 2)
                        {
                            continue;
                        }
                        cols = cols.value() + 1;
                        if (LF == b)
                        {
                            co_yield cols;
                            cols = 0;
                        }
                        break;
                    }
                    continue;
                }
            }
        }

        FSM_rows parse_rows() const
        {
            std::optional<bool> line_end = std::nullopt;
            for(;;)
            {
                auto b = co_await char{};
                if (limiter(b))
                {
                    if (LF == b)
                    {
                        line_end = true;
                        co_yield line_end;
                    }
                    continue;
                }
                if (Quote::value == b)
                {
                    std::size_t quote_counter = 1;
                    for(;;)
                    {
                        b = co_await char{};
                        if (!limiter(b))
                        {
                            quote_counter += (Quote::value == b) ? 1 : 0;
                            continue;
                        }
                        if (quote_counter % 2)
                        {
                            continue;
                        }

                        if (LF == b)
                        {
                            line_end = true;
                            co_yield line_end;
                        }
                        break;
                    }
                    continue;
                }
            }
        }

        using coroutine_stream_type = mio::ro_mmap::value_type;

        template <typename Range>
        generator<coroutine_stream_type> sender(Range const & r) const
        {
            for (auto e : r)
            {
                co_yield e;
            }
#if 0
            // TODO: after moving this is false:
            // But! object in "move-from state" - do not use it!
            // Make valid method exception throwing
            assert(!r.empty());
#else
            if (!r.empty())
#endif
            if (LF != r.back())
            {
                co_yield '\n';
            }
        }

        std::variant<mio::ro_mmap, cell_string> src;

        // nullptr by default, or used-default by run(). UB if nullptr
        header_field_callback_type hf_cb;
        // always user-defined: by run() or UB if user-defined nullptr
        value_field_callback_type  vf_cb;
        // defaulted or user-defined by run(), or UB if user-defined nullptr
        new_row_callback_type      new_row_callback_;

        header_field_cell_span_callback_type hfcs_cb;
        value_field_cell_span_callback_type  vfcs_cb;

    public:
        using trim_policy_type = TrimPolicy;
        using quote_type = Quote;
        using delimiter_type = Delimiter;

        // TODO: stop calling for rvalue string...
        explicit reader(std::filesystem::path const & fp) : src {mio::ro_mmap {}}
        {
            std::error_code mmap_error;
            std::get<0>(src).map(fp.string().c_str(), mmap_error);
            if (mmap_error)
            {
                throw exception ("Exception! ", mmap_error.message(),' ', fp.string());
            }
        }

        template <template<class> class Alloc=std::allocator>
        explicit reader (std::basic_string<char, std::char_traits<char>, Alloc<char>> const & s)
                : src {s}
        {
            if (std::get<1>(src).empty())
            {
                throw exception ("Exception! ", "Argument cannot be empty");
            }
        }

        // let us express C-style string parameter constructor via usual string parameter constructor
        explicit reader (const char * s) : reader(cell_string(s)) {}

        reader (reader const & other) = delete;
        reader & operator= (reader const & other) = delete;

        reader (reader && other) noexcept = default;
        reader & operator=(reader && other) noexcept = default;

        ~reader() = default;

        [[nodiscard]] std::size_t cols() const noexcept
        {
            auto result {0};
            std::visit([&](auto&& arg)
            {
                auto source = sender(arg);
                auto p = parse_cols();
                for(const auto& b : source)
                {
                    p.send(b);
                    if (const auto& res = p(); res.has_value())
                    {
                        result = res.value();
                        return;
                    }
                }
            }, src);
            return result;
        }

        [[nodiscard]] std::size_t rows() const noexcept
        {
            auto rows {0};

            std::visit([&](auto&& arg)
            {
                auto source = sender(arg);
                auto p = parse_rows();

                for(const auto& b : source)
                {
                    p.send(b);
                    if (const auto& res = p(); res.has_value())
                    {
                        rows++;
                    }
                }
            }, src);
            return rows;
        }

        [[nodiscard]] bool valid() const noexcept
        {
            auto result {false};
            std::visit([&](auto&& arg)
            {
                std::optional<std::size_t> curr_cols;
                auto source = sender(arg);
                auto p = parse_cols();
                for(const auto& b : source)
                {
                    p.send(b);
                    if (const auto& res = p(); res.has_value())
                    {
                        if (curr_cols == std::nullopt)
                        {
                            curr_cols = res.value();
                            result = true; // if no more lines but this - stay valid!
                        } else
                        {
                            if (!(result = (curr_cols.value() == res.value()))) { return;}
                        }
                    }
                }
            }, src);
            return result;
        }

        void run(value_field_callback_type fcb, new_row_callback_type nrc=[]{})
        {
            assert(!hf_cb);
            vf_cb = std::move(fcb);
            new_row_callback_ = std::move(nrc);
            std::visit([&](auto&& arg)
            {
                auto source = sender(arg);
                auto p = parse();
                for (const auto &b: source)
                {
                    p.send(b);
                    if (const auto &res = p(); !res.empty())
                    {
                        vf_cb(res.front()==special?(""):(res.back()!=LF?res:cell_string{res.begin(),res.end()-1}));
                        if (res.back()==LF)
                        {
                            new_row_callback_();
                        }
                    }
                }
            }, src);
        }

        void run_lazy(value_field_cell_span_callback_type fcb, new_row_callback_type nrc=[]{})
        {
            vfcs_cb = std::move(fcb);
            new_row_callback_ = std::move(nrc);
            std::visit([&](auto&& arg)
            {
                auto range_end = std::addressof(arg[arg.size()]);
                auto source = sender(arg);
                auto p = parse_cell_span();
                for (const auto &b: source)
                {
                    p.send(b);
                    if (auto res = p(); res.has_value())
                    {
                        if (--res->e < range_end)
                        {
                            assert(res->e < range_end);
                            auto const delim = *(res->e);
                            vfcs_cb(res.value());
                            if (delim == LF)
                            {
                                new_row_callback_();
                            }
                        } else // ultimate LF
                        {
                            assert(res->e == range_end);
                            vfcs_cb(res.value());
                            new_row_callback_();
                        }
                    }
                }
            }, src);
        }

        void run(header_field_callback_type hfcb, value_field_callback_type fcb, new_row_callback_type nrc=[]{})
        {
            hf_cb = std::move(hfcb);
            vf_cb = std::move(fcb);
            new_row_callback_ = std::move(nrc);
            std::visit([&](auto&& arg)
            {
                auto cols1 = cols();
                auto source = sender(arg);
                auto p = parse();
                auto b = source.begin();
                for (;;)
                {
                    p.send(*b);
                    ++b;
                    if (const auto &res = p(); !res.empty())
                    {
                        hf_cb(res.front()==special?(""):(res.back()!=LF?res:cell_string{res.begin(),res.end() - 1}));
                        --cols1;
                    }
                    if (!cols1)
                    {
                        new_row_callback_();
                        break;
                    }
                }
                while (b != source.end()) {
                    p.send(*b);
                    ++b;
                    if (const auto &res = p(); !res.empty())
                    {
                        vf_cb(res.front()==special?(""):(res.back()!=LF?res:cell_string{res.begin(),res.end() - 1}));

                        if (res.back() == LF)
                        {
                            new_row_callback_();
                        }
                    }
                }
            }, src);
        }

        struct exception : public std::runtime_error
        {
            template <typename ... Types>
            explicit constexpr exception(Types ... args) : std::runtime_error("")
            {
                save_details(args...);
            }

            [[nodiscard]] constexpr char const* what() const noexcept override
            {
              return msg.c_str();
            }

        private:
            std::string msg;

            void save_details() noexcept {}

            template <class First, class ... Rest>
            void save_details(First && first, Rest && ... rest)
            {
                save_detail(std::forward<First>(first));
                save_details(std::forward<Rest>(rest)...);
            }

            template <typename T>
            void save_detail(T && v)
            {
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
    unhandled_exception()
    {
        std::terminate();
    }

} // namespace




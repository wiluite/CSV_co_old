#pragma once

#include "short_alloc.h"

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
#include "mmap.hpp"
#include <functional>
#include <filesystem>

#include <cstring>

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

    using csv_field_string = std::basic_string<char, std::char_traits<char>,
#if (IS_GCC==0) || (IS_MSVC==1)
            std::pmr::polymorphic_allocator<char>
#else
            std::allocator<char>
#endif
            >;

    namespace trim_policy {
        struct no_trimming {
        public:
            static void trim (csv_field_string & s) {
                s.erase(0,0);
            }
        };

        template <char const * list>
        struct trimming {
        public:
            static void trim (csv_field_string & s) {
                s.erase(0,s.find_first_not_of(list));
                s.erase(s.find_last_not_of(list)+1);
            }
        };
    }

    template <char ch> struct delimiter {
        constexpr static char value = ch;
    };
    template <char ch> struct quote_char {
        constexpr static char value = ch;
    };

    namespace string_functions {
        inline void alltrim (auto & source)
        {
            source.erase(0,source.find_first_not_of(" \n\r\t"));
            source.erase(source.find_last_not_of(" \n\r\t") + 1);
        }

        inline bool is_vain( auto s)
        {
            alltrim(s);
            return s.empty();
        }

        template <class CHAR_T>
        inline void trim_right_if_last (auto & source, CHAR_T ch)
        {
            auto const pos = source.find_last_of(ch);
            assert (pos != std::string::npos);
            auto const sv = std::string_view (source.begin() + pos + 1, source.end());
            if (is_vain(std::decay_t<decltype(source)>{sv.begin(), sv.end()}))
                source.erase(pos,1);
        }

        template <typename Quote>
        inline void unique_quote (auto & s, Quote)
        {
            auto const last = unique(s.begin(), s.end(), [](auto const& first, auto const& second)
            {
                return first==Quote::value && second==Quote::value;
            });
            s.erase(last, s.end());
        }

    }
    template <typename TrimPolicy = trim_policy::no_trimming, typename Quote=quote_char<'"'>, typename Delimiter=delimiter<','> >
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
                return std::move(mCoroHdl.promise().mValue);
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

        using FSM = async_generator<csv_field_string, char>;

        static constexpr char LF{'\n'};

        [[nodiscard]] inline bool limiter(char b) const
        {
            return (Delimiter::value == b) || (LF == b);
        }

        inline void overcome_architectural_limitations(auto & s)
        {
            if (s.empty())
                s.push_back('\0');
        }

        FSM parse()
        {
            csv_field_string field;
            for(;;) {
                auto b = co_await char{};
                if (limiter(b))
                {
                    TrimPolicy::trim(field);
                    overcome_architectural_limitations(field);
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

                    bool vain = is_vain(field);
                    if (!vain)
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
                        if (vain)
                        {
                            trim_right_if_last(field, Quote::value);
                        }
                        unique_quote(field, Quote());
                        TrimPolicy::trim(field);
                        overcome_architectural_limitations(field);
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

        using coroutine_stream_type = mio::ro_mmap::value_type;

        template <typename Range>
        generator<coroutine_stream_type> sender(Range const & r)
        {
            for (auto e : r)
            {
                co_yield e;
            }
            if (LF != r[r.size()-1])
            {
                co_yield '\n';
            }
        }

        template <class Range>
        void run(Range const & r)
        {
            auto const begin = std::chrono::high_resolution_clock::now();

            auto source = sender(r);
            auto p = parse();
            for(const auto& b : source)
            {
                p.send(b);
                if (const auto& res = p(); !res.empty())
                {
                    if (res.front() == '\0' and res.size() == 1)
                    {
                        assert(res.size() == 1);
                        field_callback_("");
                    } else
                    {
                        field_callback_(res.back() != LF ? res : csv_field_string{res.begin(), res.end()-1});
                    }
                    if (res.back() == LF)
                    {
                        new_row_callback_();
                    }
                } else
                {
                    assert (res.length()==0);
                }
            }
            auto const end = std::chrono::high_resolution_clock::now();
            std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count() << " msecs" <<std::endl;
        }

        std::function <void (csv_field_string const & frame)> field_callback_;
        std::function <void ()> new_row_callback_;
        mio::ro_mmap mmap_;

    public:

        template <class FieldCallback = decltype(field_callback_), typename NewRowCallback=std::function <void ()>>
        explicit reader(std::filesystem::path const & fp, FieldCallback fcb=[](csv_field_string const & frame){}
                , NewRowCallback nrc=[]{}) : field_callback_(std::move(fcb)),  new_row_callback_(std::move(nrc))
        {
            std::error_code mmap_error;
            mmap_.map(fp.string().c_str(), mmap_error);
            if (mmap_error)
            {
                throw exception ("Exception! ", mmap_error.message(),' ', fp.string());
            }
            run(mmap_);
        }

        template <template<class> class Alloc=std::allocator,
                class FieldCallback = std::function <void (csv_field_string const & frame)>>
        explicit reader (std::basic_string<char, std::char_traits<char>, Alloc<char>> const & s,
                             FieldCallback field_callback=[](csv_field_string const & frame){})
                : field_callback_(std::move(field_callback))
        { run(s); }

        template <class FieldCallback, typename NewRowCallback=std::function <void ()>>
        explicit reader (const char * s, FieldCallback fcb=[](csv_field_string const & frame){}, NewRowCallback nrc=[]{})
                : field_callback_(std::move(fcb)), new_row_callback_(std::move(nrc))
        { run(csv_field_string(s)); }


        struct exception : public std::runtime_error
        {
            template <typename ... Types>
            constexpr exception(Types ... args) : std::runtime_error("")
            {
                save_details(args...);
            }

            [[nodiscard]] char const* what() const noexcept override
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

    template <class TrimPolicy, class Quote, class Delimiter>
    template<typename T, typename G, class ... Bases>
    void reader<TrimPolicy, Quote, Delimiter>::promise_type_base<T, G, Bases...>::
    unhandled_exception()
    {
        std::cout << "unhandled\n";
        std::terminate();
    }

} // namespace



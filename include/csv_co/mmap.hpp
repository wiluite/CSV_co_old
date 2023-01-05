/* This code initially belongs to:
 * Copyright 20xx-2022 https://github.com/mandreyel
 *
 * Copyright 2022 wiluite (verification, reduction and addition for special purposes)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef MIO_MMAP_HEADER
#define MIO_MMAP_HEADER

#include <iterator>
#include <string>
#include <system_error>
#include <cstdint>

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif // WIN32_LEAN_AND_MEAN
# include <Windows.h>
#else // ifdef _WIN32
# define INVALID_HANDLE_VALUE -1
#endif // ifdef _WIN32

namespace mio {

    enum { map_entire_file = 0 };

#ifdef _WIN32
    using file_handle_type = HANDLE;
#else
    using file_handle_type = int;
#endif

    const static file_handle_type invalid_handle = INVALID_HANDLE_VALUE;

    constexpr int read_access_mode = 0;

    struct ro_mmap
    {
        using value_type = char; //std::byte;
        using size_type = size_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using const_iterator = const_pointer;
        using handle_type = file_handle_type;

    private:
        // Points to the first requested byte, and not to the actual start of the mapping.
        pointer data_ = nullptr;
        size_type length_ = 0;
        size_type mapped_length_ = 0;

        handle_type file_handle_ = INVALID_HANDLE_VALUE;
#ifdef _WIN32
        handle_type file_mapping_handle_ = INVALID_HANDLE_VALUE;
#endif
        bool is_handle_internal_{};

    public:
        ro_mmap() = default;
        ro_mmap(const ro_mmap&) = delete;
        ro_mmap& operator=(const ro_mmap&) = delete;

        ro_mmap(ro_mmap && o) noexcept : ro_mmap()
        {
            Swap(o);
        }

        void Swap (ro_mmap & o) noexcept
        {
            std::swap(data_,o.data_);
            std::swap(length_,o.length_);
            std::swap(mapped_length_, o.mapped_length_);
            std::swap(file_handle_,o.file_handle_);
#ifdef _WIN32
            std::swap(file_mapping_handle_,o.file_mapping_handle_);
#endif
            std::swap(is_handle_internal_,o.is_handle_internal_);
        }

        ro_mmap& operator=(ro_mmap && o) noexcept
        {
            ro_mmap tmp(std::move(o));
            Swap(tmp);
            return *this;
        }

        ~ro_mmap();

        [[nodiscard]] handle_type file_handle() const noexcept { return file_handle_; }
        [[nodiscard]] bool is_open() const noexcept { return file_handle_ != invalid_handle; }
        [[nodiscard]] bool empty() const noexcept { return length() == 0; }
        [[nodiscard]] bool is_mapped() const noexcept;
        [[nodiscard]] size_type size() const noexcept { return length(); }
        [[nodiscard]] size_type length() const noexcept { return length_; }
        [[nodiscard]] size_type mapping_offset() const noexcept
        {
            return mapped_length_ - length_;
        }
        // length_ should be always greater than zero, because file is mapped
        [[nodiscard]] auto back() const noexcept {return data_[length_-1];};

        [[nodiscard]] const_pointer data() const noexcept { return data_; }

        [[nodiscard]] const_iterator begin() const noexcept { return data(); }
        [[nodiscard]] const_iterator end() const noexcept { return data() + length(); }

        reference operator[](const size_type i) noexcept { return data_[i]; }
        const_reference operator[](const size_type i) const noexcept { return data_[i]; }

        template<typename String>
        void map(const String& path, size_type offset,
                 size_type length, std::error_code& error);

        template<typename String>
        void map(const String& path, std::error_code& error)
        {
            map(path, 0, map_entire_file, error);
        }

        void map(handle_type handle, size_type offset,
                 size_type length, std::error_code& error);

        void unmap();

    private:
        [[nodiscard]] const_pointer get_mapping_start() const noexcept
        {
            return !data() ? nullptr : data() - mapping_offset();
        }
    };

} // namespace mio

// implementation details

#ifndef _WIN32
# include <unistd.h>
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
#else
#include <vector>
#endif

namespace mio {
    namespace detail {
        template<
                typename S,
                typename C = typename std::decay<S>::type,
                typename = decltype(std::declval<C>().data()),
                typename = typename std::enable_if<
                        std::is_same<typename C::value_type, char>::value
#ifdef _WIN32
                        || std::is_same<typename C::value_type, wchar_t>::value
#endif
                >::type
        > struct char_type_helper {
            using type = typename C::value_type;
        };

        template<class T>
        struct char_type {
            using type = typename char_type_helper<T>::type;
        };

// TODO: can we avoid this brute force approach?
        template<>
        struct char_type<char*> {
            using type = char;
        };

        template<>
        struct char_type<const char*> {
            using type = char;
        };

        template<typename CharT, typename S>
        struct is_c_str_helper
        {
            static constexpr bool value = std::is_same<
                    CharT*,
                    // TODO: I'm so sorry for this... Can this be made cleaner?
                    typename std::add_pointer<
                            typename std::remove_cv<
                                    typename std::remove_pointer<
                                            typename std::decay<
                                                    S
                                            >::type
                                    >::type
                            >::type
                    >::type
            >::value;
        };

        template<typename S>
        struct is_c_str
        {
            static constexpr bool value = is_c_str_helper<char, S>::value;
        };

#ifdef _WIN32
        template<typename S>
        struct is_c_wstr
        {
            static constexpr bool value = is_c_str_helper<wchar_t, S>::value;
        };
#endif // _WIN32

        template<typename S>
        struct is_c_str_or_c_wstr
        {
            static constexpr bool value = is_c_str<S>::value
#ifdef _WIN32
            || is_c_wstr<S>::value
#endif
            ;
        };

        template<
                typename String,
                typename = typename std::enable_if<is_c_str_or_c_wstr<String>::value>::type
        > const typename char_type<String>::type* c_str(String path)
        {
            return path;
        }

        template<
                typename String,
                typename = typename std::enable_if<is_c_str_or_c_wstr<String>::value>::type
        > bool empty(String path)
        {
            return !path || (*path == 0);
        }

#ifdef _WIN32
        namespace win {

            inline DWORD int64_high(int64_t n) noexcept
            {
                return n >> 32;
            }

            inline DWORD int64_low(int64_t n) noexcept
            {
                return n & 0xffffffff;
            }

            std::wstring s_2_ws(const std::string& value)
            {
                if (value.empty())
                    return{};
                const auto s_length = static_cast<int>(value.length());
                auto buf = std::vector<wchar_t>(s_length);
                const auto wide_char_count = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), s_length, buf.data(), s_length);
                return std::wstring(buf.data(), wide_char_count);
            }

            template<
                    typename String,
                    typename = typename std::enable_if<
                            std::is_same<typename char_type<String>::type, char>::value
                    >::type
            > file_handle_type open_file_helper(const String& path, const int )
            {
                return ::CreateFileW(s_2_ws(path).c_str(),
                                     GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     0,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     0);
            }
        } // win
#endif // _WIN32

        inline std::error_code last_error() noexcept
        {
            std::error_code error;
#ifdef _WIN32
            error.assign(GetLastError(), std::system_category());
#else
            error.assign(errno, std::system_category());
#endif
            return error;
        }

        template<typename String>
        file_handle_type open_file(const String& path, const int,
                                   std::error_code& error)
        {
            error.clear();
            if(detail::empty(path))
            {
                error = std::make_error_code(std::errc::invalid_argument);
                return invalid_handle;
            }
#ifdef _WIN32
            const auto handle = win::open_file_helper(path, read_access_mode);
#else // POSIX
            const auto handle = ::open(c_str(path), O_RDONLY);
#endif
            if(handle == invalid_handle)
            {
                error = detail::last_error();
            }
            return handle;
        }

        inline size_t query_file_size(file_handle_type handle, std::error_code& error)
        {
            error.clear();
#ifdef _WIN32
            LARGE_INTEGER file_size;
            if(::GetFileSizeEx(handle, &file_size) == 0)
            {
                error = detail::last_error();
                return 0;
            }
            return static_cast<int64_t>(file_size.QuadPart);
#else // POSIX
            struct stat sbuf;
            if(::fstat(handle, &sbuf) == -1)
            {
                error = detail::last_error();
                return 0;
            }
            return sbuf.st_size;
#endif
        }

        struct mmap_context
        {
            char* data;
            int64_t length;
            int64_t mapped_length;
#ifdef _WIN32
            file_handle_type file_mapping_handle;
#endif
        };

        inline size_t page_size()
        {
            static const size_t page_size = []
            {
#ifdef _WIN32
                SYSTEM_INFO SystemInfo;
                GetSystemInfo(&SystemInfo);
                return SystemInfo.dwAllocationGranularity;
#else
                return sysconf(_SC_PAGE_SIZE);
#endif
            }();
            return page_size;
        }

        inline size_t make_offset_page_aligned(size_t offset) noexcept
        {
            const size_t page_size_ = page_size();
            return offset / page_size_ * page_size_;
        }

        inline mmap_context memory_map(const file_handle_type file_handle, const int64_t offset,
                                       const int64_t length, const int /*access_mode mode*/, std::error_code& error)
        {
            const int64_t aligned_offset = make_offset_page_aligned(offset);
            const int64_t length_to_map = offset - aligned_offset + length;
#ifdef _WIN32
            const int64_t max_file_size = offset + length;
            const auto file_mapping_handle = ::CreateFileMapping(
                    file_handle,
                    nullptr,
                    PAGE_READONLY,
                    win::int64_high(max_file_size),
                    win::int64_low(max_file_size),
                    nullptr);
            if(file_mapping_handle == invalid_handle)
            {
                error = detail::last_error();
                return {};
            }
            char* mapping_start = static_cast<char*>(::MapViewOfFile(
                    file_mapping_handle,
                    FILE_MAP_READ,
                    win::int64_high(aligned_offset),
                    win::int64_low(aligned_offset),
                    length_to_map));
            if(mapping_start == nullptr)
            {
                // Close file handle if mapping it failed.
                ::CloseHandle(file_mapping_handle);
                error = detail::last_error();
                return {};
            }
#else // POSIX
            char* mapping_start = static_cast<char*>(::mmap(
                    nullptr, // Don't give hint as to where to map.
                    length_to_map,
                    PROT_READ,
                    MAP_SHARED,
                    file_handle,
                    aligned_offset));
            if(mapping_start == MAP_FAILED)
            {
                error = detail::last_error();
                return {};
            }
#endif
            mmap_context ctx {mapping_start + offset - aligned_offset, length, length_to_map
#ifdef _WIN32
                    ,
                              file_mapping_handle
#endif
            };
            return ctx;
        }

    } // namespace detail

// -- ro_mmap --

    ro_mmap::~ro_mmap()
    {
        unmap();
    }

    template<typename String>
    void ro_mmap::map(const String& path, const size_type offset,
                                         const size_type length, std::error_code& error)
    {
        error.clear();
        if(detail::empty(path))
        {
            error = std::make_error_code(std::errc::invalid_argument);
            return;
        }
        const auto handle = detail::open_file(path, read_access_mode, error);
        if(error)
        {
            return;
        }

        map(handle, offset, length, error);
        if(!error)
        {
            is_handle_internal_ = true;
        }
    }

    void ro_mmap::map(const handle_type handle,
                                         const size_type offset, const size_type length, std::error_code& error)
    {
        error.clear();
        if(handle == invalid_handle)
        {
            error = std::make_error_code(std::errc::bad_file_descriptor);
            return;
        }

        const auto file_size = detail::query_file_size(handle, error);
        if(error)
        {
            return;
        }

        if(offset + length > file_size)
        {
            error = std::make_error_code(std::errc::invalid_argument);
            return;
        }

        const auto ctx = detail::memory_map(handle, offset,
                                            length == map_entire_file ? (file_size - offset) : length,
                                            read_access_mode, error);
        if(!error)
        {
            unmap();
            file_handle_ = handle;
            is_handle_internal_ = false;
            data_ = reinterpret_cast<pointer>(ctx.data);
            length_ = ctx.length;
            mapped_length_ = ctx.mapped_length;
#ifdef _WIN32
            file_mapping_handle_ = ctx.file_mapping_handle;
#endif
        }
    }

    void ro_mmap::unmap()
    {
        if(!is_open()) { return; }
        // TODO do we care about errors here?
#ifdef _WIN32
        if(is_mapped())
        {
            ::UnmapViewOfFile(get_mapping_start());
            ::CloseHandle(file_mapping_handle_);
        }
#else // POSIX
        if(data_) { ::munmap(const_cast<pointer>(get_mapping_start()), mapped_length_); }
#endif

        if(is_handle_internal_)
        {
#ifdef _WIN32
            ::CloseHandle(file_handle_);
#else // POSIX
            ::close(file_handle_);
#endif
        }

        data_ = nullptr;
        length_ = mapped_length_ = 0;
        file_handle_ = invalid_handle;
#ifdef _WIN32
        file_mapping_handle_ = invalid_handle;
#endif
    }

    bool ro_mmap::is_mapped() const noexcept
    {
#ifdef _WIN32
        return file_mapping_handle_ != invalid_handle;
#else // POSIX
        return is_open();
#endif
    }

} // namespace mio


#endif // MIO_MMAP_HEADER

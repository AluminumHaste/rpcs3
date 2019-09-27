﻿#pragma once

#include <cstdint>
#include "Utilities/types.h"

namespace stx
{
	template <typename T, std::size_t Align = alignof(T), std::size_t Size = sizeof(T)>
	struct se_storage
	{
		struct type8
		{
			alignas(Align > alignof(T) ? alignof(T) : Align) unsigned char data[sizeof(T)];
		};

		struct type64
		{
			alignas(8) std::uint64_t data[sizeof(T) / 8];
		};

		using type = std::conditional_t<(Align >= 8 && sizeof(T) % 8 == 0), type64, type8>;

		// Possibly unoptimized generic byteswap for unaligned data
		static constexpr type swap(const type& src) noexcept;
	};

	template <typename T>
	struct se_storage<T, alignof(std::uint16_t), 2>
	{
		using type = std::uint16_t;

		static constexpr std::uint16_t swap(std::uint16_t src) noexcept
		{
#if defined(__GNUG__)
			return __builtin_bswap16(src);
#else
			return _byteswap_ushort(src);
#endif
		}
	};

	template <typename T>
	struct se_storage<T, alignof(std::uint32_t), 4>
	{
		using type = std::uint32_t;

		static constexpr std::uint32_t swap(std::uint32_t src) noexcept
		{
#if defined(__GNUG__)
			return __builtin_bswap32(src);
#else
			return _byteswap_ulong(src);
#endif
		}
	};

	template <typename T>
	struct se_storage<T, alignof(std::uint64_t), 8>
	{
		using type = std::uint64_t;

		static constexpr std::uint64_t swap(std::uint64_t src) noexcept
		{
#if defined(__GNUG__)
			return __builtin_bswap64(src);
#else
			return _byteswap_uint64(src);
#endif
		}
	};

	template <typename T, std::size_t Align, std::size_t Size>
	constexpr typename se_storage<T, Align, Size>::type se_storage<T, Align, Size>::swap(const type& src) noexcept
	{
		// Try to keep u16/u32/u64 optimizations at the cost of more bitcasts
		if constexpr (sizeof(T) == 1)
		{
			return src;
		}
		else if constexpr (sizeof(T) == 2)
		{
			return std::bit_cast<type>(se_storage<std::uint16_t>::swap(std::bit_cast<std::uint16_t>(src)));
		}
		else if constexpr (sizeof(T) == 4)
		{
			return std::bit_cast<type>(se_storage<std::uint32_t>::swap(std::bit_cast<std::uint32_t>(src)));
		}
		else if constexpr (sizeof(T) == 8)
		{
			return std::bit_cast<type>(se_storage<std::uint64_t>::swap(std::bit_cast<std::uint64_t>(src)));
		}
		else if constexpr (sizeof(T) % 8 == 0)
		{
			type64 tmp = std::bit_cast<type64>(src);
			type64 dst{};

			// Swap u64 blocks
			for (std::size_t i = 0; i < sizeof(T) / 8; i++)
			{
				dst.data[i] = se_storage<std::uint64_t>::swap(tmp.data[sizeof(T) / 8 - 1 - i]);
			}

			return std::bit_cast<type>(dst);
		}
		else
		{
			type dst{};

			// Swap by moving every byte
			for (std::size_t i = 0; i < sizeof(T); i++)
			{
				dst.data[i] = src.data[sizeof(T) - 1 - i];
			}

			return dst;
		}
	}

	// Endianness support template
	template <typename T, bool Swap, std::size_t Align = alignof(T)>
	class alignas(Align) se_t
	{
		using type = typename std::remove_cv<T>::type;
		using stype = typename se_storage<type, Align>::type;
		using storage = se_storage<type, Align>;

		stype m_data;

		static_assert(!std::is_pointer<type>::value, "se_t<> error: invalid type (pointer)");
		static_assert(!std::is_reference<type>::value, "se_t<> error: invalid type (reference)");
		static_assert(!std::is_array<type>::value, "se_t<> error: invalid type (array)");
		static_assert(sizeof(type) == alignof(type), "se_t<> error: unexpected alignment");

		static stype to_data(type value) noexcept
		{
			if constexpr (Swap)
			{
				return storage::swap(std::bit_cast<stype>(value));
			}
			else
			{
				return std::bit_cast<stype>(value);
			}
		}

	public:
		se_t() = default;

		se_t(const se_t& right) = default;

		se_t(type value) noexcept
			: m_data(to_data(value))
		{
		}

		type value() const noexcept
		{
			if constexpr (Swap)
			{
				return std::bit_cast<type>(storage::swap(m_data));
			}
			else
			{
				return std::bit_cast<type>(m_data);
			}
		}

		type get() const noexcept
		{
			return value();
		}

		se_t& operator=(const se_t&) = default;

		se_t& operator=(type value) noexcept
		{
			m_data = to_data(value);
			return *this;
		}

		using simple_type = simple_t<T>;

		operator type() const noexcept
		{
			return value();
		}

#ifdef _MSC_VER
		explicit operator bool() const noexcept
		{
			static_assert(!type{});
			static_assert(!std::is_floating_point_v<type>);
			return !!std::bit_cast<type>(m_data);
		}
#endif

		template <typename T1>
		se_t& operator+=(const T1& rhs)
		{
			*this = value() + rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator-=(const T1& rhs)
		{
			*this = value() - rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator*=(const T1& rhs)
		{
			*this = value() * rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator/=(const T1& rhs)
		{
			*this = value() / rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator%=(const T1& rhs)
		{
			*this = value() % rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator&=(const T1& rhs)
		{
			*this = value() & rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator|=(const T1& rhs)
		{
			*this = value() | rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator^=(const T1& rhs)
		{
			*this = value() ^ rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator<<=(const T1& rhs)
		{
			*this = value() << rhs;
			return *this;
		}

		template <typename T1>
		se_t& operator>>=(const T1& rhs)
		{
			*this = value() >> rhs;
			return *this;
		}

		se_t& operator++()
		{
			T value = *this;
			*this = ++value;
			return *this;
		}

		se_t& operator--()
		{
			T value = *this;
			*this = --value;
			return *this;
		}

		T operator++(int)
		{
			T value = *this;
			T result = value++;
			*this = value;
			return result;
		}

		T operator--(int)
		{
			T value = *this;
			T result = value--;
			*this = value;
			return result;
		}
	};
}
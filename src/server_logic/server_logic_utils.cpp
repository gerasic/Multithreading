#include "server_logic_impl.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <compare>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace server_logic {

namespace {

std::int64_t saturating_duration_cast_ms(const std::chrono::system_clock::duration duration) {
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return std::clamp<std::int64_t>(milliseconds, 0, std::numeric_limits<std::int64_t>::max());
}

std::uint32_t rotate_right(const std::uint32_t value, const std::uint32_t shift) {
    return (value >> shift) | (value << (32U - shift));
}

std::string sha256_hex(std::string_view input) {
    constexpr std::array<std::uint32_t, 64> k{
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
        0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
        0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
        0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
        0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
        0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
        0xc67178f2U,
    };
    std::array<std::uint32_t, 8> hash{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };

    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const auto bit_length = static_cast<std::uint64_t>(bytes.size()) * 8U;
    bytes.push_back(0x80U);
    while ((bytes.size() % 64U) != 56U) {
        bytes.push_back(0U);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    }

    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t offset = 0; offset < bytes.size(); offset += 64U) {
        for (std::size_t i = 0; i < 16U; ++i) {
            const auto index = offset + i * 4U;
            schedule[i] = (static_cast<std::uint32_t>(bytes[index]) << 24U) |
                          (static_cast<std::uint32_t>(bytes[index + 1U]) << 16U) |
                          (static_cast<std::uint32_t>(bytes[index + 2U]) << 8U) |
                          static_cast<std::uint32_t>(bytes[index + 3U]);
        }
        for (std::size_t i = 16U; i < 64U; ++i) {
            const auto s0 = rotate_right(schedule[i - 15U], 7U) ^ rotate_right(schedule[i - 15U], 18U) ^
                            (schedule[i - 15U] >> 3U);
            const auto s1 = rotate_right(schedule[i - 2U], 17U) ^ rotate_right(schedule[i - 2U], 19U) ^
                            (schedule[i - 2U] >> 10U);
            schedule[i] = schedule[i - 16U] + s0 + schedule[i - 7U] + s1;
        }

        auto a = hash[0];
        auto b = hash[1];
        auto c = hash[2];
        auto d = hash[3];
        auto e = hash[4];
        auto f = hash[5];
        auto g = hash[6];
        auto h = hash[7];

        for (std::size_t i = 0; i < 64U; ++i) {
            const auto s1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
            const auto ch = (e & f) ^ (~e & g);
            const auto temp1 = h + s1 + ch + k[i] + schedule[i];
            const auto s0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
            const auto maj = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    std::ostringstream out;
    out << std::hex << std::nouppercase;
    for (const auto value : hash) {
        out.width(8);
        out.fill('0');
        out << value;
    }
    return out.str();
}

}  // namespace

ServerError::ServerError(ErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

ErrorCode ServerError::code() const noexcept {
    return code_;
}

ServerLogic::Impl::Impl(ServerConfig config_value) : config(std::move(config_value)) {}

ServerLogic::Impl::~Impl() = default;

bool ServerLogic::Impl::is_valid_login(std::string_view value) const {
    if (value.empty() || value.size() > config.max_login_length) {
        return false;
    }
    return std::ranges::all_of(value, [](const unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
    });
}

bool ServerLogic::Impl::is_valid_password(std::string_view value) const {
    return value.size() >= config.min_password_length && value.size() <= config.max_password_length;
}

std::string ServerLogic::Impl::hash_password(std::string_view login, std::string_view password) {
    std::string material;
    material.reserve(login.size() + password.size() + 24U);
    material.append("multithreading-server:");
    material.append(login);
    material.push_back(':');
    material.append(password);
    return sha256_hex(material);
}

std::uint64_t ServerLogic::Impl::now_ms() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(saturating_duration_cast_ms(now));
}

}  // namespace server_logic

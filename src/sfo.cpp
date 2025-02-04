#include "sfo.hpp"

#include <fmt/format.h>

#include <stdexcept>

static constexpr uint32_t SFO_MAGIC = 0x46535000;

struct SfoHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t keyofs;
    uint32_t valofs;
    uint32_t count;
} __attribute__((packed));

struct SfoEntry
{
    uint16_t nameofs;
    uint8_t alignment;
    uint8_t type;
    uint32_t valsize;
    uint32_t totalsize;
    uint32_t dataofs;
} __attribute__((packed));

std::string pkgi_sfo_get_string(
        const uint8_t* buffer, size_t size, const std::string& name)
{
    if (size < sizeof(SfoHeader))
        throw std::runtime_error("param.sfo truncado");

    const SfoHeader* header = reinterpret_cast<const SfoHeader*>(buffer);
    const SfoEntry* entries =
            reinterpret_cast<const SfoEntry*>(buffer + sizeof(SfoHeader));

    if (header->magic != SFO_MAGIC)
        throw std::runtime_error("Imposible parsear SFO, magia invalida");

    if (size < sizeof(SfoHeader) + header->count * sizeof(SfoEntry))
        throw std::runtime_error("param.sfo truncado");

    for (uint32_t i = 0; i < header->count; i++)
        if (std::string(reinterpret_cast<const char*>(
                    buffer + header->keyofs + entries[i].nameofs)) == name)
            return std::string(reinterpret_cast<const char*>(
                    buffer + header->valofs + entries[i].dataofs));

    return {};
}

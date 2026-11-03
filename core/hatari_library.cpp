#include "hatari_library.h"

namespace kairost {

std::unique_ptr<HatariLibrary> HatariLibrary::load(std::string &error) {
    auto library = kairo::FreshLibrary::load(KAIROST_HATARI_FILE, error);
    if (!library) return nullptr;
    auto entry = reinterpret_cast<const KairoStApi *(*)()>(library->symbol(KAIRO_ST_API_SYMBOL));
    if (!entry) {
        error = std::string(KAIROST_HATARI_FILE) + " has no " + KAIRO_ST_API_SYMBOL;
        return nullptr;
    }
    const KairoStApi *api = entry();
    return std::unique_ptr<HatariLibrary>(new HatariLibrary(std::move(library), api));
}

} // namespace kairost

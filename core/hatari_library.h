// A fresh copy of the Hatari library (kairost_hatari, core/kairo/kairo_st.h) for one machine.
//
// Hatari keeps its state in process-wide variables that only a new process resets. Loading a new
// copy of the library gives each machine those variables as a new Hatari process has them, and
// unloading it discards whatever the machine left behind (the shared kairo/fresh_library.h).

#pragma once

#include <memory>
#include <string>

#include "kairo/fresh_library.h"
#include "kairo_st.h"

namespace kairost {

class HatariLibrary {
public:
    // A new copy of the library; null with error set when it cannot be loaded.
    static std::unique_ptr<HatariLibrary> load(std::string &error);

    HatariLibrary(const HatariLibrary &) = delete;
    HatariLibrary &operator=(const HatariLibrary &) = delete;

    const KairoStApi &api() const { return *api_; }

private:
    HatariLibrary(std::unique_ptr<kairo::FreshLibrary> library, const KairoStApi *api)
        : library_(std::move(library)), api_(api) {}

    // Unloaded with this object: its machine's thread must have ended.
    std::unique_ptr<kairo::FreshLibrary> library_;
    const KairoStApi *api_;
};

} // namespace kairost

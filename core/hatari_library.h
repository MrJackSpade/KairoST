// A fresh copy of the Hatari library (kairost_hatari, core/kairo/kairo_st.h) for one machine.
//
// Hatari keeps its state in process-wide variables that only a new process resets. Loading a new
// copy of the library gives each machine those variables as a new Hatari process has them, and
// unloading it discards whatever the machine left behind. On Android the library is loaded again
// even though a copy of it is already loaded (ANDROID_DLEXT_FORCE_LOAD); on Windows, where a
// module loads once per path, from a copy of the file made for this machine.

#pragma once

#include <memory>
#include <string>

#include "kairo_st.h"

namespace kairost {

class HatariLibrary {
public:
    // A new copy of the library; null with error set when it cannot be loaded.
    static std::unique_ptr<HatariLibrary> load(std::string &error);
    // Unloads the copy: its machine's thread must have ended.
    ~HatariLibrary();

    HatariLibrary(const HatariLibrary &) = delete;
    HatariLibrary &operator=(const HatariLibrary &) = delete;

    const KairoStApi &api() const { return *api_; }

private:
    HatariLibrary(void *handle, const KairoStApi *api, std::string copy)
        : handle_(handle), api_(api), copy_(std::move(copy)) {}

    void *handle_;
    const KairoStApi *api_;
    // The file this copy was loaded from, when it was made for it (Windows); "" otherwise.
    std::string copy_;
};

} // namespace kairost

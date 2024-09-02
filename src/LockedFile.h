#pragma once

#include <cstdint>
#include <string>
#include "FileUtil.h"

namespace utils {

class LockedFile final : public File {
public:
    enum LockMode : uint32_t
    {
        NoLock = 0,
        ReadLock,
        WriteLock,
    };

    LockedFile() = default;
    LockedFile( const std::string &name );
    ~LockedFile();

    bool Open( OpenMode mode ) override;

    bool Lock( LockMode mode, bool is_block = true );

    bool Unlock();

    bool IsLocked() const;

    LockMode GetLockMode() const;

private:
    LockMode lock_mode_{ LockMode::NoLock };
};

}  // namespace utils
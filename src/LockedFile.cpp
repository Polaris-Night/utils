#include "LockedFile.h"

#include "Macros.h"

#if PLATFORM_OS_LINUX
    #include <errno.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace utils {

LockedFile::LockedFile( const std::string &name ) : File( name ) {}

LockedFile::~LockedFile() {}

bool LockedFile::Open( OpenMode mode ) {
    // 不能以清空方式打开，否则锁没有意义
    if ( EnumContainsBit( mode, OpenMode::Truncate ) ) {
        return false;
    }
    return File::Open( mode );
}

bool LockedFile::Lock( LockMode mode, bool is_block ) {
    if ( !IsOpened() ) {
        return false;
    }
    // 设置为无锁时直接解锁
    if ( mode == LockMode::NoLock ) {
        return Unlock();
    }
    // 状态相同时直接返回
    if ( mode == lock_mode_ ) {
        return true;
    }
    // 当前已上锁时先解锁再设置新状态
    if ( IsLocked() ) {
        Unlock();
    }
#if PLATFORM_OS_LINUX
    // 使用flock锁定
    struct flock fl;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    fl.l_type   = ( mode == ReadLock ) ? F_RDLCK : F_WRLCK;
    int cmd     = is_block ? F_SETLKW : F_SETLK;
    int ret     = fcntl( Handle(), cmd, &fl );
    if ( ret == -1 && errno != EINTR && errno != EAGAIN ) {
        return false;
    }
#endif
    lock_mode_ = mode;
    return true;
}

bool LockedFile::Unlock() {
    if ( !IsOpened() ) {
        return false;
    }
    if ( !IsLocked() ) {
        return true;
    }
#if PLATFORM_OS_LINUX
    struct flock fl;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    fl.l_type   = F_UNLCK;
    int ret     = fcntl( Handle(), F_SETLKW, &fl );

    if ( ret == -1 ) {
        return false;
    }
#endif
    lock_mode_ = NoLock;
    return true;
}

bool LockedFile::IsLocked() const {
    return lock_mode_ != LockMode::NoLock;
}

LockedFile::LockMode LockedFile::GetLockMode() const {
    return lock_mode_;
}

}  // namespace utils
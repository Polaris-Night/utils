#include <chrono>
#include <string>
#include <thread>
#include "FileUtil.h"
#include "LockedFile.h"
#include "Macros.h"
#include "gtest/gtest.h"

#if defined( PLATFORM_OS_LINUX )
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace utils;

TEST( LockedFileTest, BasicLockUnlock ) {
    std::string path = FileUtil::CreateTempFile( "locked", ".txt" );
    ASSERT_FALSE( path.empty() );

    LockedFile file( path );
    ASSERT_TRUE( file.Open( LockedFile::ReadWrite ) );

    EXPECT_FALSE( file.IsLocked() );
    EXPECT_EQ( file.GetLockMode(), LockedFile::NoLock );

    EXPECT_TRUE( file.Lock( LockedFile::WriteLock ) );
    EXPECT_TRUE( file.IsLocked() );
    EXPECT_EQ( file.GetLockMode(), LockedFile::WriteLock );

    // 重复加同一把锁直接成功
    EXPECT_TRUE( file.Lock( LockedFile::WriteLock ) );

    EXPECT_TRUE( file.Unlock() );
    EXPECT_FALSE( file.IsLocked() );

    // 改为读锁
    EXPECT_TRUE( file.Lock( LockedFile::ReadLock ) );
    EXPECT_EQ( file.GetLockMode(), LockedFile::ReadLock );

    // 传 NoLock 等价于解锁
    EXPECT_TRUE( file.Lock( LockedFile::NoLock ) );
    EXPECT_FALSE( file.IsLocked() );

    file.Close();
    FileUtil::Remove( path );
}

TEST( LockedFileTest, LockWithoutOpenFails ) {
    LockedFile file( "not_opened_locked_file.txt" );
    EXPECT_FALSE( file.Lock( LockedFile::WriteLock ) );
    EXPECT_FALSE( file.Unlock() );
}

TEST( LockedFileTest, TruncateOpenRejected ) {
    std::string path = FileUtil::CreateTempFile( "locked_trunc", ".txt" );
    ASSERT_FALSE( path.empty() );

    LockedFile file( path );
    EXPECT_FALSE( file.Open( LockedFile::Truncate | LockedFile::WriteOnly ) );
    EXPECT_FALSE( file.IsOpened() );

    FileUtil::Remove( path );
}

#if defined( PLATFORM_OS_LINUX )
TEST( LockedFileTest, CrossProcessNonBlockingLock ) {
    std::string path = FileUtil::CreateTempFile( "locked_cross", ".txt" );
    ASSERT_FALSE( path.empty() );

    int pipefd[2];
    ASSERT_EQ( pipe( pipefd ), 0 );

    pid_t pid = fork();
    ASSERT_GE( pid, 0 );

    if ( pid == 0 ) {
        // 子进程：持有写锁后通知父进程，并保持 300ms
        close( pipefd[0] );

        LockedFile child( path );
        bool       opened = child.Open( LockedFile::ReadWrite );
        bool       locked = opened && child.Lock( LockedFile::WriteLock, true );

        char    notification = locked ? '1' : '0';
        ssize_t ignored      = write( pipefd[1], &notification, 1 );
        (void)ignored;

        std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
        if ( locked ) {
            child.Unlock();
        }
        child.Close();
        close( pipefd[1] );
        _exit( 0 );
    }

    // 父进程：等待子进程加锁完成
    close( pipefd[1] );
    char    notification = 0;
    ssize_t n            = read( pipefd[0], &notification, 1 );
    EXPECT_EQ( n, 1 );
    ASSERT_EQ( notification, '1' );  // 子进程必须成功加锁
    close( pipefd[0] );

    LockedFile parent( path );
    ASSERT_TRUE( parent.Open( LockedFile::ReadWrite ) );

    // 回归测试：非阻塞加锁遇到他人已持有的锁必须失败（原实现把 EAGAIN 当作成功）
    EXPECT_FALSE( parent.Lock( LockedFile::WriteLock, false ) );
    EXPECT_FALSE( parent.IsLocked() );

    // 阻塞加锁：等待子进程释放后应成功
    EXPECT_TRUE( parent.Lock( LockedFile::WriteLock, true ) );
    EXPECT_TRUE( parent.IsLocked() );
    EXPECT_TRUE( parent.Unlock() );

    parent.Close();

    int status = 0;
    waitpid( pid, &status, 0 );
    FileUtil::Remove( path );
}
#endif

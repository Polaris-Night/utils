#include <string>
#include "FileUtil.h"
#include "Macros.h"
#include "TarUtil.h"
#include "gtest/gtest.h"

using namespace utils;

TEST( TarUtilTest, ObjectCompressAndDecompress ) {
#if defined( PLATFORM_OS_LINUX ) || defined( PLATFORM_OS_OSX )
    std::string work_dir = FileUtil::CreateTempDirectory( "tarutil" );
    ASSERT_FALSE( work_dir.empty() );

    std::string out_dir = FileUtil::JoinPaths( work_dir, "out" );
    ASSERT_TRUE( FileUtil::CreateDirectories( out_dir ) );

    // 文件名包含空格，回归验证参数安全引用（避免 shell 拆分/注入）
    std::string file_a = FileUtil::JoinPaths( out_dir, "a.txt" );
    std::string file_b = FileUtil::JoinPaths( out_dir, "b b.txt" );
    ASSERT_TRUE( FileUtil::WriteStr( file_a, "content-a" ) );
    ASSERT_TRUE( FileUtil::WriteStr( file_b, "content-b" ) );

    std::string tar_path = FileUtil::JoinPaths( work_dir, "test.tar.gz" );

    TarObject tar( tar_path, { file_a, file_b } );
    EXPECT_EQ( tar.GetFileCount(), 2u );

    auto compress_result = tar.Compress();
    EXPECT_TRUE( compress_result.success ) << compress_result.message;
    EXPECT_TRUE( tar.TarExist() );

    // 解压到不存在的目标目录（应自动创建）
    std::string dec_dir    = FileUtil::JoinPaths( work_dir, "dec" );
    auto        dec_result = TarUtil::Decompress( tar_path, dec_dir );
    EXPECT_TRUE( dec_result.success ) << dec_result.message;

    // 压缩绝对路径时使用 -C + 文件名，解压后文件位于 dec_dir 下
    std::string dec_a = FileUtil::JoinPaths( dec_dir, "a.txt" );
    std::string dec_b = FileUtil::JoinPaths( dec_dir, "b b.txt" );
    EXPECT_TRUE( FileUtil::Exists( dec_a ) );
    EXPECT_TRUE( FileUtil::Exists( dec_b ) );
    EXPECT_EQ( FileUtil::Load2Str( dec_a ), "content-a" );
    EXPECT_EQ( FileUtil::Load2Str( dec_b ), "content-b" );

    FileUtil::RemoveAll( work_dir );
#else
    GTEST_SKIP() << "tar tests only run on POSIX platforms";
#endif
}

TEST( TarUtilTest, StaticCompressAndDecompress ) {
#if defined( PLATFORM_OS_LINUX ) || defined( PLATFORM_OS_OSX )
    std::string work_dir = FileUtil::CreateTempDirectory( "tarutil_static" );
    ASSERT_FALSE( work_dir.empty() );

    std::string file_a = FileUtil::JoinPaths( work_dir, "x.txt" );
    ASSERT_TRUE( FileUtil::WriteStr( file_a, "x-content" ) );

    std::string tar_path = FileUtil::JoinPaths( work_dir, "static.tar.gz" );

    // 回归测试：曾使用 -czfP 组合写法，GNU tar 会把 "P" 当作归档文件名导致失败
    auto result = TarUtil::Compress( tar_path, { file_a } );
    EXPECT_TRUE( result.success ) << result.message;
    EXPECT_TRUE( FileUtil::Exists( tar_path ) );

    std::string dec_dir    = FileUtil::JoinPaths( work_dir, "dec" );
    auto        dec_result = TarUtil::Decompress( tar_path, dec_dir );
    EXPECT_TRUE( dec_result.success ) << dec_result.message;
    EXPECT_EQ( FileUtil::Load2Str( FileUtil::JoinPaths( dec_dir, "x.txt" ) ), "x-content" );

    FileUtil::RemoveAll( work_dir );
#else
    GTEST_SKIP() << "tar tests only run on POSIX platforms";
#endif
}

TEST( TarUtilTest, InvalidInputs ) {
    // 空归档路径
    auto compress_result = TarUtil::Compress( "", { "/tmp/whatever.txt" } );
    EXPECT_FALSE( compress_result.success );
    EXPECT_FALSE( compress_result.message.empty() );

    auto decompress_result = TarUtil::Decompress( "", "" );
    EXPECT_FALSE( decompress_result.success );
    EXPECT_FALSE( decompress_result.message.empty() );

    // 归档文件不存在
    auto missing_result = TarUtil::Decompress( "/tmp/definitely_not_exists_tar_file.tar.gz", "" );
    EXPECT_FALSE( missing_result.success );
    EXPECT_FALSE( missing_result.message.empty() );

    // TarObject 空文件列表
    TarObject empty_tar( "/tmp/empty_tar_object.tar.gz", {} );
    auto      empty_result = empty_tar.Compress();
    EXPECT_FALSE( empty_result.success );
    EXPECT_FALSE( empty_result.message.empty() );
    EXPECT_FALSE( empty_tar.TarExist() );
}

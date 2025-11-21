#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "FileUtil.h"
#include "gtest/gtest.h"

using namespace utils;

class FileUtilTest : public ::testing::Test {
public:
    ~FileUtilTest() override {
        // 删除测试目录
        if ( !test_dir_.empty() ) {
            FileUtil::RemoveAll( test_dir_ );
        }
    }

protected:
    void SetUp() override {
        // 创建测试用的临时目录
        test_dir_ = FileUtil::CreateTempDirectory( "fileutil_test" );
        ASSERT_FALSE( test_dir_.empty() );
    }

    void TearDown() override {
        // 清理测试目录
        if ( !test_dir_.empty() ) {
            FileUtil::RemoveAll( test_dir_ );
        }
    }

    std::string test_dir_;
};

TEST_F( FileUtilTest, Exists ) {
    // 创建一个测试文件
    std::string test_file = FileUtil::JoinPaths( test_dir_, "exist_test.txt" );
    EXPECT_FALSE( FileUtil::Exists( test_file ) );

    // 创建文件后应该存在
    FileUtil::WriteStr( test_file, "test content" );
    EXPECT_TRUE( FileUtil::Exists( test_file ) );

    // 创建目录后也应该存在
    std::string test_subdir = FileUtil::JoinPaths( test_dir_, "subdir" );
    EXPECT_FALSE( FileUtil::Exists( test_subdir ) );
    FileUtil::CreateDirectories( test_subdir );
    EXPECT_TRUE( FileUtil::Exists( test_subdir ) );
}

TEST_F( FileUtilTest, JoinPaths ) {
    // 测试基本路径拼接功能
#ifdef _WIN32
    EXPECT_EQ( FileUtil::JoinPaths( "usr", "local", "bin" ), "usr\\local\\bin" );
    EXPECT_EQ( FileUtil::JoinPaths( "C:", "Users", "test" ), "C:\\Users\\test" );
#else
    EXPECT_EQ( FileUtil::JoinPaths( "usr", "local", "bin" ), "usr/local/bin" );
    EXPECT_EQ( FileUtil::JoinPaths( "/home", "user", "file.txt" ), "/home/user/file.txt" );
#endif

    // 测试单个路径
    EXPECT_EQ( FileUtil::JoinPaths( "file.txt" ), "file.txt" );

    // 测试空路径
    EXPECT_EQ( FileUtil::JoinPaths( "" ), "" );

    // 测试不同参数类型的路径拼接
    EXPECT_EQ( FileUtil::JoinPaths( "usr", std::string_view( "123" ), std::string( "bin" ) ), "usr/123/bin" );
}

TEST_F( FileUtilTest, Remove ) {
    // 创建一个测试文件
    std::string test_file = FileUtil::JoinPaths( test_dir_, "remove_test.txt" );
    FileUtil::WriteStr( test_file, "test content" );
    EXPECT_TRUE( FileUtil::Exists( test_file ) );

    // 删除文件
    EXPECT_TRUE( FileUtil::Remove( test_file ) );
    EXPECT_FALSE( FileUtil::Exists( test_file ) );

    // 删除不存在的文件应该返回false
    EXPECT_FALSE( FileUtil::Remove( "non_existent_file.txt" ) );

    // 创建目录并测试删除
    std::string test_dir = FileUtil::JoinPaths( test_dir_, "remove_dir" );
    FileUtil::CreateDirectories( test_dir );
    EXPECT_TRUE( FileUtil::Exists( test_dir ) );
    EXPECT_TRUE( FileUtil::Remove( test_dir ) );
    EXPECT_FALSE( FileUtil::Exists( test_dir ) );
}

TEST_F( FileUtilTest, RemoveAll ) {
    // 创建目录结构
    std::string test_dir = FileUtil::JoinPaths( test_dir_, "remove_all_test" );
    std::string subdir1  = FileUtil::JoinPaths( test_dir, "subdir1" );
    std::string subdir2  = FileUtil::JoinPaths( subdir1, "subdir2" );

    FileUtil::CreateDirectories( subdir2 );

    // 创建一些文件
    FileUtil::WriteStr( FileUtil::JoinPaths( test_dir, "file1.txt" ), "content1" );
    FileUtil::WriteStr( FileUtil::JoinPaths( subdir1, "file2.txt" ), "content2" );
    FileUtil::WriteStr( FileUtil::JoinPaths( subdir2, "file3.txt" ), "content3" );

    // 确保目录和文件都存在
    EXPECT_TRUE( FileUtil::Exists( test_dir ) );
    EXPECT_TRUE( FileUtil::Exists( FileUtil::JoinPaths( test_dir, "file1.txt" ) ) );
    EXPECT_TRUE( FileUtil::Exists( FileUtil::JoinPaths( subdir1, "file2.txt" ) ) );
    EXPECT_TRUE( FileUtil::Exists( FileUtil::JoinPaths( subdir2, "file3.txt" ) ) );

    // 删除整个目录树
    uint32_t removed_count = FileUtil::RemoveAll( test_dir );
    EXPECT_GE( removed_count, 5 );  // 至少删除了目录和3个文件
    EXPECT_FALSE( FileUtil::Exists( test_dir ) );
}

TEST_F( FileUtilTest, CleanPath ) {
    // 测试路径清理功能
    EXPECT_EQ( FileUtil::CleanPath( "/home/user/" ), "/home/user" );
    EXPECT_EQ( FileUtil::CleanPath( "/home/user/../test" ), "/home/user/../test" );

    // 测试空路径
    EXPECT_EQ( FileUtil::CleanPath( "" ), "" );

    // 测试相对路径
    EXPECT_EQ( FileUtil::CleanPath( "test/path/" ), "test/path" );
}

TEST_F( FileUtilTest, Load2StrAndWriteStr ) {
    std::string test_file = FileUtil::JoinPaths( test_dir_, "load_test.txt" );

    // 测试写入和读取基本内容
    std::string content = "Hello, World!\nThis is a test file.\nWith multiple lines.";
    EXPECT_TRUE( FileUtil::WriteStr( test_file, content ) );

    std::string loaded_content = FileUtil::Load2Str( test_file );
    EXPECT_EQ( loaded_content, content );

    // 测试追加模式
    std::string append_content = "\nAppended content.";
    EXPECT_TRUE( FileUtil::WriteStr( test_file, append_content, true ) );

    std::string loaded_content_appended = FileUtil::Load2Str( test_file );
    EXPECT_EQ( loaded_content_appended, content + append_content );

    // 测试覆盖模式
    std::string new_content = "New content";
    EXPECT_TRUE( FileUtil::WriteStr( test_file, new_content, false ) );

    std::string loaded_new_content = FileUtil::Load2Str( test_file );
    EXPECT_EQ( loaded_new_content, new_content );

    // 测试读取不存在的文件
    std::string non_existent = FileUtil::Load2Str( "non_existent.txt" );
    EXPECT_TRUE( non_existent.empty() );
}

TEST_F( FileUtilTest, Load2ByteArrayAndWriteBytes ) {
    std::string test_file = FileUtil::JoinPaths( test_dir_, "binary_test.bin" );

    // 创建测试二进制数据
    std::vector<uint8_t> data = { 0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0xFC };
    EXPECT_TRUE( FileUtil::WriteBytes( test_file, data ) );

    std::vector<uint8_t> loaded_data = FileUtil::Load2ByteArray( test_file );
    EXPECT_EQ( loaded_data, data );

    // 测试追加模式
    std::vector<uint8_t> append_data = { 0xAA, 0xBB, 0xCC };
    EXPECT_TRUE( FileUtil::WriteBytes( test_file, append_data, true ) );

    std::vector<uint8_t> expected_appended = data;
    expected_appended.insert( expected_appended.end(), append_data.begin(), append_data.end() );

    std::vector<uint8_t> loaded_appended_data = FileUtil::Load2ByteArray( test_file );
    EXPECT_EQ( loaded_appended_data, expected_appended );

    // 测试读取不存在的文件
    std::vector<uint8_t> non_existent = FileUtil::Load2ByteArray( "non_existent.bin" );
    EXPECT_TRUE( non_existent.empty() );
}

TEST_F( FileUtilTest, CreateDirectories ) {
    std::string deep_dir = FileUtil::JoinPaths( test_dir_, "level1", "level2", "level3" );

    // 创建深层目录结构
    EXPECT_TRUE( FileUtil::CreateDirectories( deep_dir ) );
    EXPECT_TRUE( FileUtil::Exists( deep_dir ) );

    // 重复创建返回false（因为目录已存在）
    EXPECT_FALSE( FileUtil::CreateDirectories( deep_dir ) );

    // 创建已经存在的普通文件的父目录应该失败
    std::string file_path = FileUtil::JoinPaths( test_dir_, "file.txt" );
    FileUtil::WriteStr( file_path, "content" );

    std::string sub_dir = FileUtil::JoinPaths( file_path, "subdir" );
    // 这种情况下行为可能因平台而异，但我们至少确保不会崩溃
    FileUtil::CreateDirectories( sub_dir );
}

TEST_F( FileUtilTest, CreateDirectory ) {
    std::string single_dir = FileUtil::JoinPaths( test_dir_, "single_dir" );

    // 创建单层目录
    EXPECT_TRUE( FileUtil::CreateDirectory( single_dir ) );
    EXPECT_TRUE( FileUtil::Exists( single_dir ) );

    // 重复创建返回false
    EXPECT_FALSE( FileUtil::CreateDirectory( single_dir ) );
}

TEST_F( FileUtilTest, Copy ) {
    std::string source_file = FileUtil::JoinPaths( test_dir_, "source.txt" );
    std::string dest_file   = FileUtil::JoinPaths( test_dir_, "dest.txt" );
    std::string content     = "Test copy content";

    // 创建源文件
    FileUtil::WriteStr( source_file, content );

    // 复制文件
    EXPECT_TRUE( FileUtil::Copy( source_file, dest_file ) );
    EXPECT_TRUE( FileUtil::Exists( dest_file ) );

    std::string loaded_content = FileUtil::Load2Str( dest_file );
    EXPECT_EQ( loaded_content, content );

    // 测试覆盖功能
    std::string new_content = "New content";
    FileUtil::WriteStr( source_file, new_content );

    // 不允许覆盖时应该失败
    EXPECT_FALSE( FileUtil::Copy( source_file, dest_file, false ) );

    // 允许覆盖时应该成功
    EXPECT_TRUE( FileUtil::Copy( source_file, dest_file, true ) );
    std::string loaded_new_content = FileUtil::Load2Str( dest_file );
    EXPECT_EQ( loaded_new_content, new_content );

    // 测试源不存在的情况
    EXPECT_FALSE( FileUtil::Copy( "non_existent.txt", "anywhere.txt" ) );

    // 测试目录复制
    std::string source_dir = FileUtil::JoinPaths( test_dir_, "source_dir" );
    std::string dest_dir   = FileUtil::JoinPaths( test_dir_, "dest_dir" );
    FileUtil::CreateDirectories( source_dir );

    std::string file_in_dir = FileUtil::JoinPaths( source_dir, "file_in_dir.txt" );
    FileUtil::WriteStr( file_in_dir, "content in dir" );

    EXPECT_TRUE( FileUtil::Copy( source_dir, dest_dir ) );
    EXPECT_TRUE( FileUtil::Exists( dest_dir ) );
    EXPECT_TRUE( FileUtil::Exists( FileUtil::JoinPaths( dest_dir, "file_in_dir.txt" ) ) );
}

TEST_F( FileUtilTest, Move ) {
    std::string source_file = FileUtil::JoinPaths( test_dir_, "source.txt" );
    std::string dest_file   = FileUtil::JoinPaths( test_dir_, "dest.txt" );
    std::string content     = "Test move content";

    // 创建源文件
    FileUtil::WriteStr( source_file, content );

    // 移动文件
    EXPECT_TRUE( FileUtil::Move( source_file, dest_file ) );
    EXPECT_FALSE( FileUtil::Exists( source_file ) );
    EXPECT_TRUE( FileUtil::Exists( dest_file ) );

    std::string loaded_content = FileUtil::Load2Str( dest_file );
    EXPECT_EQ( loaded_content, content );

    // 测试覆盖功能
    std::string new_source  = FileUtil::JoinPaths( test_dir_, "new_source.txt" );
    std::string new_content = "New content";
    FileUtil::WriteStr( new_source, new_content );

    // 不允许覆盖时应该失败
    EXPECT_FALSE( FileUtil::Move( new_source, dest_file, false ) );

    // 允许覆盖时应该成功
    EXPECT_TRUE( FileUtil::Move( new_source, dest_file, true ) );
    EXPECT_FALSE( FileUtil::Exists( new_source ) );
    std::string loaded_new_content = FileUtil::Load2Str( dest_file );
    EXPECT_EQ( loaded_new_content, new_content );
}

TEST_F( FileUtilTest, DirName ) {
    // 测试基本功能
    EXPECT_EQ( FileUtil::DirName( "/home/user/file.txt" ), "/home/user" );
    EXPECT_EQ( FileUtil::DirName( "/home/user/" ), "/home/user" );

    // 测试根目录
    EXPECT_EQ( FileUtil::DirName( "/" ), "/" );

    // 测试相对路径
    EXPECT_EQ( FileUtil::DirName( "dir/file.txt" ), "dir" );

    // 测试文件名只有本身
    EXPECT_EQ( FileUtil::DirName( "file.txt" ), "" );

    // 测试空路径
    EXPECT_EQ( FileUtil::DirName( "" ), "" );
}

TEST_F( FileUtilTest, BaseName ) {
    // 测试基本功能
    EXPECT_EQ( FileUtil::BaseName( "/home/user/file.txt" ), "file.txt" );
    EXPECT_EQ( FileUtil::BaseName( "/home/user/" ), "" );

    // 测试只有文件名
    EXPECT_EQ( FileUtil::BaseName( "file.txt" ), "file.txt" );

    // 测试空路径
    EXPECT_EQ( FileUtil::BaseName( "" ), "" );
}

TEST_F( FileUtilTest, Extension ) {
    // 测试基本功能
    EXPECT_EQ( FileUtil::Extension( "file.txt" ), ".txt" );
    EXPECT_EQ( FileUtil::Extension( "/path/to/file.tar.gz" ), ".gz" );
    EXPECT_EQ( FileUtil::Extension( "file" ), "" );

    // 测试以点号结尾
    EXPECT_EQ( FileUtil::Extension( "file." ), "." );

    // 测试多个点号
    EXPECT_EQ( FileUtil::Extension( "archive.tar.gz" ), ".gz" );

    // 测试空路径
    EXPECT_EQ( FileUtil::Extension( "" ), "" );
}

TEST_F( FileUtilTest, Stem ) {
    // 测试基本功能
    EXPECT_EQ( FileUtil::Stem( "file.txt" ), "file" );
    EXPECT_EQ( FileUtil::Stem( "/path/to/file.tar.gz" ), "file.tar" );
    EXPECT_EQ( FileUtil::Stem( "file" ), "file" );

    // 测试以点号结尾
    EXPECT_EQ( FileUtil::Stem( "file." ), "file" );

    // 测试空路径
    EXPECT_EQ( FileUtil::Stem( "" ), "" );
}

TEST_F( FileUtilTest, IsFileAndIsDirectory ) {
    std::string test_file     = FileUtil::JoinPaths( test_dir_, "test.txt" );
    std::string test_dir_path = FileUtil::JoinPaths( test_dir_, "test_dir" );

    // 测试不存在的路径
    EXPECT_FALSE( FileUtil::IsFile( test_file ) );
    EXPECT_FALSE( FileUtil::IsDirectory( test_dir_path ) );

    // 创建文件
    FileUtil::WriteStr( test_file, "content" );
    EXPECT_TRUE( FileUtil::IsFile( test_file ) );
    EXPECT_FALSE( FileUtil::IsDirectory( test_file ) );

    // 创建目录
    FileUtil::CreateDirectories( test_dir_path );
    EXPECT_FALSE( FileUtil::IsFile( test_dir_path ) );
    EXPECT_TRUE( FileUtil::IsDirectory( test_dir_path ) );
}

TEST_F( FileUtilTest, FileSize ) {
    std::string test_file = FileUtil::JoinPaths( test_dir_, "size_test.txt" );

    // 测试不存在的文件
    EXPECT_EQ( FileUtil::FileSize( test_file ), 0 );

    // 创建文件并测试大小
    std::string content = "1234567890";  // 10字节
    FileUtil::WriteStr( test_file, content );
    EXPECT_EQ( FileUtil::FileSize( test_file ), 10 );

    // 测试空文件
    std::string empty_file = FileUtil::JoinPaths( test_dir_, "empty.txt" );
    FileUtil::WriteStr( empty_file, "" );
    EXPECT_EQ( FileUtil::FileSize( empty_file ), 0 );

    // 测试目录大小（应该返回0）
    EXPECT_EQ( FileUtil::FileSize( test_dir_ ), 0 );
}

TEST_F( FileUtilTest, ListDir ) {
    // 创建测试文件和目录
    FileUtil::WriteStr( FileUtil::JoinPaths( test_dir_, "file1.txt" ), "content" );
    FileUtil::WriteStr( FileUtil::JoinPaths( test_dir_, "file2.txt" ), "content" );
    FileUtil::CreateDirectories( FileUtil::JoinPaths( test_dir_, "subdir" ) );
    FileUtil::WriteStr( FileUtil::JoinPaths( test_dir_, ".hidden" ), "hidden content" );

    // 列出目录内容（不包括隐藏文件）
    std::vector<std::string> files = FileUtil::ListDir( test_dir_ );
    EXPECT_EQ( files.size(), 3 );  // file1.txt, file2.txt, subdir

    // 检查是否包含期望的条目
    std::vector<std::string> expected = { "file1.txt", "file2.txt", "subdir" };
    for ( const auto &item : expected ) {
        EXPECT_NE( std::find( files.begin(), files.end(), item ), files.end() );
    }

    // 列出目录内容（包括隐藏文件）
    std::vector<std::string> files_with_hidden = FileUtil::ListDir( test_dir_, true );
    EXPECT_EQ( files_with_hidden.size(), 4 );  // 还包括 .hidden

    // 检查是否包含隐藏文件
    EXPECT_NE( std::find( files_with_hidden.begin(), files_with_hidden.end(), ".hidden" ), files_with_hidden.end() );

    // 测试不存在的目录
    std::vector<std::string> non_existent = FileUtil::ListDir( "non_existent_dir" );
    EXPECT_TRUE( non_existent.empty() );
}

TEST_F( FileUtilTest, ListDirFullPaths ) {
    std::string file1  = FileUtil::JoinPaths( test_dir_, "file1.txt" );
    std::string file2  = FileUtil::JoinPaths( test_dir_, "file2.txt" );
    std::string subdir = FileUtil::JoinPaths( test_dir_, "subdir" );

    FileUtil::WriteStr( file1, "content" );
    FileUtil::WriteStr( file2, "content" );
    FileUtil::CreateDirectories( subdir );

    // 列出完整路径
    std::vector<std::string> full_paths = FileUtil::ListDirFullPaths( test_dir_ );
    EXPECT_EQ( full_paths.size(), 3 );

    // 检查是否包含完整路径
    EXPECT_NE( std::find( full_paths.begin(), full_paths.end(), file1 ), full_paths.end() );
    EXPECT_NE( std::find( full_paths.begin(), full_paths.end(), file2 ), full_paths.end() );
    EXPECT_NE( std::find( full_paths.begin(), full_paths.end(), subdir ), full_paths.end() );

    // 测试不存在的目录
    std::vector<std::string> non_existent = FileUtil::ListDirFullPaths( "non_existent_dir" );
    EXPECT_TRUE( non_existent.empty() );
}

TEST_F( FileUtilTest, IsAbsolutePath ) {
    // Unix/Linux绝对路径测试
    EXPECT_TRUE( FileUtil::IsAbsolutePath( "/home/user" ) );
    EXPECT_TRUE( FileUtil::IsAbsolutePath( "/" ) );
    EXPECT_FALSE( FileUtil::IsAbsolutePath( "home/user" ) );
    EXPECT_FALSE( FileUtil::IsAbsolutePath( "../user" ) );

    // 空路径测试
    EXPECT_FALSE( FileUtil::IsAbsolutePath( "" ) );
}

TEST_F( FileUtilTest, CreateTempFile ) {
    // 创建临时文件
    std::string temp_file = FileUtil::CreateTempFile( "test_prefix", ".txt" );
    EXPECT_FALSE( temp_file.empty() );
    EXPECT_TRUE( FileUtil::Exists( temp_file ) );

    // 检查文件是否可以写入和读取
    std::string content = "Test content";
    EXPECT_TRUE( FileUtil::WriteStr( temp_file, content ) );
    EXPECT_EQ( FileUtil::Load2Str( temp_file ), content );

    // 清理临时文件
    FileUtil::Remove( temp_file );

    // 在指定目录创建临时文件
    std::string temp_file_in_dir = FileUtil::CreateTempFile( "test", ".tmp", test_dir_ );
    EXPECT_FALSE( temp_file_in_dir.empty() );
    EXPECT_TRUE( FileUtil::Exists( temp_file_in_dir ) );

    // 检查文件是否在正确的目录中
    EXPECT_EQ( FileUtil::DirName( temp_file_in_dir ), test_dir_ );

    // 清理
    FileUtil::Remove( temp_file_in_dir );
}

TEST_F( FileUtilTest, CreateTempDirectory ) {
    // 创建临时目录
    std::string temp_dir = FileUtil::CreateTempDirectory( "test_prefix" );
    EXPECT_FALSE( temp_dir.empty() );
    EXPECT_TRUE( FileUtil::Exists( temp_dir ) );
    EXPECT_TRUE( FileUtil::IsDirectory( temp_dir ) );

    // 在临时目录中创建文件
    std::string file_in_temp = FileUtil::JoinPaths( temp_dir, "test.txt" );
    FileUtil::WriteStr( file_in_temp, "content" );
    EXPECT_TRUE( FileUtil::Exists( file_in_temp ) );

    // 清理临时目录
    FileUtil::RemoveAll( temp_dir );

    // 在指定父目录中创建临时目录
    std::string temp_dir_in_parent = FileUtil::CreateTempDirectory( "test", test_dir_ );
    EXPECT_FALSE( temp_dir_in_parent.empty() );
    EXPECT_TRUE( FileUtil::Exists( temp_dir_in_parent ) );

    // 检查目录是否在正确的父目录中
    EXPECT_EQ( FileUtil::DirName( temp_dir_in_parent ), test_dir_ );

    // 清理
    FileUtil::RemoveAll( temp_dir_in_parent );
}
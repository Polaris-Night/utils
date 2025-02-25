#include <memory>
#include <string>
#include "ResourceGuard.h"
#include "gtest/gtest.h"

using namespace utils;

class TestResource {
public:
    int *pcount;

    TestResource( int *count ) : pcount( count ) { ( *pcount ) += 1; }
    ~TestResource() { ( *pcount ) -= 1; }
};

void TestResourceDeleter( TestResource *resource ) {
    delete resource;
}

TEST( ResourceGuardTest, ConstructorDestructor ) {
    int count = 0;
    {
        TestResource               *resource = new TestResource( &count );
        ResourceGuard<TestResource> guard( resource, TestResourceDeleter );
        EXPECT_EQ( count, 1 );
    }
    EXPECT_EQ( count, 0 );
}

TEST( ResourceGuardTest, MoveConstructor ) {
    int                         count    = 0;
    TestResource               *resource = new TestResource( &count );
    ResourceGuard<TestResource> guard1( resource, TestResourceDeleter );
    EXPECT_EQ( count, 1 );
    ResourceGuard<TestResource> guard2 = std::move( guard1 );
    EXPECT_EQ( count, 1 );
    EXPECT_EQ( guard1.Get(), nullptr );
    EXPECT_NE( guard2.Get(), nullptr );
}

TEST( ResourceGuardTest, MoveAssignment ) {
    int                           count = 0;
    std::shared_ptr<TestResource> resource1( new TestResource( &count ) );
    TestResource                 *resource2 = new TestResource( &count );
    {
        ResourceGuard<TestResource> guard1( resource1.get(), [&resource1]( auto ) { resource1.reset(); } );
        ResourceGuard<TestResource> guard2( resource2, TestResourceDeleter );
        EXPECT_EQ( count, 2 );
        guard2 = std::move( guard1 );
        EXPECT_EQ( count, 2 );
        EXPECT_EQ( guard1.Get(), nullptr );
        EXPECT_NE( guard2.Get(), nullptr );
    }
    EXPECT_EQ( count, 1 );
    EXPECT_EQ( resource1, nullptr );
    TestResourceDeleter( resource2 );
    EXPECT_EQ( count, 0 );
}

TEST( ResourceGuardTest, GetAndRelease ) {
    int                         count    = 0;
    TestResource               *resource = new TestResource( &count );
    ResourceGuard<TestResource> guard( resource, TestResourceDeleter );
    EXPECT_EQ( count, 1 );
    EXPECT_NE( guard.Get(), nullptr );
    TestResource *released_resource = guard.Release();
    EXPECT_EQ( guard.Get(), nullptr );
    EXPECT_NE( released_resource, nullptr );
    delete released_resource;
    EXPECT_EQ( count, 0 );
}

TEST( ResourceGuardTest, NullResource ) {
    int count = 0;
    EXPECT_THROW( { ResourceGuard<TestResource> guard( nullptr, TestResourceDeleter ); }, std::runtime_error );
    EXPECT_EQ( count, 0 );
}

TEST( ResourceGuardTest, NullDeleter ) {
    int           count    = 0;
    TestResource *resource = new TestResource( &count );
    EXPECT_THROW( { ResourceGuard<TestResource> guard( resource, nullptr ); }, std::runtime_error );
    delete resource;
    EXPECT_EQ( count, 0 );
}

TEST( ResourceGuardTest, UncheckNull ) {
    using GuardType = ResourceGuard<TestResource>;
    EXPECT_NO_THROW( { GuardType guard( nullptr, GuardType::Deleter{}, false ); } );
}

TEST( ResourceGuardTest, Reset ) {
    int                         count = 0;
    ResourceGuard<TestResource> guard( new TestResource( &count ), TestResourceDeleter );
    EXPECT_EQ( count, 1 );
    EXPECT_NE( guard.Get(), nullptr );
    guard.Reset();
    EXPECT_EQ( count, 0 );
    EXPECT_EQ( guard.Get(), nullptr );
}
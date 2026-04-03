#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include "../src/git_engine.hpp"

namespace fs = std::filesystem;

static fs::path make_temp_dir()
{
    char tpl[] = "/tmp/kgit_test_XXXXXX";
    if (!mkdtemp(tpl)) return {};
    return tpl;
}

static void cleanup(const fs::path& dir)
{
    std::filesystem::remove_all(dir);
}

int main()
{
    std::cout << "Running GitEngine tests...\n";

    // Test: init a new repo
    {
        GitEngine eng;
        auto dir = make_temp_dir();
        auto r = eng.init(dir.string());
        assert(r.ok() && "init should succeed");
        std::cout << "  [PASS] init\n";
        cleanup(dir);
    }

    // Test: open non-existent repo fails
    {
        GitEngine eng;
        auto r = eng.open("/nonexistent/path/that/does/not/exist");
        assert(!r.ok() && "open non-existent should fail");
        std::cout << "  [PASS] open non-existent fails\n";
    }

    // Test: status on empty repo
    {
        GitEngine eng;
        auto dir = make_temp_dir();
        eng.init(dir.string());
        auto statuses = eng.status_all();
        assert(statuses.ok() && "status should succeed on fresh repo");
        std::cout << "  [PASS] status on empty repo\n";
        cleanup(dir);
    }

    // Test: stage a file
    {
        auto dir = make_temp_dir();
        {
            GitEngine eng;
            eng.init(dir.string());
            auto f = dir / "test.txt";
            std::ofstream(f) << "hello";
            auto r = eng.stage("test.txt");
            assert(r.ok() && "stage should succeed");
        }
        std::cout << "  [PASS] stage file\n";
        cleanup(dir);
    }

    std::cout << "All tests passed.\n";
    return 0;
}

#include <gtest/gtest.h>

#include "socket.hpp"
#include "task.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<Task>,
              "Task must be move-only: that is the whole point");
static_assert(!std::is_copy_assignable_v<Task>, "Task must not be copy-assignable");
static_assert(std::is_move_constructible_v<Task>, "Task must be movable");
static_assert(std::is_move_assignable_v<Task>, "Task must be move-assignable");
static_assert(std::is_nothrow_move_constructible_v<Task>, "move must be noexcept");
static_assert(std::is_default_constructible_v<Task>,
              "worker_thread declares an empty Task before assigning");

namespace {

bool is_fd_open(int fd) {
    return ::fcntl(fd, F_GETFD) != -1;
}

int g_free_function_calls = 0;

void free_function() {
    ++g_free_function_calls;
}

struct DtorFlag {
    bool* flag;

    explicit DtorFlag(bool* f) : flag(f) {}
    DtorFlag(const DtorFlag&)            = default;
    DtorFlag& operator=(const DtorFlag&) = default;

    ~DtorFlag() {
        if (flag != nullptr) {
            *flag = true;
        }
    }

    void operator()() const {}
};

TEST(TaskBasicTest, CallsCapturelessLambda) {
    bool called = false;
    Task task = [&called] { called = true; };
    task();
    EXPECT_TRUE(called);
}

TEST(TaskBasicTest, CaptureByReferenceWorks) {
    int counter = 0;
    Task task = [&counter] { counter += 5; };
    task();
    EXPECT_EQ(counter, 5);
}

TEST(TaskBasicTest, AcceptsFreeFunction) {
    g_free_function_calls = 0;
    Task task = free_function;
    task();
    EXPECT_EQ(g_free_function_calls, 1);
}

TEST(TaskBasicTest, CanBeCalledMoreThanOnce) {
    int counter = 0;
    Task task = [&counter] { ++counter; };
    task();
    task();
    EXPECT_EQ(counter, 2);
}

TEST(TaskBasicTest, DefaultConstructedIsEmpty) {
    Task task;
    EXPECT_FALSE(static_cast<bool>(task));
}

TEST(TaskBasicTest, NonEmptyConvertsToTrue) {
    Task task = [] {};
    EXPECT_TRUE(static_cast<bool>(task));
}

TEST(TaskBasicTest, CallingEmptyTaskThrows) {
    Task task;
    EXPECT_THROW(task(), std::bad_function_call);
}

TEST(TaskMoveOnlyTest, AcceptsLambdaCapturingUniquePtr) {
    auto ptr = std::make_unique<int>(42);
    int  seen = 0;

    Task task = [p = std::move(ptr), &seen] { seen = *p; };
    task();

    EXPECT_EQ(seen, 42);
}

TEST(TaskMoveOnlyTest, AcceptsLambdaCapturingSocket) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    const int read_end = fds[0];

    bool called = false;
    {
        Socket sock(read_end);
        Task   task = [s = std::move(sock), &called] { called = true; };

        task();
        EXPECT_TRUE(is_fd_open(read_end)) << "socket must stay alive inside the task";
    }

    EXPECT_TRUE(called);
    EXPECT_FALSE(is_fd_open(read_end)) << "descriptor leaked";
    ::close(fds[1]);
}

TEST(TaskMoveTest, MoveConstructorTransfersContents) {
    int counter = 0;
    Task source = [&counter] { ++counter; };
    Task target(std::move(source));

    EXPECT_FALSE(static_cast<bool>(source)) << "source must be left empty";
    ASSERT_TRUE(static_cast<bool>(target));

    target();
    EXPECT_EQ(counter, 1);
}

TEST(TaskMoveTest, MoveAssignmentDestroysOldCallable) {
    bool old_destroyed = false;
    {
        Task target = DtorFlag(&old_destroyed);
        old_destroyed = false;

        Task source = [] {};
        target      = std::move(source);

        EXPECT_TRUE(old_destroyed) << "previous callable was not destroyed";
        EXPECT_FALSE(static_cast<bool>(source));
    }
}

TEST(TaskMoveTest, MovedFromTaskCanBeReassigned) {
    Task a = [] {};
    Task b(std::move(a));

    int counter = 0;
    a = [&counter] { ++counter; };

    ASSERT_TRUE(static_cast<bool>(a));
    a();
    EXPECT_EQ(counter, 1);
}

TEST(TaskConstraintTest, AcceptsLvalueLambda) {
    int  counter = 0;
    auto lambda  = [&counter] { ++counter; };

    Task task(lambda);  // lvalue, not a temporary
    task();

    EXPECT_EQ(counter, 1);
}

TEST(TaskConstraintTest, MoveConstructionIsNotHijackedByTemplate) {
    int counter = 0;
    Task source = [&counter] { ++counter; };
    Task target(std::move(source));

    EXPECT_FALSE(static_cast<bool>(source));

    target();
    EXPECT_EQ(counter, 1);
}

}  // namespace
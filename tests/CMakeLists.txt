add_executable(unit_tests
    test_main.cc # 自动机定义的 main 入口，也可以不写，使用 gtest_main 入口
    utils/thread_pool_test.cc # TEST() 宏
)

# GTest::gtest_main 是 googletest 提供的 main 入口，类似这样：
# int main(int argc, char **argv) {
#     testing::InitGoogleTest(&argc, argv); // 初始化框架
#     return RUN_ALL_TESTS();               // 寻找并运行所有 TEST() 宏定义的案例
# }
target_link_libraries(unit_tests
    PRIVATE
    GTest::gtest_main
    chat_core # 链接我们上面定义的库
    Threads::Threads
)

# 3. 注册到 CTest 框架，通过 'ctest' 即可一键运行所有测试案例
add_test(NAME AllUnitTests COMMAND unit_tests)

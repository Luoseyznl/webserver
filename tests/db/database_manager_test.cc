#include "db/database_manager.h"

#include <gtest/gtest.h>

#include <memory>

namespace chat {
namespace {

// 定义一个测试夹具 (Test Fixture)
// GoogleTest 会在每次运行一个 TEST_F 时，自动调用 SetUp() 和 TearDown()
class DatabaseManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    db_ = std::make_unique<DatabaseManager>(":memory:");  // 在内存中创建数据库
  }

  void TearDown() override {
    // unique_ptr 自动销毁 db_
  }

  std::unique_ptr<DatabaseManager> db_;
};

TEST_F(DatabaseManagerTest, CreateUserAndValidate) {
  EXPECT_TRUE(db_->createUser("test_user", "hashed_pwd_123"));  // 1. 创建用户

  EXPECT_TRUE(db_->userExists("test_user"));
  EXPECT_FALSE(db_->userExists("ghost_user"));

  EXPECT_TRUE(db_->validateUser("test_user", "hashed_pwd_123"));  // 3. 验证密码
  EXPECT_FALSE(db_->validateUser("test_user", "wrong_pwd"));

  EXPECT_FALSE(db_->createUser("test_user", "another_pwd"));  // 4. 创建同名用户
}

TEST_F(DatabaseManagerTest, UserOnlineStatus) {
  EXPECT_TRUE(db_->createUser("online_user", "pwd"));

  auto users = db_->getAllUsers();
  ASSERT_EQ(users.size(), 1);
  EXPECT_FALSE(users[0].is_online);

  EXPECT_TRUE(db_->setUserOnlineStatus("online_user", true));  // 设置为上线状态
  users = db_->getAllUsers();
  EXPECT_TRUE(users[0].is_online);
}

TEST_F(DatabaseManagerTest, RoomOperations) {
  EXPECT_TRUE(db_->createUser("admin", "pwd"));

  EXPECT_TRUE(db_->createRoom("C++ Lounge", "admin"));  // 1. 创建房间

  auto rooms = db_->getRooms();
  ASSERT_EQ(rooms.size(), 1);
  EXPECT_EQ(rooms[0], "C++ Lounge");

  EXPECT_TRUE(db_->deleteRoom("C++ Lounge"));  // 2. 删除房间
  EXPECT_TRUE(db_->getRooms().empty());
}

TEST_F(DatabaseManagerTest, RoomMemberManagement) {
  EXPECT_TRUE(db_->createUser("user1", "pwd"));
  EXPECT_TRUE(db_->createUser("user2", "pwd"));
  EXPECT_TRUE(db_->createRoom("Lobby", "user1"));

  EXPECT_TRUE(db_->addRoomMember("Lobby", "user1"));  // 1. 添加成员
  EXPECT_TRUE(db_->addRoomMember("Lobby", "user2"));
  EXPECT_TRUE(db_->addRoomMember("Lobby", "user1"));  // 2. 重复添加

  auto members = db_->getRoomMembers("Lobby");  // 3. 获取成员
  ASSERT_EQ(members.size(), 2);

  EXPECT_TRUE(db_->removeRoomMember("Lobby", "user2"));  // 4. 移除成员
  members = db_->getRoomMembers("Lobby");
  ASSERT_EQ(members.size(), 1);
  EXPECT_EQ(members[0], "user1");
}

TEST_F(DatabaseManagerTest, MessageOperations) {
  EXPECT_TRUE(db_->createUser("alice", "pwd"));
  EXPECT_TRUE(db_->createUser("bob", "pwd"));
  EXPECT_TRUE(db_->createRoom("Tech Talk", "alice"));

  // 1. 模拟发送消息
  int64_t t1 = 1000;
  int64_t t2 = 2000;
  EXPECT_TRUE(db_->saveMessage("Tech Talk", "alice", "Hello C++", t1));
  EXPECT_TRUE(db_->saveMessage("Tech Talk", "bob", "Hi Alice", t2));

  // 2. 获取所有消息 (since = 0)
  auto msgs = db_->getMessages("Tech Talk", 0);
  ASSERT_EQ(msgs.size(), 2);
  EXPECT_EQ(msgs[0]["username"], "alice");
  EXPECT_EQ(msgs[0]["content"], "Hello C++");
  EXPECT_EQ(msgs[1]["username"], "bob");

  // 3. 获取 t1 之后的消息
  auto new_msgs = db_->getMessages("Tech Talk", t1);
  ASSERT_EQ(new_msgs.size(), 1);
  EXPECT_EQ(new_msgs[0]["username"], "bob");
  EXPECT_EQ(new_msgs[0]["content"], "Hi Alice");
}

}  // namespace
}  // namespace chat
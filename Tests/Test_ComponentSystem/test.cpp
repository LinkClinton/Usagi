#include <gtest/gtest.h>

#include <chrono>
#include <eigen3/Eigen/Core>

#include <Usagi/Engine/Entity/Game.hpp>
#include <Usagi/Engine/Entity/Entity.hpp>
#include <Usagi/Engine/Entity/Component.hpp>
#include <Usagi/Engine/Event/Event.hpp>
#include <Usagi/Engine/Entity/ConstrainedSubsystem.hpp>

using namespace yuki;

namespace
{
struct PositionComponent : Component
{
    Eigen::Vector3f position { 0, 0, 0 };
};

struct PhysicalComponent : Component
{
    Eigen::Vector3f velocity { 1, 2, 3 };
};

class PhysicsSubsystem
    : public ConstrainedSubsystem<PositionComponent, PhysicalComponent>
{
    Entity *mEntity = nullptr;

public:
    void update(const std::chrono::seconds &dt) override
    {
        if(mEntity)
        {
            auto pos = mEntity->getComponent<PositionComponent>();
            auto phy = mEntity->getComponent<PhysicalComponent>();
            pos->position += dt.count() * phy->velocity;
        }
    }

    void updateRegistry(Entity *entity) override
    {
        if(canProcess(entity))
            mEntity = entity;
    }
};

struct TestEvent : Event
{
    const int hello = 233;
};
}

TEST(ECSTest, SubsystemNameConflict)
{
    Game game;
    {
        SubsystemInfo info;
        info.name = "Physics";
        info.subsystem = std::make_unique<PhysicsSubsystem>();
        EXPECT_NO_THROW(game.addSubsystem(std::move(info)));
    }
    {
        SubsystemInfo info;
        info.name = "Physics";
        info.subsystem = std::make_unique<PhysicsSubsystem>();
        EXPECT_THROW(game.addSubsystem(std::move(info)), std::runtime_error
        );
    }
}

TEST(ECSTest, EventBubblingTest)
{
    // Entity tree structure
    //
    //        root
    //        /  \
    //       a    b
    //      /
    //     c

    Game game;
    auto r = game.getRootEntity();
    auto a = r->addChild();
    auto b = r->addChild();
    auto c = a->addChild();
    int dr = 0, da = 0, db = 0, dc = 0;
    r->addEventListener<TestEvent>(
        [&](auto &&e)
        {
            dr = e.hello;
        });
    a->addEventListener<TestEvent>(
        [&](auto &&e)
        {
            da = e.hello;
            e.stopBubbling();
        });
    b->addEventListener<TestEvent>(
        [&](auto &&e)
        {
            db = e.hello;
        });
    c->addEventListener<TestEvent>(
        [&](auto &&e)
        {
            dc = e.hello;
        });
    c->fireEvent<TestEvent>();

    EXPECT_EQ(dr, 0);
    EXPECT_EQ(db, 0);
    EXPECT_EQ(da, 233);
    EXPECT_EQ(dc, 233);
}

TEST(ECSTest, EventCancelTest)
{
    Game game;
    auto r = game.getRootEntity();
    bool a = false, b = false, c = false;
    r->addEventListener<TestEvent>(
        [&](auto &&e)
        {
            a = true;
        });
    r->addEventListener<TestEvent>(
        [&](auto &&e)
        {
            b = true;
            e.cancel();
        });
    r->addEventListener<TestEvent>(
        [&](auto &&e)
        {
            c = true;
        });
    r->fireEvent<TestEvent>();

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_FALSE(c);
}

TEST(ECSTest, ComponentIdentityTest)
{
    Game game;
    auto r = game.getRootEntity();
    r->addComponent<PhysicalComponent>();
    EXPECT_TRUE(r->hasComponent<PhysicalComponent>());
    EXPECT_FALSE(r->hasComponent<PositionComponent>());
    EXPECT_NO_THROW(r->getComponent<PhysicalComponent>());
    EXPECT_THROW(r->getComponent<PositionComponent>(), std::runtime_error);
}

TEST(ECSTest, ConstrainedSubsystemTest)
{
    PhysicsSubsystem s;
    Entity e { nullptr };
    EXPECT_FALSE(s.canProcess(&e));
    e.addComponent<PositionComponent>();
    EXPECT_FALSE(s.canProcess(&e));
    e.addComponent<PhysicalComponent>();
    EXPECT_TRUE(s.canProcess(&e));
}

TEST(ECSTest, ComponentSystemInteractionTest)
{
    using namespace std::chrono_literals;

    Game game;
    {
        SubsystemInfo info;
        info.name = "Physics";
        info.subsystem = std::make_unique<PhysicsSubsystem>();
        EXPECT_NO_THROW(game.addSubsystem(std::move(info)));
    }
    auto r = game.getRootEntity();
    r->addComponent<PositionComponent>();
    r->addComponent<PhysicalComponent>();
    EXPECT_FLOAT_EQ(
        (r->getComponent<PositionComponent>()->position
            - Eigen::Vector3f(0, 0, 0)).norm(),
        0.f);
    game.update(2s);
    EXPECT_FLOAT_EQ(
        (r->getComponent<PositionComponent>()->position
            - Eigen::Vector3f(2, 4, 6)).norm(),
        0.f);
    game.setSubsystemEnabled("Physics", false);
    game.update(2s);
    EXPECT_FLOAT_EQ(
        (r->getComponent<PositionComponent>()->position
            - Eigen::Vector3f(2, 4, 6)).norm(),
        0.f);
}

#include "../GTestMain.hpp"

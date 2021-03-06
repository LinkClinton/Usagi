#include <gtest/gtest.h>

#include <chrono>

#include <Eigen/Core>

#include <Usagi/Engine/Core/Element.hpp>
#include <Usagi/Engine/Core/Component.hpp>
#include <Usagi/Engine/Core/Event/Event.hpp>
#include <Usagi/Engine/Game/Game.hpp>
#include <Usagi/Engine/Game/ConstrainedSubsystem.hpp>
#include <Usagi/Engine/Utility/TypeCast.hpp>
#include <chrono>

using namespace usagi;

namespace
{
struct PositionComponent : Component
{
    Eigen::Vector3f position { 0, 0, 0 };

    const std::type_info & getBaseTypeInfo() override final
    {
        return typeid(PositionComponent);
    }
};

struct PhysicalComponent : Component
{
    Eigen::Vector3f velocity { 1, 2, 3 };

    const std::type_info & getBaseTypeInfo() override final
    {
        return typeid(PhysicalComponent);
    }
};

struct AdvancedPhysicalComponent : PhysicalComponent
{
    Eigen::Vector3f acceleration { 5, 6, 7 };
};

class PhysicsSubsystem
    : public ConstrainedSubsystem<PositionComponent, PhysicalComponent>
{
    Element *mEntity = nullptr;

public:
    void update(const TimeDuration &dt) override
    {
        if(mEntity)
        {
            auto pos = mEntity->getComponent<PositionComponent>();
            auto phy = mEntity->getComponent<PhysicalComponent>();
            pos->position += dt.count() * phy->velocity;
        }
    }

    void updateRegistry(Element *element) override
    {
        if(processable(element))
            mEntity = element;
    }

    std::unique_ptr<PhysicalComponent> createPhysicalComponent() const
    {
        return std::make_unique<AdvancedPhysicalComponent>();
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
        EXPECT_THROW(game.addSubsystem(std::move(info)), std::runtime_error);
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
    auto r = game.rootElement();
    auto a = r->addChild();
    auto b = r->addChild();
    auto c = a->addChild();
    int dr = 0, da = 0, db = 0, dc = 0;
    r->addEventListener<TestEvent>(
        [&](auto &&e) {
            dr = e.hello;
        });
    a->addEventListener<TestEvent>(
        [&](auto &&e) {
            da = e.hello;
            e.stopBubbling();
        });
    b->addEventListener<TestEvent>(
        [&](auto &&e) {
            db = e.hello;
        });
    c->addEventListener<TestEvent>(
        [&](auto &&e) {
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
    auto r = game.rootElement();
    bool a = false, b = false, c = false;
    r->addEventListener<TestEvent>(
        [&](auto &&e) {
            a = true;
        });
    r->addEventListener<TestEvent>(
        [&](auto &&e) {
            b = true;
            e.cancel();
        });
    r->addEventListener<TestEvent>(
        [&](auto &&e) {
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
    auto r = game.rootElement();
    r->addComponent<PhysicalComponent>();
    EXPECT_TRUE(r->hasComponent<PhysicalComponent>());
    EXPECT_FALSE(r->hasComponent<PositionComponent>());
    EXPECT_NO_THROW(r->getComponent<PhysicalComponent>());
    EXPECT_THROW(r->getComponent<PositionComponent>(), std::runtime_error);
}

TEST(ECSTest, ConstrainedSubsystemTest)
{
    PhysicsSubsystem s;
    Element e { nullptr };
    EXPECT_FALSE(s.processable(&e));
    e.addComponent<PositionComponent>();
    EXPECT_FALSE(s.processable(&e));
    e.addComponent<PhysicalComponent>();
    EXPECT_TRUE(s.processable(&e));
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
    auto r = game.rootElement();
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
    game.disableSubsystem("Physics");
    game.update(2s);
    EXPECT_FLOAT_EQ(
        (r->getComponent<PositionComponent>()->position
            - Eigen::Vector3f(2, 4, 6)).norm(),
        0.f);
}

TEST(ECSTest, PolymorphicComponentTest)
{
    Game game;
    PhysicsSubsystem *phy = nullptr;
    {
        SubsystemInfo info;
        info.name = "Physics";
        info.subsystem = std::make_unique<PhysicsSubsystem>();
        EXPECT_NO_THROW(phy = static_cast<PhysicsSubsystem*>(
            game.addSubsystem(std::move(info))));
    }
    auto r = game.rootElement();
    r->addComponent(phy->createPhysicalComponent());
    EXPECT_NO_THROW(r->getComponent<PhysicalComponent>());
    AdvancedPhysicalComponent *c = nullptr;
    EXPECT_NO_THROW((c = r->getComponent<
        PhysicalComponent, AdvancedPhysicalComponent
    >()));
    EXPECT_FLOAT_EQ((c->acceleration - Eigen::Vector3f(5, 6, 7)).norm(), 0.f);
}

namespace
{
class StrictElement : public ElementTreeNode<Element>
{
public:
    StrictElement(Element *parent)
        : ElementTreeNode { parent }
    {
    }

private:
    bool acceptChild(Element *child) override
    {
        return is_instance_of<StrictElement>(child);
    }
};
}

TEST(ECSTest, ChildRejectionTest)
{
    StrictElement e { nullptr };
    EXPECT_THROW(e.addChild(), std::logic_error);
    EXPECT_NO_THROW(e.addChild<StrictElement>());
}

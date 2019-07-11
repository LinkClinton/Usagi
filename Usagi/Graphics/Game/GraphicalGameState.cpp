﻿#include "GraphicalGameState.hpp"

#include <execution>

#include <Usagi/Graphics/RenderTarget/RenderTargetDescriptor.hpp>

#include "RenderableSystem.hpp"
#include "GraphicalGame.hpp"

void usagi::GraphicalGameState::subsystemFilter(System *subsystem)
{
    if(const auto sys = dynamic_cast<RenderableSystem*>(subsystem))
    {
        RenderTargetDescriptor desc { mGame };
        // create the render target before pipelines as render pass is used
        // in the later process.
        sys->createRenderTarget(desc);
        sys->createPipelines();
        mRenderableSystems.push_back({ mRenderableSystems.size(), sys });
    }
}

usagi::GraphicalGameState::GraphicalGameState(
    Element *parent,
    std::string name,
    GraphicalGame *game)
    : GameState(parent, std::move(name))
    , mGame(game)
{
}

void usagi::GraphicalGameState::update(const Clock &clock)
{
    GameState::update(clock);

    // process input... but not leak into previous state???? do in game?
    // pause previous state & remove input handler

    // set camera for each subsystem??

    // record command lists in parallel
    mCommandLists.resize(mRenderableSystems.size());
    std::for_each(
        std::execution::par,
        mRenderableSystems.begin(),
        mRenderableSystems.end(),
        [&](const IndexedRenderable &i) {
            mCommandLists[i.first] = i.second->render(mClock);
        }
    );
    mGame->submitGraphicsJobs(mCommandLists);
}

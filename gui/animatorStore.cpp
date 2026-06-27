#include "gui/animatorStore.hpp"

#include "com/logger.hpp"

namespace Gui {

AnimatorStore::AnimatorStore()
:
    mAnimators{},
    mLogger{Logging::LogState::GetLogger("Gui::AnimatorStore")}
{
}

void AnimatorStore::AddAnimator(std::unique_ptr<IAnimator>&& animator)
{
    mAnimators.emplace_back(std::move(animator));
    mLogger.Spam() << "Added animator @" << mAnimators.back() << "\n";
}

void AnimatorStore::OnTimeDelta(double delta)
{
    mLogger.Spam() << "Ticking : " << delta << "\n";
    // Snapshot the count: a completion callback may AddAnimator (e.g. the next combat
    // turn's animation). Index-based iteration tolerates the vector reallocating, and
    // animators added during this tick (index >= count) wait until the next frame.
    const std::size_t count = mAnimators.size();
    for (std::size_t i = 0; i < count; ++i)
        mAnimators[i]->OnTimeDelta(delta);

    mAnimators.erase(
        std::remove_if(
            mAnimators.begin(), mAnimators.end(),
            [&](const auto& a){
                return !a->IsAlive();
            }),
        mAnimators.end());
}

}

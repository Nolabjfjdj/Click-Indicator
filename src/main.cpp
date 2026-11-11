#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <array>
#include <vector>

using namespace geode::prelude;

namespace {
    struct ClickEvent {
        int start;
        int duration;
    };

    std::vector<ClickEvent> recordedClicks;
    int activePressStart = -1;

    constexpr float timelineWidth = 260.f;
    constexpr float timelineHeight = 48.f;
    constexpr float timelineNowX = 180.f;
    constexpr float timelinePixelsPerMs = 0.12f;
    constexpr int visibleClickCount = 16;

    CCLayerColor* makeRect(ccColor4B color, float width, float height) {
        return CCLayerColor::create(color, width, height);
    }

    CCNode* createClickIndicator() {
        auto root = CCNode::create();
        root->setContentSize({34.f, 180.f});

        auto background = makeRect({0, 20, 12, 110}, 34.f, 180.f);
        background->setPosition({0.f, 0.f});
        root->addChild(background);

        auto left = makeRect({35, 255, 145, 255}, 4.f, 180.f);
        left->setPosition({0.f, 0.f});
        root->addChild(left);

        auto right = makeRect({35, 255, 145, 255}, 4.f, 180.f);
        right->setPosition({30.f, 0.f});
        root->addChild(right);

        auto top = makeRect({35, 255, 145, 255}, 34.f, 4.f);
        top->setPosition({0.f, 176.f});
        root->addChild(top);

        auto bottom = makeRect({35, 255, 145, 255}, 34.f, 4.f);
        bottom->setPosition({0.f, 0.f});
        root->addChild(bottom);

        auto rungOne = makeRect({35, 255, 145, 230}, 26.f, 4.f);
        rungOne->setPosition({4.f, 132.f});
        root->addChild(rungOne);

        auto rungTwo = makeRect({35, 255, 145, 230}, 26.f, 4.f);
        rungTwo->setPosition({4.f, 88.f});
        root->addChild(rungTwo);

        auto rungThree = makeRect({35, 255, 145, 230}, 26.f, 4.f);
        rungThree->setPosition({4.f, 44.f});
        root->addChild(rungThree);

        auto timingBar = makeRect({255, 55, 65, 255}, 48.f, 7.f);
        timingBar->setTag(416);
        timingBar->setPosition({-7.f, -5.f});
        root->addChild(timingBar);

        return root;
    }

    CCNode* createTimeline() {
        auto root = CCNode::create();
        root->setContentSize({timelineWidth, timelineHeight});

        auto background = makeRect({0, 0, 0, 165}, timelineWidth, timelineHeight);
        root->addChild(background);

        auto track = makeRect({100, 115, 125, 180}, timelineWidth - 12.f, 2.f);
        track->setPosition({6.f, 23.f});
        root->addChild(track);

        auto nowMarker = makeRect({245, 245, 245, 255}, 2.f, 32.f);
        nowMarker->setPosition({timelineNowX, 8.f});
        root->addChild(nowMarker);

        for (int i = 0; i < visibleClickCount; i++) {
            auto bar = makeRect({45, 225, 145, 220}, 8.f, 12.f);
            bar->setTag(500 + i);
            bar->setVisible(false);
            root->addChild(bar);
        }

        auto prediction = makeRect({255, 175, 45, 255}, 8.f, 16.f);
        prediction->setTag(516);
        prediction->setVisible(false);
        root->addChild(prediction);

        return root;
    }

    void updateTimeline(CCNode* timeline, int now) {
        if (!timeline) {
            return;
        }

        auto first = recordedClicks.size() > visibleClickCount
            ? recordedClicks.size() - visibleClickCount
            : 0;

        int visibleIndex = 0;
        for (size_t i = first; i < recordedClicks.size(); i++) {
            auto bar = static_cast<CCLayerColor*>(timeline->getChildByTag(500 + visibleIndex));
            if (!bar) {
                visibleIndex++;
                continue;
            }

            auto const& click = recordedClicks[i];
            auto delta = click.start - now;
            auto x = timelineNowX + static_cast<float>(delta) * timelinePixelsPerMs;
            auto width = std::max(3.f, static_cast<float>(click.duration) * timelinePixelsPerMs);

            bar->setContentSize({width, 12.f});
            bar->setPosition({x, 18.f});
            bar->setVisible(x + width >= 0.f && x <= timelineWidth);
            visibleIndex++;
        }

        for (; visibleIndex < visibleClickCount; visibleIndex++) {
            auto bar = timeline->getChildByTag(500 + visibleIndex);
            if (bar) {
                bar->setVisible(false);
            }
        }

        auto prediction = static_cast<CCLayerColor*>(timeline->getChildByTag(516));
        if (!prediction) {
            return;
        }

        prediction->setVisible(false);
        if (recordedClicks.size() < 3) {
            return;
        }

        auto count = std::min<size_t>(8, recordedClicks.size() - 1);
        long long intervalTotal = 0;
        long long durationTotal = 0;

        for (size_t i = recordedClicks.size() - count; i < recordedClicks.size(); i++) {
            intervalTotal += recordedClicks[i].start - recordedClicks[i - 1].start;
            durationTotal += recordedClicks[i].duration;
        }

        auto averageInterval = static_cast<int>(intervalTotal / static_cast<long long>(count));
        auto averageDuration = static_cast<int>(durationTotal / static_cast<long long>(count));

        if (averageInterval < 40 || averageInterval > 2000) {
            return;
        }

        auto lastStart = recordedClicks.back().start;
        auto expectedStart = lastStart + averageInterval;

        if (now > expectedStart + 150 || expectedStart - now > 650) {
            return;
        }

        auto predictionX = timelineNowX + static_cast<float>(expectedStart - now) * timelinePixelsPerMs;
        auto predictionWidth = std::max(4.f, static_cast<float>(averageDuration) * timelinePixelsPerMs);

        prediction->setContentSize({predictionWidth, 16.f});
        prediction->setPosition({predictionX, 16.f});
        prediction->setVisible(predictionX + predictionWidth >= 0.f && predictionX <= timelineWidth);
    }
}

class $modify(ClickIndicatorPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        recordedClicks.clear();
        activePressStart = -1;

        auto recordingEnabled = Mod::get()->getSettingValue<bool>("recording");
        log::info("Click recording is {}", recordingEnabled ? "ENABLED" : "DISABLED");

        if (!Mod::get()->getSettingValue<bool>("enabled")) {
            return true;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto indicator = createClickIndicator();
        indicator->setTag(415);
        auto rightSide = Mod::get()->getSettingValue<bool>("right_side");
        indicator->setPosition({
            rightSide ? winSize.width - 62.f : 28.f,
            (winSize.height - 180.f) / 2.f
        });
        this->addChild(indicator, 9999);

        if (recordingEnabled) {
            auto timeline = createTimeline();
            timeline->setTag(417);
            timeline->setPosition({
                rightSide ? winSize.width - timelineWidth - 24.f : 24.f,
                24.f
            });
            this->addChild(timeline, 10000);
        }

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (Mod::get()->getSettingValue<bool>("recording")) {
            updateTimeline(this->getChildByTag(417), this->getPlayTimerMilli());
        }
    }
};

$on_mod(Loaded) {
    log::info("Click Indicator loaded");
}

class $modify(ClickIndicatorInputLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1 && isPlayer1) {
            auto now = this->getPlayTimerMilli();

            if (down && Mod::get()->getSettingValue<bool>("recording") && activePressStart < 0) {
                activePressStart = now;
                log::info("Recording click press at {} ms", activePressStart);
            }
            else if (!down && activePressStart >= 0) {
                auto duration = now - activePressStart;
                if (duration >= 0) {
                    recordedClicks.push_back({activePressStart, duration});
                    log::info("RECORDED CLICK #{}: start={} ms, duration={} ms", recordedClicks.size(), activePressStart, duration);
                }
                activePressStart = -1;
            }
        }

        if (down && button == 1) {
            auto indicator = this->getChildByTag(415);
            if (indicator) {
                auto timingBar = static_cast<CCLayerColor*>(indicator->getChildByTag(416));
                if (timingBar) {
                    timingBar->stopAllActions();
                    timingBar->setOpacity(255);
                    timingBar->runAction(CCFadeTo::create(0.18f, 70));
                }
            }
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

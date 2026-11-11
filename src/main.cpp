#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <vector>

using namespace geode::prelude;

namespace {
    struct ClickEvent {
        int start;
        int duration;
    };

    std::vector<ClickEvent> recordedClicks;
    int activePressStart = -1;

    constexpr float indicatorWidth = 34.f;
    constexpr float indicatorHeight = 180.f;
    constexpr float playbackPixelsPerMs = 0.09f;
    constexpr int visiblePlaybackBars = 16;

    CCLayerColor* makeRect(ccColor4B color, float width, float height) {
        return CCLayerColor::create(color, width, height);
    }

    CCNode* createClickIndicator(bool playbackEnabled) {
        auto root = CCNode::create();
        root->setContentSize({indicatorWidth, indicatorHeight});

        auto background = makeRect({0, 20, 12, 110}, indicatorWidth, indicatorHeight);
        root->addChild(background);

        auto left = makeRect({35, 255, 145, 255}, 4.f, indicatorHeight);
        root->addChild(left);

        auto right = makeRect({35, 255, 145, 255}, 4.f, indicatorHeight);
        right->setPosition({30.f, 0.f});
        root->addChild(right);

        auto top = makeRect({35, 255, 145, 255}, indicatorWidth, 4.f);
        top->setPosition({0.f, 176.f});
        root->addChild(top);

        auto bottom = makeRect({35, 255, 145, 255}, indicatorWidth, 4.f);
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

        if (playbackEnabled) {
            for (int i = 0; i < visiblePlaybackBars; i++) {
                auto bar = makeRect({45, 225, 145, 245}, 26.f, 7.f);
                bar->setTag(600 + i);
                bar->setVisible(false);
                root->addChild(bar);
            }
        }

        return root;
    }

    void updatePlaybackIndicator(CCNode* indicator, int now) {
        if (!indicator) {
            return;
        }

        int barIndex = 0;
        for (auto const& click : recordedClicks) {
            auto delta = click.start - now;
            if (delta < 0 || delta > 1900) {
                continue;
            }

            if (barIndex >= visiblePlaybackBars) {
                break;
            }

            auto bar = indicator->getChildByTag(600 + barIndex);
            if (bar) {
                auto y = indicatorHeight - 6.f - static_cast<float>(delta) * playbackPixelsPerMs;
                bar->setPosition({4.f, y});
                bar->setVisible(y >= 4.f && y <= indicatorHeight - 7.f);
            }
            barIndex++;
        }

        for (; barIndex < visiblePlaybackBars; barIndex++) {
            auto bar = indicator->getChildByTag(600 + barIndex);
            if (bar) {
                bar->setVisible(false);
            }
        }
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
        auto playbackEnabled = Mod::get()->getSettingValue<bool>("playback");

        log::info("Click recording is {}", recordingEnabled ? "ENABLED" : "DISABLED");
        log::info("Click playback is {}", playbackEnabled ? "ENABLED" : "DISABLED");

        if (!Mod::get()->getSettingValue<bool>("enabled")) {
            return true;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto indicator = createClickIndicator(playbackEnabled);
        indicator->setTag(415);
        auto rightSide = Mod::get()->getSettingValue<bool>("right_side");
        indicator->setPosition({
            rightSide ? winSize.width - 62.f : 28.f,
            (winSize.height - indicatorHeight) / 2.f
        });
        this->addChild(indicator, 9999);

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (Mod::get()->getSettingValue<bool>("playback") &&
            !Mod::get()->getSettingValue<bool>("recording")) {
            updatePlaybackIndicator(this->getChildByTag(415), this->getPlayTimerMilli());
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

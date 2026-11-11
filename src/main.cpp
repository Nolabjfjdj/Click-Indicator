#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    struct ClickEvent {
        int start;
        int duration;
    };

    std::vector<ClickEvent> recordedClicks;
    int activePressStart = -1;
    std::string activeRecordingKey;
    bool playbackDiagnosticLogged = false;
    bool playbackBarDiagnosticLogged = false;
    bool playbackUpdateDiagnosticLogged = false;

    constexpr float indicatorWidth = 34.f;
    constexpr float indicatorHeight = 180.f;
    constexpr float playbackPixelsPerMs = 0.09f;
    constexpr int visiblePlaybackBars = 16;

    CCLayerColor* makeRect(ccColor4B color, float width, float height) {
        return CCLayerColor::create(color, width, height);
    }

    std::string makeRecordingKey(int levelID) {
        return "click-recording-" + std::to_string(levelID);
    }

    void saveRecordedClicks() {
        if (activeRecordingKey.empty()) {
            return;
        }

        std::string data;
        for (auto const& click : recordedClicks) {
            if (!data.empty()) {
                data += ";";
            }
            data += std::to_string(click.start);
            data += ",";
            data += std::to_string(click.duration);
        }

        Mod::get()->setSavedValue<std::string>(activeRecordingKey, data);
    }

    void loadRecordedClicks(std::string const& key) {
        recordedClicks.clear();
        activeRecordingKey = key;
        auto data = Mod::get()->getSavedValue<std::string>(key, "");

        size_t position = 0;
        while (position < data.size()) {
            auto separator = data.find(';', position);
            auto item = data.substr(position, separator == std::string::npos ? std::string::npos : separator - position);
            auto comma = item.find(',');
            if (comma != std::string::npos) {
                try {
                    auto start = std::stoi(item.substr(0, comma));
                    auto duration = std::stoi(item.substr(comma + 1));
                    if (start >= 0 && duration >= 0) {
                        recordedClicks.push_back({start, duration});
                    }
                }
                catch (...) {
                }
            }
            if (separator == std::string::npos) {
                break;
            }
            position = separator + 1;
        }

        log::info("Loaded {} recorded clicks for playback", recordedClicks.size());
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

        if (!playbackDiagnosticLogged) {
            playbackDiagnosticLogged = true;
            log::info("Playback diagnostic: timer={} ms, recorded={}, indicator={}, firstBar={}", now, recordedClicks.size(), indicator != nullptr, indicator->getChildByTag(600) != nullptr);
            if (!recordedClicks.empty()) {
                log::info("Playback diagnostic: firstClick={} ms, delta={} ms", recordedClicks.front().start, recordedClicks.front().start - now);
            }
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
                auto visible = y >= 4.f && y <= indicatorHeight - 7.f;
                bar->setPosition({4.f, y});
                bar->setVisible(visible);
                if (barIndex == 0 && !playbackBarDiagnosticLogged) {
                    playbackBarDiagnosticLogged = true;
                    log::info("Playback diagnostic: nextClick={} ms, delta={} ms, y={}, visible={}", click.start, delta, y, visible);
                }
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

        activePressStart = -1;
        auto recordingEnabled = Mod::get()->getSettingValue<bool>("recording");
        auto playbackEnabled = Mod::get()->getSettingValue<bool>("playback");
        auto recordingKey = makeRecordingKey(level->m_levelID);

        if (recordingEnabled) {
            recordedClicks.clear();
            activeRecordingKey = recordingKey;
            saveRecordedClicks();
        }
        else if (playbackEnabled) {
            loadRecordedClicks(recordingKey);
        }
        else {
            recordedClicks.clear();
            activeRecordingKey = recordingKey;
        }

        log::info("Click recording is {}", recordingEnabled ? "ENABLED" : "DISABLED");
        log::info("Click playback is {}", playbackEnabled ? "ENABLED" : "DISABLED");

        if (!Mod::get()->getSettingValue<bool>("enabled")) {
            return true;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto indicator = createClickIndicator(playbackEnabled);
        indicator->setTag(415);
        log::info("Playback diagnostic: indicator created, playback={}, bars={}", playbackEnabled, playbackEnabled ? visiblePlaybackBars : 0);
        auto rightSide = Mod::get()->getSettingValue<bool>("right_side");
        indicator->setPosition({
            rightSide ? winSize.width - 62.f : 28.f,
            (winSize.height - indicatorHeight) / 2.f
        });
        this->addChild(indicator, 9999);

        return true;
    }

};

$on_mod(Loaded) {
    log::info("Click Indicator loaded");
}

class $modify(ClickIndicatorInputLayer, GJBaseGameLayer) {
    void update(float dt) {
        GJBaseGameLayer::update(dt);

        if (Mod::get()->getSettingValue<bool>("playback") &&
            !Mod::get()->getSettingValue<bool>("recording")) {
            auto indicator = this->getChildByTag(415);
            if (!playbackUpdateDiagnosticLogged) {
                playbackUpdateDiagnosticLogged = true;
                log::info("Playback diagnostic: GJBaseGameLayer update reached, timer={} ms, indicatorFound={}, recorded={}", this->getPlayTimerMilli(), indicator != nullptr, recordedClicks.size());
            }
            updatePlaybackIndicator(indicator, this->getPlayTimerMilli());
        }
    }

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
                    saveRecordedClicks();
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

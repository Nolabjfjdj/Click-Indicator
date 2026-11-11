#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
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
        timingBar->setPosition({-7.f, -5.f});
        root->addChild(timingBar);

        return root;
    }
}

class $modify(ClickIndicatorPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        if (!Mod::get()->getSettingValue<bool>("enabled")) {
            return true;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto indicator = createClickIndicator();
        auto rightSide = Mod::get()->getSettingValue<bool>("right_side");
        indicator->setPosition({
            rightSide ? winSize.width - 62.f : 28.f,
            (winSize.height - 180.f) / 2.f
        });
        this->addChild(indicator, 9999);

        return true;
    }
};

$on_mod(Loaded) {
    log::info("Click Indicator loaded");
}

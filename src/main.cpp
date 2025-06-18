#include <Geode/utils/web.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelCell.hpp>

using namespace geode::prelude;

static std::unordered_map<int, std::string> links;

void parseLinks(const std::string& str) {
    if (str.empty()) return;
    
    links.clear();
    
    std::istringstream iss(str);
    std::string line;

    while (getline(iss, line)) {
        size_t delimiter_pos = line.find('|');
        if (delimiter_pos == std::string::npos) continue;

        int id = numFromString<int>(line.substr(0, delimiter_pos)).unwrapOr(0);
        if (id == 0) continue;

        std::string link = line.substr(delimiter_pos + 1);
        if (link.empty()) return;

        links[id] = link;
    }
}

void loadLinks(bool startup = false) {
    parseLinks(Mod::get()->getSavedValue<std::string>("saved-string"));
    
    auto req = web::WebRequest();
    
    req.header("Content-Type", "application/json");

    req.get("https://raw.githubusercontent.com/ZiLko/Level-Showcases-Links/main/links").listen([startup] (web::WebResponse* e) { // wa
        auto res = e->string();

        std::string linksString = res.unwrapOr("");
        
        bool err = linksString.size() < 100;

        if (err && links.empty()) {
            if (startup)
                Notification::create("Level Showcases: Failed to load showcases.", NotificationIcon::Error)->show();
            
            return log::error("Failed to load showcases (Startup: {}): {}", startup, res.unwrapErr());
        } else if (!err) {
            linksString = res.unwrap();
            Mod::get()->setSavedValue("saved-string", linksString);
        }
        
        parseLinks(linksString);
        
    });
}

$on_mod(Loaded) {

    if (!Mod::get()->getSettingValue<bool>("disable"))
        loadLinks(true);

    geode::listenForSettingChanges("disable", +[](bool value) {
        if (!value)
            loadLinks(false);
    });
    
};

class $modify(MyLevelInfoLayer, LevelInfoLayer) {

    void onShowcase(CCObject*) {
        geode::utils::web::openLinkInBrowser("https://www.youtube.com/watch?v=" + links.at(m_level->m_levelID.value()));
    }

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        if (Mod::get()->getSettingValue<bool>("disable")) return true;

        if (!links.contains(level->m_levelID.value())) return true;

        Loader::get()->queueInMainThread([this] {
            CCNode* lbl = getChildByID("title-label");
            if (!lbl) return;

            CCNode* menu = getChildByID("other-menu");
            if (!menu) return;

            CCNode* garageMenu = getChildByID("garage-menu");
            if (!garageMenu) return;

            CCNode* garageButton = garageMenu->getChildByID("garage-button");
            if (!garageButton) return;

            CCSprite* spr = CCSprite::createWithSpriteFrameName("gj_ytIcon_001.png");
            spr->setScale(0.65f);

            CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyLevelInfoLayer::onShowcase));
            btn->setID("showcase-button"_spr);

            menu->addChild(btn);

            float labelEdge = lbl->getPosition().x + lbl->getContentSize().width * lbl->getScale() / 2.f;
            float buttonOffset = btn->getContentSize().width / 2.f + 3.f;

            float garagePos = garageMenu->getPosition().x - (garageMenu->getContentSize().width * (garageMenu->getLayout() ? 0.5f : 0.f));
            float buttonLeftEdge = garagePos + garageButton->getPosition().x - (garageButton->getContentSize().width / 2.f);
            float extra = labelEdge + 6.f + btn->getContentSize().width - buttonLeftEdge;

            if (extra > 0) {
                float targetWidth = (buttonLeftEdge - lbl->getPosition().x - 6.f - btn->getContentSize().width) * 2;
                lbl->setScale(targetWidth / lbl->getContentSize().width);
            } else
                extra = 0;

            labelEdge = lbl->getPosition().x + lbl->getContentSize().width * lbl->getScale() / 2.f;

            if (CCNode* dailyLbl = getChildByID("daily-label")) {
                dailyLbl->setPositionX(dailyLbl->getPositionX() + btn->getContentSize().width + 4.f - extra);
                dailyLbl->setZOrder(dailyLbl->getZOrder() + 1);
            }

            btn->setPositionX(labelEdge + buttonOffset);
            btn->setPositionY(lbl->getPosition().y);
            btn->setPosition(btn->getPosition() - menu->getPosition());
        });

        return true;
    }

};

class $modify(MyLevelCell, LevelCell) {

    struct Fields {
        CCSprite* m_showcaseIcon = nullptr;

        CCNode* m_mainLayer = nullptr;
        CCNode* m_copyIcon = nullptr;
        CCNode* m_objectIcon = nullptr;
        CCNode* m_maxRight = nullptr;
        CCNode* m_mainMenu = nullptr;
        CCNode* m_creatorName = nullptr;

        bool m_didSchedule = false;
    };

    void setIconPosition(float) {
        auto f = m_fields.self();

        float scale = 1.f;
        cocos2d::CCPoint pos = {0, 0};

        CCNode* maxRight = f->m_objectIcon ? f->m_objectIcon : f->m_copyIcon;

        if (f->m_copyIcon || f->m_objectIcon) {
            if (f->m_copyIcon)
                if (f->m_copyIcon->getPositionX() > maxRight->getPositionX())
                    maxRight = f->m_copyIcon;

            scale = maxRight->getContentSize().width * maxRight->getScale() / f->m_showcaseIcon->getContentSize().width;
            pos = maxRight->getPosition() + ccp(15, 0);
        } else {
            scale = getContentSize().height < 80 ? 0.8f : 1.f;

            if (f->m_mainMenu)
                if (f->m_creatorName) {
                    pos = f->m_mainMenu->getPosition() + f->m_creatorName->getPosition();
                    pos += ccp(f->m_creatorName->getContentSize().width / 2.f + 5, -1);
                }
        }

        if (f->m_mainMenu)
            if (f->m_creatorName) {
                std::string name = static_cast<CCLabelBMFont*>(f->m_creatorName->getChildByType<CCLabelBMFont>(0))->getString();
                if (name == "By -" && Loader::get()->isModLoaded("cvolton.betterinfo")) {
                    if (!f->m_didSchedule)
                        schedule(schedule_selector(MyLevelCell::setIconPosition), 0.1f);
                    
                    f->m_didSchedule = true;
                } else
                    unschedule(schedule_selector(MyLevelCell::setIconPosition));
            }

        if (pos == ccp(0, 0)) return f->m_showcaseIcon->setVisible(false);

        f->m_showcaseIcon->setPosition(pos);
        f->m_showcaseIcon->setScale(scale);
        f->m_showcaseIcon->setVisible(true);
    }

    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);

        if (Mod::get()->getSettingValue<bool>("disable") || Mod::get()->getSettingValue<bool>("disable-icon")) return;
        if (!links.contains(level->m_levelID.value())) return;

        Loader::get()->queueInMainThread([this] {
            auto f = m_fields.self();

            f->m_mainLayer = getChildByID("main-layer");
            if (!f->m_mainLayer) return;
            
            f->m_mainLayer = getChildByID("main-layer");
            f->m_copyIcon = f->m_mainLayer->getChildByID("copy-indicator");
            f->m_objectIcon = f->m_mainLayer->getChildByID("high-object-indicator");

            f->m_mainMenu = f->m_mainLayer->getChildByID("main-menu");

            if (f->m_mainMenu)
                f->m_creatorName = f->m_mainMenu->getChildByID("creator-name");

            f->m_showcaseIcon = CCSprite::create("showcase-icon.png"_spr);
            f->m_showcaseIcon->setAnchorPoint({0, 0.5f});
            f->m_showcaseIcon->setID("showcase-indicator"_spr);

            f->m_mainLayer->addChild(f->m_showcaseIcon);

            setIconPosition(0.f);
        });
    }

};

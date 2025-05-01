#include <Geode/utils/web.hpp>

#include <Geode/modify/LevelInfoLayer.hpp>

using namespace geode::prelude;

static std::unordered_map<int, std::string> links;

void loadLinks(bool startup = false) {
    auto req = web::WebRequest();

    req.header("Content-Type", "application/json");

    req.get("https://raw.githubusercontent.com/ZiLko/Level-Showcases-Links/main/links").listen([startup] (web::WebResponse* e) { // wa
        auto res = e->string();

        std::string linksString = "";

        if (res.isErr()) {
            if (Mod::get()->hasSavedValue("saved-string"))
                linksString = Mod::get()->getSavedValue<std::string>("saved-string");
            else {
                if (startup) Notification::create("Level Showcases: Failed to load showcases.", NotificationIcon::Error)->show();
                return log::error("Failed to load showcases (Startup: {}): {}", startup, res.unwrapErr());
            }
        }

        links.clear();

        linksString = res.unwrapOr("");

        Mod::get()->setSavedValue("saved-string", linksString);

        std::istringstream iss(linksString);
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

        if (Mod::get()->getSettingValue<bool>("disabled")) return true;

        if (!links.contains(level->m_levelID.value())) return true;

        if (CCNode* lbl = getChildByID("title-label")) {
            CCSprite* spr = CCSprite::createWithSpriteFrameName("gj_ytIcon_001.png");
            spr->setScale(0.625f);
            CCMenuItemSpriteExtra* btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyLevelInfoLayer::onShowcase));
            btn->setAnchorPoint({0, 0.5f});
            
            if (CCNode* menu = getChildByID("other-menu")) {
                menu->addChild(btn);
                btn->setPositionX(lbl->getPosition().x + lbl->getContentSize().width * lbl->getScale() * 0.5f + 3.f);
                btn->setPositionY(lbl->getPosition().y);
                btn->setPosition(btn->getPosition() - menu->getPosition() - ccp(0, 1.4));
            }
        }

        return true;
    }

};
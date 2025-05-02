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
            }

            labelEdge = lbl->getPosition().x + lbl->getContentSize().width * lbl->getScale() / 2.f;

            btn->setPositionX(labelEdge + buttonOffset);
            btn->setPositionY(lbl->getPosition().y);
            btn->setPosition(btn->getPosition() - menu->getPosition());
        });

        return true;
    }

};
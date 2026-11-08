#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

bool g_autoplayEnabled = false;
bool g_isBotDoingAction = false;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        PlayerObject* m_ghostPlayer = nullptr;
        cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
        int m_bufferSize = 15;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        
        m_fields->m_statusLabel = cocos2d::CCLabelBMFont::create("AUTOPLAY ON", "goldFont.fnt");
        m_fields->m_statusLabel->setPosition({ winSize.width / 2, 40 });
        m_fields->m_statusLabel->setScale(0.7f);
        m_fields->m_statusLabel->setVisible(g_autoplayEnabled);
        this->addChild(m_fields->m_statusLabel, 100);

        m_fields->m_ghostPlayer = PlayerObject::create(1, 1, this, false);
        if (m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer->retain();
            m_fields->m_ghostPlayer->setVisible(false);
            this->addChild(m_fields->m_ghostPlayer);
        }

        return true;
    }

    void onQuit() {
        if (m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer->release();
        }
        PlayLayer::onQuit();
    }

    void syncGhost() {
        auto p1 = this->m_player1;
        auto g = m_fields->m_ghostPlayer;
        if (!p1 || !g) return;

        g->setPosition(p1->getPosition());
        g->setRotation(p1->getRotation());
        g->addToYVelocity(p1->getYVelocity() - g->getYVelocity());
        
        g->m_isUpsideDown = p1->m_isUpsideDown;
        g->m_isShip = p1->m_isShip;
        g->m_isBird = p1->m_isBird;
        g->m_isBall = p1->m_isBall;
        g->m_isDart = p1->m_isDart;
        g->m_isRobot = p1->m_isRobot;
        g->m_isSpider = p1->m_isSpider;
        g->m_isSwing = p1->m_isSwing;
        g->m_isDead = false;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        
        if (m_fields->m_statusLabel) {
            m_fields->m_statusLabel->setVisible(g_autoplayEnabled);
        }

        if (!g_autoplayEnabled || !this->m_player1 || this->m_player1->m_isDead) return;

        syncGhost();

        bool shouldJump = false;
        bool shouldRelease = false;
        auto g = m_fields->m_ghostPlayer;
        if (!g) return;

        // Simulation
        for (int i = 0; i < m_fields->m_bufferSize; i++) {
            g->update(1.0f);
            if (g->m_isDead) {
                shouldJump = true;
                break;
            }
        }

        if (shouldJump) {
            syncGhost();
            g_isBotDoingAction = true;
            g->handleButton(true, 1, true); 
            for (int i = 0; i < m_fields->m_bufferSize; i++) {
                g->update(1.0f);
                if (g->m_isDead) {
                    shouldRelease = true;
                    break;
                }
            }
            g_isBotDoingAction = false;
        }

        g_isBotDoingAction = true;
        if (shouldJump && !shouldRelease) {
            this->handleButton(true, 1, true);
        } else {
            this->handleButton(false, 1, true);
        }
        g_isBotDoingAction = false;
    }
};

class $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        if (g_autoplayEnabled && !g_isBotDoingAction) return;
        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

class $modify(MyCCKeyboardDispatcher, cocos2d::CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool down, bool isKeyRepeat, double idk) {
        if (key == cocos2d::enumKeyCodes::KEY_K && down && !isKeyRepeat) {
            g_autoplayEnabled = !g_autoplayEnabled;
            std::string msg = g_autoplayEnabled ? "Autoplay: ON" : "Autoplay: OFF";
            Notification::create(msg, NotificationIcon::Success)->show();
        }
        return cocos2d::CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isKeyRepeat, idk);
    }
};
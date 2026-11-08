#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

bool g_autoplayEnabled = false;
bool g_isBotDoingAction = false;

struct $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        PlayerObject* m_ghostPlayer = nullptr;
        CCLabelBMFont* m_statusLabel = nullptr;
        int m_bufferSize = 15;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        m_fields->m_statusLabel = CCLabelBMFont::create("AUTOPLAY ON", "goldFont.fnt");
        m_fields->m_statusLabel->setPosition({ winSize.width / 2, 40 });
        m_fields->m_statusLabel->setScale(0.5f);
        m_fields->m_statusLabel->setZOrder(999);
        m_fields->m_statusLabel->setOpacity(0);
        this->addChild(m_fields->m_statusLabel);

        return true;
    }

    void syncGhost() {
        if (!m_fields->m_ghostPlayer || !this->m_player1) return;

        auto p1 = this->m_player1;
        auto g  = m_fields->m_ghostPlayer;

        // Position
        g->setPosition(p1->getPosition());
        g->setVisible(false);
        g->setRotation(p1->getRotation());

        // ✅ FIXED: use getYVelocity() and addToYVelocity() — no m_yAccel/m_xAccel
        double yDiff = p1->getYVelocity() - g->getYVelocity();
        g->addToYVelocity(yDiff, 0);

        // Game mode flags
        g->m_isDead       = false;
        g->m_isUpsideDown = p1->m_isUpsideDown;
        g->m_isShip       = p1->m_isShip;
        g->m_isBall       = p1->m_isBall;
        g->m_isBird       = p1->m_isBird;
        g->m_isDart       = p1->m_isDart;
        g->m_isRobot      = p1->m_isRobot;
        g->m_isSpider     = p1->m_isSpider;
        g->m_isSwing      = p1->m_isSwing;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (m_fields->m_statusLabel) {
            m_fields->m_statusLabel->setOpacity(g_autoplayEnabled ? 255 : 0);
        }

        if (!g_autoplayEnabled || !this->m_player1 || this->m_player1->m_isDead) return;

        if (!m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer = PlayerObject::create(1, 1, this, this->m_objectLayer, true);
            m_fields->m_ghostPlayer->retain();
        }

        // 1. If we do NOTHING, do we die?
        syncGhost();
        bool willDieIfNothing = false;
        for (int i = 0; i < m_fields->m_bufferSize; i++) {
            m_fields->m_ghostPlayer->update(1.0f / 60.0f);
            if (m_fields->m_ghostPlayer->m_isDead) {
                willDieIfNothing = true;
                break;
            }
        }

        if (willDieIfNothing) {
            g_isBotDoingAction = true;
            this->handleButton(true, (int)PlayerButton::Jump, true);
            g_isBotDoingAction = false;
        } else {
            // 2. If we HOLD, do we die? (Ship/Wave/UFO)
            syncGhost();
            m_fields->m_ghostPlayer->pushButton(PlayerButton::Jump);

            bool willDieIfHolding = false;
            for (int i = 0; i < m_fields->m_bufferSize; i++) {
                m_fields->m_ghostPlayer->update(1.0f / 60.0f);
                if (m_fields->m_ghostPlayer->m_isDead) {
                    willDieIfHolding = true;
                    break;
                }
            }

            if (willDieIfHolding) {
                g_isBotDoingAction = true;
                this->handleButton(false, (int)PlayerButton::Jump, true);
                g_isBotDoingAction = false;
            }
        }
    }

    void onQuit() {
        if (m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer->release();
            m_fields->m_ghostPlayer = nullptr;
        }
        PlayLayer::onQuit();
    }
};

// Block manual input but allow bot actions through
struct $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        if (g_autoplayEnabled) {
            if (g_isBotDoingAction) {
                GJBaseGameLayer::handleButton(down, button, player1);
            }
            return;
        }
        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

// Toggle with K key
struct $modify(MyCCKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isKeyRepeat, double idk) {
        if (down && !isKeyRepeat && key == enumKeyCodes::KEY_K) {
            g_autoplayEnabled = !g_autoplayEnabled;
            std::string msg = g_autoplayEnabled ? "Autoplay ON" : "Autoplay OFF";
            Notification::create(msg, NotificationIcon::Info)->show();
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isKeyRepeat, idk);
    }
};
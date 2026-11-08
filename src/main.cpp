#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

// Global variable so it's easy to access and toggle
bool g_autoplayEnabled = false;

struct $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        PlayerObject* m_ghostPlayer = nullptr;
        CCLabelBMFont* m_statusLabel = nullptr;
        int m_bufferSize = 10;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        m_fields->m_statusLabel = CCLabelBMFont::create("AUTOPLAY ON", "goldFont.fnt");
        m_fields->m_statusLabel->setPosition({ winSize.width / 2, 40 });
        m_fields->m_statusLabel->setScale(0.5f);
        m_fields->m_statusLabel->setZOrder(999);
        this->addChild(m_fields->m_statusLabel);

        return true;
    }

    void syncGhost() {
        if (!m_fields->m_ghostPlayer || !this->m_player1) return;

        auto p1 = this->m_player1;
        auto g  = m_fields->m_ghostPlayer;

        g->setPosition(p1->getPosition());
        g->setVisible(false); // Make sure ghost is hidden

        // Sync physics and state
        g->addToYVelocity(p1->getYVelocity() - g->getYVelocity(), 0);
        g->m_isDead = false;
        g->m_isUpsideDown = p1->m_isUpsideDown;
        g->m_isShip = p1->m_isShip;
        g->m_isBall = p1->m_isBall;
        g->m_isBird = p1->m_isBird;
        g->m_isDart = p1->m_isDart;
        g->m_isRobot = p1->m_isRobot;
        g->m_isSpider = p1->m_isSpider;
        g->m_isSwing = p1->m_isSwing;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // Update Label visibility based on global toggle
        if (m_fields->m_statusLabel) {
            m_fields->m_statusLabel->setOpacity(g_autoplayEnabled ? 255 : 0);
        }

        if (!g_autoplayEnabled || !this->m_player1 || this->m_player1->m_isDead) {
            return;
        }

        // Create ghost if needed
        if (!m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer = PlayerObject::create(1, 1, this, this->m_objectLayer, true);
            m_fields->m_ghostPlayer->retain();
        }

        // --- PREDICTION LOGIC ---
        bool shouldJump = false;

        // 1. Check for immediate death
        syncGhost();
        for (int i = 0; i < m_fields->m_bufferSize; i++) {
            m_fields->m_ghostPlayer->update(1.0f / 60.0f);
            if (m_fields->m_ghostPlayer->m_isDead) {
                shouldJump = true;
                break;
            }
        }

        if (shouldJump) {
            this->handleButton(true, (int)PlayerButton::Jump, true);
        } else {
            // 2. Check if holding causes death (for Ship/Wave)
            syncGhost();
            m_fields->m_ghostPlayer->pushButton(PlayerButton::Jump);
            bool holdIsBad = false;
            for (int i = 0; i < m_fields->m_bufferSize; i++) {
                m_fields->m_ghostPlayer->update(1.0f / 60.0f);
                if (m_fields->m_ghostPlayer->m_isDead) {
                    holdIsBad = true;
                    break;
                }
            }
            if (holdIsBad) {
                this->handleButton(false, (int)PlayerButton::Jump, true);
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

// Block manual input when enabled
struct $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        if (g_autoplayEnabled && PlayLayer::get()) return;
        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

// Toggle with 'K' key
struct $modify(MyCCKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isKeyRepeat, double idk) {
        if (down && !isKeyRepeat && key == enumKeyCodes::KEY_K) {
            g_autoplayEnabled = !g_autoplayEnabled;
            
            // Show a notification so you KNOW it toggled
            std::string msg = g_autoplayEnabled ? "Autoplay ON" : "Autoplay OFF";
            Notification::create(msg, NotificationIcon::Info)->show();
            
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isKeyRepeat, idk);
    }
};
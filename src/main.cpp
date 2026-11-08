#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

// Global settings
bool g_autoplayEnabled = false;
bool g_isBotDoingAction = false; // Prevents the mod from blocking itself

struct $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        PlayerObject* m_ghostPlayer = nullptr;
        CCLabelBMFont* m_statusLabel = nullptr;
        int m_bufferSize = 15; // Increased look-ahead for better reaction
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

    // This function copies your current state to the invisible "Ghost"
    void syncGhost() {
        if (!m_fields->m_ghostPlayer || !this->m_player1) return;

        auto p1 = this->m_player1;
        auto g  = m_fields->m_ghostPlayer;

        g->setPosition(p1->getPosition());
        g->setVisible(false);
        
        // Sync Physics
        g->m_yAccel = p1->m_yAccel;
        g->m_xAccel = p1->m_xAccel;
        g->setRotation(p1->getRotation());
        
        // Sync Mode Flags
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

        if (m_fields->m_statusLabel) {
            m_fields->m_statusLabel->setOpacity(g_autoplayEnabled ? 255 : 0);
        }

        if (!g_autoplayEnabled || !this->m_player1 || this->m_player1->m_isDead) return;

        if (!m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer = PlayerObject::create(1, 1, this, this->m_objectLayer, true);
            m_fields->m_ghostPlayer->retain();
        }

        // --- THE BRAIN ---
        
        // 1. Check: If we do NOTHING, do we die?
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
            // JUMP!
            g_isBotDoingAction = true;
            this->handleButton(true, (int)PlayerButton::Jump, true);
            g_isBotDoingAction = false;
        } else {
            // 2. Check: If we are currently HOLDING, will that kill us? (Ship/Wave)
            syncGhost();
            // Simulate the ghost holding the button
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
                // RELEASE!
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

// Fixed Input Blocker
struct $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        // If Autoplay is ON...
        if (g_autoplayEnabled) {
            // ...ONLY allow the button if the BOT is the one pressing it
            if (g_isBotDoingAction) {
                GJBaseGameLayer::handleButton(down, button, player1);
            }
            // Otherwise, ignore the human input
            return;
        }
        
        // Autoplay is OFF, let human play normally
        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

// Toggle with 'K'
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
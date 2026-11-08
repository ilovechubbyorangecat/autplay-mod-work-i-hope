#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

/* --- Fields for Logic --- */
struct $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_enabled = false;
        PlayerObject* m_ghostPlayer = nullptr;
        CCLabelBMFont* m_statusLabel = nullptr;
        int m_bufferSize = 10; // How many frames to look ahead
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;

        // Create the "AUTOPLAY ON" label (hidden by default)
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        m_fields->m_statusLabel = CCLabelBMFont::create("AUTOPLAY ON", "goldFont.fnt");
        m_fields->m_statusLabel->setPosition({ winSize.width / 2, 40 });
        m_fields->m_statusLabel->setScale(0.5f);
        m_fields->m_statusLabel->setOpacity(0); // Hidden
        m_fields->m_statusLabel->setZOrder(999);
        this->addChild(m_fields->m_statusLabel);

        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!m_fields->m_enabled || this->m_isDead) {
            if (m_fields->m_statusLabel) m_fields->m_statusLabel->setOpacity(0);
            return;
        }

        // Show label
        m_fields->m_statusLabel->setOpacity(255);

        // Create ghost if it doesn't exist
        if (!m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer = PlayerObject::create(1, 1, this, this->m_objectLayer, true);
            m_fields->m_ghostPlayer->retain();
        }

        // --- PREDICTION LOGIC ---
        
        // 1. Check if we need to jump (Ghost simulation)
        bool shouldJump = false;

        // Sync ghost to real player
        syncGhost();

        // Simulate frames ahead
        for (int i = 0; i < m_fields->m_bufferSize; i++) {
            m_fields->m_ghostPlayer->update(1.0f / 60.0f);
            
            // If ghost hits something, we definitely need to try jumping
            if (m_fields->m_ghostPlayer->m_isDead) {
                shouldJump = true;
                break;
            }
        }

        // 2. Apply the action to the real player
        if (shouldJump) {
            this->handleButton(true, (int)PlayerButton::Jump, true);
        } else {
            // If not dead, we check if holding is bad
            syncGhost();
            m_fields->m_ghostPlayer->pushButton(PlayerButton::Jump);
            
            bool holdingIsSafe = true;
            for (int i = 0; i < m_fields->m_bufferSize; i++) {
                m_fields->m_ghostPlayer->update(1.0f / 60.0f);
                if (m_fields->m_ghostPlayer->m_isDead) {
                    holdingIsSafe = false;
                    break;
                }
            }

            if (!holdingIsSafe) {
                this->handleButton(false, (int)PlayerButton::Jump, true);
            }
        }
    }

    void syncGhost() {
        if (!m_fields->m_ghostPlayer || !this->m_player1) return;
        
        auto p1 = this->m_player1;
        auto g = m_fields->m_ghostPlayer;

        g->setPosition(p1->getPosition());
        g->m_yAccel = p1->m_yAccel;
        g->m_xAccel = p1->m_xAccel;
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

    void onQuit() {
        if (m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer->release();
            m_fields->m_ghostPlayer = nullptr;
        }
        PlayLayer::onQuit();
    }
};

/* --- Block Manual Input while Autoplay is ON --- */
struct $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        auto pl = PlayLayer::get();
        if (pl) {
            auto fields = static_cast<MyPlayLayer*>(static_cast<PlayLayer*>(pl));
            // If autoplay is ON, block manual player input so they don't fight
            if (fields->m_fields->m_enabled) {
                // We only let the mod's own calls through (this is a simplified check)
                // In actual logic, this blocks clicking the mouse/screen.
                return;
            }
        }
        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

/* --- Toggle via Backtick (`) --- */
struct $modify(MyCCKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isKeyRepeat, double idk) {
        if (down && key == enumKeyCodes::KEY_Backtick) {
            if (auto pl = PlayLayer::get()) {
                auto fields = static_cast<MyPlayLayer*>(static_cast<PlayLayer*>(pl));
                fields->m_fields->m_enabled = !fields->m_fields->m_enabled;
                
                // Visual feedback
                auto label = fields->m_fields->m_statusLabel;
                if (label) {
                    label->setOpacity(fields->m_fields->m_enabled ? 255 : 0);
                }
            }
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isKeyRepeat, idk);
    }
};
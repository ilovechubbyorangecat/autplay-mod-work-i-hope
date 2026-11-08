#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

// Backtick key virtual keycode (0xC0 = 192 on Windows/Cocos)
static const enumKeyCodes KEY_GRAVE = (enumKeyCodes)0xC0;

struct $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_enabled = false;
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
        m_fields->m_statusLabel->setOpacity(0);
        m_fields->m_statusLabel->setZOrder(999);
        this->addChild(m_fields->m_statusLabel);

        return true;
    }

    void syncGhost() {
        if (!m_fields->m_ghostPlayer || !this->m_player1) return;

        auto p1 = this->m_player1;
        auto g  = m_fields->m_ghostPlayer;

        // Position
        g->setPosition(p1->getPosition());

        // Velocity — use the proper getter/setter methods
        g->addToYVelocity(p1->getYVelocity() - g->getYVelocity(), 0);

        // Game mode flags (these ARE in the bindings)
        g->m_isDead      = false;
        g->m_isUpsideDown = p1->m_isUpsideDown;
        g->m_isShip      = p1->m_isShip;
        g->m_isBall      = p1->m_isBall;
        g->m_isBird      = p1->m_isBird;
        g->m_isDart      = p1->m_isDart;
        g->m_isRobot     = p1->m_isRobot;
        g->m_isSpider    = p1->m_isSpider;
        g->m_isSwing     = p1->m_isSwing;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // Hide label if disabled or player is dead
        if (!m_fields->m_enabled ||
            !this->m_player1 ||
            this->m_player1->m_isDead)
        {
            if (m_fields->m_statusLabel)
                m_fields->m_statusLabel->setOpacity(0);
            return;
        }

        m_fields->m_statusLabel->setOpacity(255);

        // Create ghost once
        if (!m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer = PlayerObject::create(
                1, 1, this, this->m_objectLayer, true
            );
            m_fields->m_ghostPlayer->retain();
        }

        // --- Predict: should we jump? ---
        bool shouldJump = false;

        syncGhost();
        for (int i = 0; i < m_fields->m_bufferSize; i++) {
            m_fields->m_ghostPlayer->update(1.0f / 60.0f);
            if (m_fields->m_ghostPlayer->m_isDead) {
                shouldJump = true;
                break;
            }
        }

        if (shouldJump) {
            // Press jump
            this->handleButton(true, (int)PlayerButton::Jump, true);
        } else {
            // Check if holding jump causes death
            syncGhost();
            m_fields->m_ghostPlayer->pushButton(PlayerButton::Jump);

            bool holdSafe = true;
            for (int i = 0; i < m_fields->m_bufferSize; i++) {
                m_fields->m_ghostPlayer->update(1.0f / 60.0f);
                if (m_fields->m_ghostPlayer->m_isDead) {
                    holdSafe = false;
                    break;
                }
            }

            if (!holdSafe) {
                // Release jump
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

// Block manual input while autoplay is on
struct $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        auto pl = PlayLayer::get();
        if (pl) {
            auto fields = static_cast<MyPlayLayer*>(static_cast<PlayLayer*>(pl));
            if (fields->m_fields->m_enabled) return; // block manual input
        }
        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

// Toggle with backtick
struct $modify(MyCCKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isKeyRepeat, double idk) {
        if (down && !isKeyRepeat && key == KEY_GRAVE) {
            if (auto pl = PlayLayer::get()) {
                auto fields = static_cast<MyPlayLayer*>(static_cast<PlayLayer*>(pl));
                fields->m_fields->m_enabled = !fields->m_fields->m_enabled;

                if (fields->m_fields->m_statusLabel) {
                    fields->m_fields->m_statusLabel->setOpacity(
                        fields->m_fields->m_enabled ? 255 : 0
                    );
                }
            }
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isKeyRepeat, idk);
    }
};
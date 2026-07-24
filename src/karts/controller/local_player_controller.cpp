//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2015 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006-2015 Joerg Henrichs, Steve Baker
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "karts/controller/local_player_controller.hpp"

#include "audio/sfx_base.hpp"
#include "config/player_manager.hpp"
#include "config/stk_config.hpp"
#include "config/user_config.hpp"
#include "graphics/camera/camera.hpp"
#include "graphics/camera/camera_normal.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/particle_emitter.hpp"
#include "graphics/particle_kind.hpp"
#include "input/input_manager.hpp"
#include "items/attachment.hpp"
#include "items/item.hpp"
#include "items/powerup.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/ai_base_controller.hpp"
#include "karts/controller/battle_ai.hpp"
#include "karts/controller/player_controller.hpp"
#include "karts/controller/soccer_ai.hpp"
#include "karts/controller/skidding_ai.hpp"
#include "karts/kart_properties.hpp"
#include "karts/skidding.hpp"
#include "karts/rescue_animation.hpp"
#include "modes/world.hpp"
#include "network/network_config.hpp"
#include "network/protocols/game_events_protocol.hpp"
#include "network/protocols/game_protocol.hpp"
#include "network/race_event_manager.hpp"
#include "network/rewind_manager.hpp"
#include "race/history.hpp"
#include "race/race_manager.hpp"
#include "states_screens/race_gui_base.hpp"
#include "tracks/drive_graph.hpp"
#include "tracks/track.hpp"
#include "utils/constants.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

#include "input/device_manager.hpp"
#include "input/gamepad_device.hpp"
#include "input/sdl_controller.hpp"

#include "LinearMath/btTransform.h"

namespace
{
AIBaseController* createAutoDriveAI(AbstractKart* kart)
{
    if (RaceManager::get()->isBattleMode())
        return new BattleAI(kart);
    if (RaceManager::get()->isSoccerMode())
        return new SoccerAI(kart);
    return new SkiddingAI(kart);
}
}

/** The constructor for a local player kart, i.e. a player that is playing
 *  on this machine (non-local player would be network clients).
 *  \param kart_name Name of the kart.
 *  \param position The starting position (1 to n).
 *  \param player The player to which this kart belongs.
 *  \param init_pos The start coordinates and heading of the kart.
 */
LocalPlayerController::LocalPlayerController(AbstractKart *kart,
                                             const int local_player_id,
                                             HandicapLevel h)
                     : PlayerController(kart)
{
    m_last_crash = 0;
    m_has_started = false;
    m_handicap = h;
    m_player = StateManager::get()->getActivePlayer(local_player_id);
    if(m_player)
        m_player->setKart(kart);

    // Keep a pointer to the camera to remove the need to search for
    // the right camera once per frame later.
    m_camera_index = -1;
    if (!GUIEngine::isNoGraphics())
    {
        Camera *camera = Camera::createCamera(kart, local_player_id);
        m_camera_index = camera->getIndex();
    }

    m_wee_sound    = SFXManager::get()->createSoundSource("wee");
    m_bzzt_sound   = SFXManager::get()->getBuffer("bzzt");
    m_ugh_sound    = SFXManager::get()->getBuffer("ugh");
    m_grab_sound   = SFXManager::get()->getBuffer("grab_collectable");
    m_full_sound   = SFXManager::get()->getBuffer("energy_bar_full");
    m_unfull_sound = SFXManager::get()->getBuffer("energy_bar_unfull");

    m_is_above_nitro_target = false;
    m_auto_drive_ai = NULL;
    m_auto_drive_controls = NULL;
    m_auto_drive_active = false;
    m_auto_drive_wanted = UserConfigParams::m_multitouch_auto_drive;
    m_player_fire = false;
    m_player_look_back = false;
    m_auto_drive_stuck_time = 0.0f;
    m_auto_drive_last_pos = Vec3(0,0,0);
    initParticleEmitter();
}   // LocalPlayerController

//-----------------------------------------------------------------------------
/** Destructor for a player kart.
 */
LocalPlayerController::~LocalPlayerController()
{
    m_wee_sound->deleteSFX();
    if (m_auto_drive_ai)
        delete m_auto_drive_ai;
    delete m_auto_drive_controls;
}   // ~LocalPlayerController

//-----------------------------------------------------------------------------
void LocalPlayerController::initParticleEmitter()
{
    // Attach Particle System
#ifndef SERVER_ONLY
    m_sky_particles_emitter = nullptr;
    Track *track = Track::getCurrentTrack();
    if (!GUIEngine::isNoGraphics() &&
        UserConfigParams::m_particles_effects > 1 &&
        track->getSkyParticles() != NULL)
    {
        track->getSkyParticles()->setBoxSizeXZ(150.0f, 150.0f);

        m_sky_particles_emitter.reset(
            new ParticleEmitter(track->getSkyParticles(),
                                core::vector3df(0.0f, 30.0f, 100.0f),
                                m_kart->getNode(),
                                true));

        // FIXME: in multiplayer mode, this will result in several instances
        //        of the heightmap being calculated and kept in memory
        m_sky_particles_emitter->addHeightMapAffector(track);
    }
#endif
}   // initParticleEmitter

//-----------------------------------------------------------------------------
/** Resets the player kart for a new or restarted race.
 */
void LocalPlayerController::reset()
{
    PlayerController::reset();
    m_last_crash = 0;
    m_sound_schedule = false;
    m_has_started = false;
    m_auto_drive_active = false;
    m_player_fire = false;
    m_player_look_back = false;
    m_auto_drive_stuck_time = 0.0f;
    if (m_auto_drive_ai)
        m_auto_drive_ai->reset();
    if (m_auto_drive_controls)
        m_auto_drive_controls->reset();
}   // reset

// ----------------------------------------------------------------------------
/** Resets the state of control keys. This is used after the in-game menu to
 *  avoid that any keys pressed at the time the menu is opened are still
 *  considered to be pressed.
 */
void LocalPlayerController::resetInputState()
{
    PlayerController::resetInputState();
    m_sound_schedule = false;
    m_player_fire = false;
    m_player_look_back = false;
}   // resetInputState

// ----------------------------------------------------------------------------
/** This function interprets a kart action and value, and set the corresponding
 *  entries in the kart control data structure. This function handles esp.
 *  cases like 'press left, press right, release right' - in this case after
 *  releasing right, the steering must switch to left again. Similarly it
 *  handles 'press left, press right, release left' (in which case still
 *  right must be selected). Similarly for braking and acceleration.
 * \param action  The action to be executed.
 * \param value   If 32768, it indicates a digital value of 'fully set'
 *                if between 1 and 32767, it indicates an analog value,
 *                and if it's 0 it indicates that the corresponding button
 *                was released.
 *  \param dry_run If set it will return if this action will trigger a
 *                 state change or not.
 *  \return       True if dry_run==true and a state change would be triggered.
 *                If dry_run==false, it returns true.
 */
bool LocalPlayerController::action(PlayerAction action, int value,
                                   bool dry_run)
{
    if (!dry_run)
    {
        if (action == PA_FIRE)
            m_player_fire = value != 0;
        else if (action == PA_LOOK_BACK)
            m_player_look_back = value != 0;
    }

    // Pause race doesn't need to be sent to server
    if (action == PA_PAUSE_RACE)
    {
        PlayerController::action(action, value);
        return true;
    }

    if (action == PA_ACCEL && value != 0 && !m_has_started)
    {
        m_has_started = true;
        if (!NetworkConfig::get()->isNetworking())
        {
            float f = m_kart->getStartupBoostFromStartTicks(
                World::getWorld()->getAuxiliaryTicks());
            m_kart->setStartupBoost(f);
        }
        else if (NetworkConfig::get()->isClient())
        {
            auto ge = RaceEventManager::get()->getProtocol();
            assert(ge);
            ge->sendStartupBoost((uint8_t)m_kart->getWorldKartId());
        }
    }

    // If this event does not change the control state (e.g.
    // it's a (auto) repeat event), do nothing. This especially
    // optimises traffic to the server and other clients.
    if (!PlayerController::action(action, value, /*dry_run*/true)) return false;

    // Register event with history
    if(!history->replayHistory())
        history->addEvent(m_kart->getWorldKartId(), action, value);

    // If this is a client, send the action to networking layer
    if (NetworkConfig::get()->isNetworking() &&
        NetworkConfig::get()->isClient() &&
        !RewindManager::get()->isRewinding() &&
        World::getWorld() && !World::getWorld()->isLiveJoinWorld())
    {
        if (auto gp = GameProtocol::lock())
        {
            gp->controllerAction(m_kart->getWorldKartId(), action, value,
                m_steer_val_l, m_steer_val_r);
        }
    }
    return PlayerController::action(action, value, /*dry_run*/false);
}   // action

//-----------------------------------------------------------------------------
/** Handles steering for a player kart.
 */
void LocalPlayerController::steer(int ticks, int steer_val)
{
    RaceGUIBase* gui_base = World::getWorld()->getRaceGUI();
    if (gui_base && UserConfigParams::m_gamepad_debug)
    {
        gui_base->clearAllMessages();
        gui_base->addMessage(StringUtils::insertValues(L"steer_val %i", steer_val),
                             m_kart, 1.0f,
                             video::SColor(255, 255, 0, 255), false);
    }
    PlayerController::steer(ticks, steer_val);

    if(UserConfigParams::m_gamepad_debug)
    {
        Log::debug("LocalPlayerController", "  set to: %f\n",
                   m_controls->getSteer());
    }
}   // steer

//-----------------------------------------------------------------------------
/** Updates the player kart, called once each timestep.
 */
void LocalPlayerController::update(int ticks)
{
    if (UserConfigParams::m_gamepad_debug)
    {
        Log::debug("LocalPlayerController", "irr_driver",
                   "-------------------------------------");
    }

    // ── ROOT CAUSE FIX ──────────────────────────────────────────
    // When auto-drive is actively steering the kart, we must NOT call
    // PlayerController::update().  Its steer() method pulls the steer
    // value toward 0 every frame (the "return-to-center" behaviour for
    // digital input devices).  This creates a tug-of-war with the AI:
    //   Frame N:   AI sets steer = 0.5
    //   Frame N+1: PlayerController::steer() → steer -= decay → 0.45
    //              AI tries to recover → can't track the curve
    // NPC AI karts don't suffer from this because the SkiddingAI IS
    // their primary controller — there is no PlayerController fighting
    // it.
    //
    // We still call PlayerController::update() during the start-phase
    // countdown (when m_auto_drive_active is false) so that penalty
    // detection works normally.
    if (!m_auto_drive_active)
    {
        PlayerController::update(ticks);
    }

    // ---- Auto-drive: AI takes over when player is not steering ----
    if (m_auto_drive_wanted)
    {
        // Check if player is actively steering or accelerating
        bool player_active = (m_steer_val != 0) ||
                             (m_prev_accel != 0) ||
                             (m_prev_brake != 0);

        // Don't engage during countdown or kart animations
        if (!player_active && !m_kart->getKartAnimation() &&
            !World::getWorld()->isStartPhase())
        {
            // Create (or recreate after disengage) the AI lazily,
            // only when we are actually about to engage.  This avoids
            // recreating the AI every frame during the 3-2-1 start phase
            // when isStartPhase() is true and m_auto_drive_active never
            // transitions.
            if (!m_auto_drive_active)
            {
                if (m_auto_drive_ai)
                {
                    Log::info("LocalPlayerController",
                        "Auto-drive AI resumed | worldKart=%d",
                        m_kart->getWorldKartId());
                }
                else
                {
                    m_auto_drive_ai = createAutoDriveAI(m_kart);
                    m_auto_drive_controls = new KartControl;
                    m_auto_drive_ai->setControls(m_auto_drive_controls);
                    {
                        const DriveGraph* dg = DriveGraph::get();
                        const int num_nodes = dg ? dg->getNumNodes() : -1;
                        const std::string track_name = Track::getCurrentTrack()
                            ? Track::getCurrentTrack()->getIdent().c_str() : "??";
                        Log::info("LocalPlayerController",
                            "Auto-drive AI created | track=%s driveGraphNodes=%d "
                            "worldKart=%d speed=%.1f pos=(%.1f,%.1f,%.1f)",
                            track_name.c_str(), num_nodes,
                            m_kart->getWorldKartId(), m_kart->getSpeed(),
                            m_kart->getXYZ().getX(), m_kart->getXYZ().getY(),
                            m_kart->getXYZ().getZ());
                    }
                }

                m_auto_drive_active = true;

                const DriveGraph* dg = DriveGraph::get();
                const int nodes = dg ? dg->getNumNodes() : -1;
                Log::info("LocalPlayerController",
                    "Auto-drive ENGAGED | worldKart=%d track=%s nodes=%d "
                    "pos=(%.1f,%.1f,%.1f)",
                    m_kart->getWorldKartId(),
                    Track::getCurrentTrack()
                        ? Track::getCurrentTrack()->getIdent().c_str() : "??",
                    nodes,
                    m_kart->getXYZ().getX(), m_kart->getXYZ().getY(),
                    m_kart->getXYZ().getZ());
                World::getWorld()->getRaceGUI()->addMessage(
                    L"自动驾驶已打开", m_kart, 2.0f,
                    video::SColor(255, 80, 220, 80), false, true, true);
            }
            m_auto_drive_ai->update(ticks);

            // Apply the isolated AI command buffer exactly. This mirrors the
            // ownership model used by NetworkAIController.
            const KartControl* ai_ctrl = m_auto_drive_controls;
            if (ai_ctrl)
            {
                m_controls->setSteer(ai_ctrl->getSteer());
                m_controls->setAccel(ai_ctrl->getAccel());
                m_controls->setBrake(ai_ctrl->getBrake());
                m_controls->setNitro(ai_ctrl->getNitro());
                m_controls->setSkidControl(ai_ctrl->getSkidControl());
                m_controls->setRescue(ai_ctrl->getRescue());
                m_controls->setFire(ai_ctrl->getFire() || m_player_fire);
                m_controls->setLookBack(ai_ctrl->getLookBack() ||
                                        m_player_look_back);
            }

            // ---- Stuck detection: rescue only if kart truly hasn't moved ----
            float dt = stk_config->ticks2Time(ticks);
            Vec3 current_pos = m_kart->getXYZ();
            float dist_moved = (current_pos - m_auto_drive_last_pos).length();
            m_auto_drive_last_pos = current_pos;

            // Require BOTH low speed AND no movement for 3+ seconds.
            if (dist_moved < 0.15f && fabsf(m_kart->getSpeed()) < 2.0f &&
                !m_kart->getKartAnimation())
            {
                m_auto_drive_stuck_time += dt;
                if (m_auto_drive_stuck_time > 3.0f)
                {
                    Log::info("LocalPlayerController",
                              "Auto-drive stuck for %.1fs (dist=%.2f), triggering rescue",
                              m_auto_drive_stuck_time, dist_moved);
                    RescueAnimation::create(m_kart);
                    m_auto_drive_stuck_time = 0.0f;
                }
            }
            else
            {
                m_auto_drive_stuck_time = 0.0f;
            }
        }
        else if (player_active)
        {
            // Player is steering → disengage auto-drive
            if (m_auto_drive_active)
            {
                m_auto_drive_active = false;
                Log::info("LocalPlayerController", "Auto-drive DISENGAGED");
                World::getWorld()->getRaceGUI()->addMessage(
                    L"自动驾驶已关闭", m_kart, 2.0f,
                    video::SColor(255, 200, 200, 200), false, true, true);
            }
        }
    }
    else if (m_auto_drive_active)
    {
        // Auto-drive was ON but config was toggled OFF
        m_auto_drive_active = false;
        Log::info("LocalPlayerController", "Auto-drive DISENGAGED (config off)");
        World::getWorld()->getRaceGUI()->addMessage(
            L"自动驾驶已关闭", m_kart, 2.0f,
            video::SColor(255, 200, 200, 200), false, true, true);
    }

    // look backward when the player requests or
    // if automatic reverse camera is active
#ifndef SERVER_ONLY
    Camera *camera = NULL;
    if (!GUIEngine::isNoGraphics())
        camera = Camera::getCamera(m_camera_index);
    if (camera && camera->getType() != Camera::CM_TYPE_END)
    {
        if (m_controls->getLookBack() || (UserConfigParams::m_reverse_look_threshold > 0 &&
            m_kart->getSpeed() < -UserConfigParams::m_reverse_look_threshold))
            camera->setMode(Camera::CM_REVERSE);
        else
        {
            if (camera->getMode() == Camera::CM_REVERSE)
            {
                camera->setMode(Camera::CM_NORMAL);
            }
        }
        if (m_sky_particles_emitter)
        {
            // Need to set it every frame to account for heading changes
            btTransform local_trans(btQuaternion(Vec3(0, 1, 0), 0),
                Vec3(0, 30, 100));
            if (camera->getMode() == Camera::CM_REVERSE)
            {
                local_trans = btTransform (btQuaternion(Vec3(0, 1, 0),
                    180.0 * DEGREE_TO_RAD), Vec3(0, 30, -100));
            }
            setParticleEmitterPosition(local_trans);
        }
    }

    if (m_is_above_nitro_target == true &&
        m_kart->getEnergy() < RaceManager::get()->getCoinTarget())
        nitroNotFullSound();
#endif
    if (m_kart->getKartAnimation() && m_sound_schedule == false)
    {
        m_sound_schedule = true;
    }
    else if (!m_kart->getKartAnimation() && m_sound_schedule == true)
    {
        m_sound_schedule = false;
        m_kart->playSound(m_bzzt_sound);
    }
}   // update

//-----------------------------------------------------------------------------
void LocalPlayerController::setParticleEmitterPosition(const btTransform& t)
{
#ifndef SERVER_ONLY
    btTransform world_trans(btQuaternion(Vec3(0, 1, 0), m_kart->getHeading()),
        m_kart->getXYZ());
    world_trans *= t;
    btTransform inv_kart = m_kart->getTrans().inverse();
    inv_kart *= world_trans;
    m_sky_particles_emitter->setPosition(Vec3(inv_kart.getOrigin()));
    Vec3 rotation;
    rotation.setHPR(inv_kart.getRotation());
    m_sky_particles_emitter->setRotation(rotation.toIrrHPR());
#endif
}   // setParticleEmitterPosition

//-----------------------------------------------------------------------------
/** Displays a penalty warning for player controlled karts. Called from
 *  LocalPlayerKart::update() if necessary.
 */
void LocalPlayerController::displayPenaltyWarning()
{
    PlayerController::displayPenaltyWarning();
    RaceGUIBase* m=World::getWorld()->getRaceGUI();
    if (m)
    {
        m->addMessage(_("Penalty time!!"), m_kart, 2.0f,
                      GUIEngine::getSkin()->getColor("font::top"), true /* important */,
            false /*  big font */, true /* outline */);
        m->addMessage(_("Don't accelerate before 'Set!'"), m_kart, 2.0f,
            GUIEngine::getSkin()->getColor("font::normal"), true /* important */,
            false /*  big font */, true /* outline */);
    }
    m_kart->playSound(m_bzzt_sound);
}   // displayPenaltyWarning

//-----------------------------------------------------------------------------
/** Called just before the kart position is changed. It checks if the kart was
 *  overtaken, and if so plays a sound from the overtaking kart.
 */
void LocalPlayerController::setPosition(int p)
{
    PlayerController::setPosition(p);
    if (m_auto_drive_ai)
        m_auto_drive_ai->setPosition(p);


    if(m_kart->getPosition()<p)
    {
        World *world = World::getWorld();
        //have the kart that did the passing beep.
        //I'm not sure if this method of finding the passing kart is fail-safe.
        for(unsigned int i = 0 ; i < world->getNumKarts(); i++ )
        {
            AbstractKart *kart = world->getKart(i);
            if(kart->getPosition() == p + 1)
            {
                kart->beep();
                break;
            }
        }
    }
}   // setPosition

//-----------------------------------------------------------------------------
/** Called when a kart finishes race.
 *  /param time Finishing time for this kart.
 d*/
void LocalPlayerController::finishedRace(float time)
{
    if (m_auto_drive_ai)
        m_auto_drive_ai->finishedRace(time);

    // This will implicitly trigger setting the first end camera to be active
    if (!GUIEngine::isNoGraphics())
        Camera::changeCamera(m_camera_index, Camera::CM_TYPE_END);
}   // finishedRace

//-----------------------------------------------------------------------------
/** Called when a kart hits or uses a zipper.
 */
void LocalPlayerController::handleZipper(bool play_sound)
{
    PlayerController::handleZipper(play_sound);
    if (m_auto_drive_ai)
        m_auto_drive_ai->handleZipper(play_sound);

    // Only play a zipper sound if it's not already playing, and
    // if the material has changed (to avoid machine gun effect
    // on conveyor belt zippers).
    if (play_sound || (m_wee_sound->getStatus() != SFXBase::SFX_PLAYING &&
                       m_kart->getMaterial()!=m_kart->getLastMaterial()      ) )
    {
        m_wee_sound->play();
    }

#ifndef SERVER_ONLY
    // Apply the motion blur according to the speed of the kart
    if (!GUIEngine::isNoGraphics())
        irr_driver->giveBoost(m_camera_index);
#endif

}   // handleZipper

//-----------------------------------------------------------------------------
/** Called when a kart hits an item. It plays certain sfx (e.g. nitro full,
 *  or item specific sounds).
 *  \param item Item that was collected.
 *  \param old_energy The previous energy value
 */
void LocalPlayerController::collectedItem(const ItemState &item_state,
                                          float old_energy)
{
    if (m_auto_drive_ai)
        m_auto_drive_ai->collectedItem(item_state, old_energy);

    if (old_energy < m_kart->getKartProperties()->getNitroMax() &&
        m_kart->getEnergy() == m_kart->getKartProperties()->getNitroMax())
    {
        m_kart->playSound(m_full_sound);
    }
    else if (RaceManager::get()->getCoinTarget() > 0 &&
             old_energy < RaceManager::get()->getCoinTarget() &&
             m_kart->getEnergy() >= RaceManager::get()->getCoinTarget())
    {
        m_kart->playSound(m_full_sound);
        m_is_above_nitro_target = true;
    }
    else
    {
        switch(item_state.getType())
        {
        case Item::ITEM_BANANA:
        case Item::ITEM_BUBBLEGUM:
            //More sounds are played by the kart class
            //See Kart::collectedItem()
            m_kart->playSound(m_ugh_sound);
            break;
        default:
            m_kart->playSound(m_grab_sound);
            break;
        }
    }
}   // collectedItem

//-----------------------------------------------------------------------------
void LocalPlayerController::newLap(int lap)
{
    PlayerController::newLap(lap);
    if (m_auto_drive_ai)
        m_auto_drive_ai->newLap(lap);
}   // newLap

//-----------------------------------------------------------------------------
void LocalPlayerController::skidBonusTriggered()
{
    PlayerController::skidBonusTriggered();
    if (m_auto_drive_ai)
        m_auto_drive_ai->skidBonusTriggered();
}   // skidBonusTriggered

//-----------------------------------------------------------------------------
/** If the nitro level has gone under the nitro goal, play a bad effect sound
 */
void LocalPlayerController::nitroNotFullSound()
{
    m_kart->playSound(m_unfull_sound);
    m_is_above_nitro_target = false;
} //nitroNotFullSound

// ----------------------------------------------------------------------------
/** Returns true if the player of this controller can collect achievements.
 *  At the moment only the current player can collect them.
 *  TODO: check this, possible all local players should be able to
 *        collect achievements - synching to online account will happen
 *        next time the account gets online.
 */
bool LocalPlayerController::canGetAchievements() const
{
    return !RewindManager::get()->isRewinding() &&
        m_player->getConstProfile() == PlayerManager::getCurrentPlayer();
}   // canGetAchievements

// ----------------------------------------------------------------------------
core::stringw LocalPlayerController::getName(bool include_handicap_string) const
{
    if (NetworkConfig::get()->isNetworking())
        return PlayerController::getName();

    core::stringw name = m_player->getProfile()->getName();
    if (include_handicap_string && m_handicap != HANDICAP_NONE)
        name = _("%s (handicapped)", name);

    return name;
}   // getName

void LocalPlayerController::doCrashHaptics() {
#ifndef SERVER_ONLY
    if (RewindManager::get()->isRewinding())
        return;
    int now = World::getWorld()->getTicksSinceStart();
    int lastCrash = m_last_crash;
    m_last_crash = now;
    if ((now - lastCrash) < stk_config->time2Ticks(0.2f))
        return;

    float strength =
        pow(2, (abs(m_player->getKart()->getVelocity().length())) / 15.0f) - 1.0f;
    rumble(strength, strength, 200);
#endif
}

void LocalPlayerController::rumble(float strength_low, float strength_high, uint16_t duration) {
#ifndef SERVER_ONLY
    if (RewindManager::get()->isRewinding())
        return;
    int count = input_manager->getGamepadCount();
    while(count--)
    {
        SDLController* controller = input_manager->getSDLController(count);
        if (controller && controller->getGamePadDevice()->getPlayer() == m_player)
        {
            if (!controller->getGamePadDevice()->useForceFeedback())
                return;
            controller->doRumble(strength_low, strength_high, duration);
            break;
        }
    }
#endif
}

void LocalPlayerController::crashed(const AbstractKart* k) {
    doCrashHaptics();

    PlayerController::crashed(k);
    if (m_auto_drive_ai)
        m_auto_drive_ai->crashed(k);
}

void LocalPlayerController::crashed(const Material *m) {
    doCrashHaptics();

    PlayerController::crashed(m);
    if (m_auto_drive_ai)
        m_auto_drive_ai->crashed(m);
}

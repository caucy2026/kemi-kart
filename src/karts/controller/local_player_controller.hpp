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


#ifndef HEADER_LOCAL_PLAYER_CONTROLLER_HPP
#define HEADER_LOCAL_PLAYER_CONTROLLER_HPP

#include "karts/controller/player_controller.hpp"
#include "utils/vec3.hpp"
#include <memory>

class AbstractKart;
class SFXBase;
class SFXBuffer;
class btTransform;
class SkiddingAI;

#ifndef SERVER_ONLY
class ParticleEmitter;
#endif

/** PlayerKart manages control events from the player and moves
  * them to the Kart
  *
  * \ingroup controller
  */
class LocalPlayerController : public PlayerController
{
private:

    /** Stores the active player data structure. */
    StateManager::ActivePlayer *m_player;

    bool           m_sound_schedule;
    bool           m_has_started;
    bool           m_is_above_nitro_target;

#ifndef SERVER_ONLY
    std::unique_ptr<ParticleEmitter> m_sky_particles_emitter;
#endif

    /** The index of the camera attached to the kart for this controller. The
     *  camera object is managed in the Camera class, so no need to free it. */
    int  m_camera_index;

    int m_last_crash;

    HandicapLevel m_handicap;

    /** AI controller for auto-drive mode. Created lazily when first needed. */
    SkiddingAI* m_auto_drive_ai;
    /** Whether auto-drive is currently active (AI is controlling the kart). */
    bool        m_auto_drive_active;
    /** Per-player auto-drive toggle (independent per screen in dual mode). */
    bool        m_auto_drive_wanted;
    /** Accumulated time the kart has been nearly stationary under auto-drive. */
    float       m_auto_drive_stuck_time;
    /** Last position for stuck detection (checks actual movement, not speed). */
    Vec3        m_auto_drive_last_pos;
    /** Blend timer for smooth player→AI steer transition (0=player, 1=AI). */
    float       m_auto_drive_blend;

    SFXBase     *m_wee_sound;
    SFXBuffer   *m_bzzt_sound;
    SFXBuffer   *m_ugh_sound;
    SFXBuffer   *m_grab_sound;
    SFXBuffer   *m_full_sound;
    SFXBuffer   *m_unfull_sound;


    virtual void steer(int, int) OVERRIDE;
    virtual void displayPenaltyWarning() OVERRIDE;
    void         nitroNotFullSound();

    void doCrashHaptics();
    void setParticleEmitterPosition(const btTransform& t);

public:
                 LocalPlayerController(AbstractKart *kart,
                                       const int local_player_id,
                                       HandicapLevel h);
                ~LocalPlayerController();
    void         update            (int ticks) OVERRIDE;
    bool         action            (PlayerAction action, int value,
                                    bool dry_run=false) OVERRIDE;
            void initParticleEmitter();
    virtual void handleZipper      (bool play_sound) OVERRIDE;
    void         collectedItem     (const ItemState &item,
                                    float previous_energy=0) OVERRIDE;
    virtual void setPosition       (int p) OVERRIDE;
    virtual void reset             () OVERRIDE;
    virtual void finishedRace      (float time) OVERRIDE;
    virtual void resetInputState   () OVERRIDE;
    virtual bool canGetAchievements() const OVERRIDE;
    // ---- Per-player auto-drive (independent per screen in dual mode) ----
    bool isAutoDriveWanted() const          { return m_auto_drive_wanted; }
    void toggleAutoDrive()                  { m_auto_drive_wanted = !m_auto_drive_wanted; }

    virtual void crashed(const AbstractKart *k) OVERRIDE;
    virtual void crashed(const Material *m) OVERRIDE;

    virtual void rumble(float strength_low, float strength_high, uint16_t duration) OVERRIDE;

    // ------------------------------------------------------------------------
    virtual bool isPlayerController() const OVERRIDE {return true;}
    // ------------------------------------------------------------------------
    virtual bool isLocalPlayerController() const OVERRIDE {return true;}
    // ------------------------------------------------------------------------
    /** Returns the name of the player profile. */
    core::stringw getName(bool include_handicap_string = true) const OVERRIDE;
};   // LocalPlayerController

#endif

// LuxConsoleCommands.cpp
// Lux-only console command registrations (spawn, set/get allow-listed vars)

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

#include "impl/ImGuiConsole.h"
#include "impl/ImGuiDebugMenu.h"

#include "LuxBase.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxInventory.h"
#include "LuxProp_Item.h"
#include "LuxProp.h"
#include "LuxProp_SwingDoor.h"
#include "LuxProp_LevelDoor.h"
#include "LuxProp_Lamp.h"
#include "LuxEnemy.h"
#include "LuxArea.h"
#include "LuxMapHelper.h"
#include "system/String.h"
#include "math/Math.h"
#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "engine/Engine.h"
#include "sound/Sound.h"
#include "sound/SoundHandler.h"

static int gConsoleSpawnCounter = 0;

static std::string GetFileStem(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    const size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot < start) dot = path.size();
    return path.substr(start, dot - start);
}

static std::string ToLowerLocal(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static bool ParseBoolLocal(const std::string& s, bool& out)
{
    const std::string v = ToLowerLocal(s);
    if (v == "1" || v == "true" || v == "on"  || v == "yes" || v == "y") { out = true;  return true; }
    if (v == "0" || v == "false"|| v == "off" || v == "no"  || v == "n") { out = false; return true; }
    return false;
}

// -----------------------------------------------------------------
// Every .ent the game can load, by bare name.
//
// cFileSearcher has already indexed every mounted resource directory, so this
// is a map walk rather than a disk scan -- and it covers custom stories and
// mods for free, because those are mounted through the same searcher.
//
// Built once. The searcher's contents only change when directories are
// mounted, which happens at startup.
struct sConsoleEntityNames
{
    std::vector<std::string> mvItemNames;   // paths containing "item"
    std::vector<std::string> mvAllNames;
    bool mbBuilt;

    sConsoleEntityNames() : mbBuilt(false) {}
};

static sConsoleEntityNames gConsoleEntityNames;

static void BuildEntityNameCache()
{
    if (gConsoleEntityNames.mbBuilt) return;
    if (gpBase == nullptr || gpBase->mpEngine == nullptr) return;

    hpl::cResources* pResources = gpBase->mpEngine->GetResources();
    if (pResources == nullptr) return;

    hpl::cFileSearcher* pSearcher = pResources->GetFileSearcher();
    if (pSearcher == nullptr) return;

    gConsoleEntityNames.mbBuilt = true;

    const hpl::tFilePathMap& mapFiles = pSearcher->GetFileMap();
    for (hpl::tFilePathMap::const_iterator it = mapFiles.begin(); it != mapFiles.end(); ++it)
    {
        const std::string sName = ToLowerLocal(it->first);
        if (sName.size() < 5) continue;
        if (sName.compare(sName.size() - 4, 4, ".ent") != 0) continue;

        const std::string sStem = sName.substr(0, sName.size() - 4);
        gConsoleEntityNames.mvAllNames.push_back(sStem);

        // Amnesia keeps inventory entities under entities/item/..., and a Tab
        // that lists all ~800 entity files is not a menu. Anything outside that
        // is still reachable -- see the fallback in the provider below, and give
        // itself does not consult this list at all.
        const std::string sPath = ToLowerLocal(hpl::cString::To8Char(it->second.msPath));
        if (sPath.find("item") != std::string::npos)
            gConsoleEntityNames.mvItemNames.push_back(sStem);
    }

    std::sort(gConsoleEntityNames.mvAllNames.begin(), gConsoleEntityNames.mvAllNames.end());
    gConsoleEntityNames.mvAllNames.erase(
        std::unique(gConsoleEntityNames.mvAllNames.begin(), gConsoleEntityNames.mvAllNames.end()),
        gConsoleEntityNames.mvAllNames.end());

    std::sort(gConsoleEntityNames.mvItemNames.begin(), gConsoleEntityNames.mvItemNames.end());
    gConsoleEntityNames.mvItemNames.erase(
        std::unique(gConsoleEntityNames.mvItemNames.begin(), gConsoleEntityNames.mvItemNames.end()),
        gConsoleEntityNames.mvItemNames.end());
}

static std::vector<std::string> MatchPrefix(const std::vector<std::string>& avNames,
                                            const std::string& asPrefix)
{
    std::vector<std::string> vOut;
    for (size_t i = 0; i < avNames.size(); ++i)
    {
        if (avNames[i].compare(0, asPrefix.size(), asPrefix) == 0)
            vOut.push_back(avNames[i]);
    }
    return vOut;
}

// -----------------------------------------------------------------
// Player 1, wherever they currently sit.
//
// Console commands run from the frame loop and never inside a script callback,
// so gpBase->mpPlayer is P1 in practice -- but a CoopBeginActAs swap would put P2
// there. Matching on the index each player CARRIES rather than on which global it
// sits in costs nothing and cannot be wrong.
static cLuxPlayer* ConsolePlayer1()
{
    if (gpBase == nullptr) return nullptr;

    cLuxPlayer* pCandidates[3] = { gpBase->mpPlayer,
                                   cLuxPlayer::CoopGetSwappedOutPlayer(),
                                   gpBase->mpPlayer2 };

    for (int i = 0; i < 3; ++i)
        if (pCandidates[i] && pCandidates[i]->GetPlayerIndex() == 0) return pCandidates[i];

    return nullptr;
}

// How far these commands reach. Far enough to pick a door across a room, short
// enough not to grab one through a doorway you were not looking through.
static const float gfConsoleAimRange = 10.0f;

// The door Player 1 is aiming at, or nullptr with a reason already logged.
//
// A real raycast rather than the focus/crosshair system, which only registers
// entities inside interaction range -- the point of these commands is to reach a
// door you are looking at, not one you could already touch.
static iLuxProp* ConsoleGetAimedDoor(const char* asCmd, eLuxPropType* apTypeOut)
{
    if (gpBase == nullptr || gpBase->mpMapHandler == nullptr || gpBase->mpMapHelper == nullptr)
    {
        cImGuiConsole::AddLog("%s: game not ready", asCmd);
        return nullptr;
    }

    if (gpBase->mpMapHandler->GetCurrentMap() == nullptr)
    {
        cImGuiConsole::AddLog("%s: no current map", asCmd);
        return nullptr;
    }

    cLuxPlayer* pPlayer = ConsolePlayer1();
    if (pPlayer == nullptr || pPlayer->GetCamera() == nullptr)
    {
        cImGuiConsole::AddLog("%s: player 1 has no camera", asCmd);
        return nullptr;
    }

    const cVector3f vStart = pPlayer->GetCamera()->GetPosition();
    const cVector3f vDir   = pPlayer->GetCamera()->GetForward();

    float fDist = 0.0f;
    iPhysicsBody* pBody = nullptr;
    iLuxEntity* pEntity = nullptr;

    //Every out-parameter gets a real address on purpose: GetClosestEntity finishes
    //with `return *apEntity != NULL;` and dereferences that one unconditionally, so
    //passing nullptr for it crashes however little you want the result.
    gpBase->mpMapHelper->GetClosestEntity(vStart, vDir, gfConsoleAimRange, &fDist, &pBody, &pEntity);

    if (pEntity == nullptr)
    {
        cImGuiConsole::AddLog("%s: not aiming at anything within %.0fm", asCmd, gfConsoleAimRange);
        return nullptr;
    }

    if (pEntity->GetEntityType() != eLuxEntityType_Prop)
    {
        cImGuiConsole::AddLog("%s: '%s' is not a door", asCmd, pEntity->GetName().c_str());
        return nullptr;
    }

    iLuxProp* pProp = static_cast<iLuxProp*>(pEntity);
    const eLuxPropType propType = pProp->GetPropType();

    if (propType != eLuxPropType_SwingDoor && propType != eLuxPropType_LevelDoor)
    {
        cImGuiConsole::AddLog("%s: '%s' is a prop, but not a swing door or level door",
                              asCmd, pEntity->GetName().c_str());
        return nullptr;
    }

    if (apTypeOut) *apTypeOut = propType;

    return pProp;
}

// The sound an unlock falls back to when the door itself defines none.
//
// A raw .ogg, played through the sound handler rather than iLuxEntity::PlaySound.
// That path goes via cWorld::CreateSoundEntity, which runs the name through
// cString::SetFileExt(name,"snt") -- it can ONLY play a sound entity, so handing
// it an .ogg would silently look for a .snt of the same name and find nothing.
//
// Still a variable, so a story that keeps its audio somewhere else can repoint
// it with `consoleUnlockSound <path>`.
static tString gsConsoleUnlockSound = "sounds/door/unlock_door.ogg";

// Play the unlock fallback at the door, positional, so it comes from the door
// rather than out of the player's head.
static void ConsolePlayUnlockSound(iLuxProp* apDoor)
{
    if (gsConsoleUnlockSound == "") return;
    if (gpBase == nullptr || gpBase->mpEngine == nullptr) return;

    cSoundHandler* pHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
    if (pHandler == nullptr) return;

    cVector3f vPos(0.0f);
    iEntity3D* pAttach = apDoor ? apDoor->GetAttachEntity() : nullptr;
    if (pAttach) vPos = pAttach->GetWorldPosition();

    //Same distance envelope the game's own door sounds use, so it does not
    //carry further than the rattle it replaces.
    pHandler->Play3D(gsConsoleUnlockSound, false, 1.0f, vPos, 1.0f, 10.0f);
}

// ------------------------------------------------------------------
// Undo history for `delete`.
//
// A deleted entity is not actually destroyed -- it is switched off, which for a
// prop hides the mesh and stops its bodies and lights, so it is gone as far as
// the map is concerned. It is held here instead, newest last.
//
// The history holds `undoArray` entries, 1 by default. Deleting past that
// destroys the OLDEST held entity for real, which is what stops a session's
// worth of deleted furniture sitting in memory, and why undo can never reach
// past the limit: those ones no longer exist to reach.
//
// Shrinking the limit does not free anything on the spot. The trim happens on
// the next delete, so setting it to 1 does not silently throw away nine things
// you might still have wanted while you were mid-thought.
//
// The map pointer is kept per entry because a map change destroys every entity
// in the old map. Comparing it against the live map is how a stale pointer is
// spotted without needing to hear about the map change.
struct sConsoleDeletedEntity
{
    iLuxEntity* mpEntity;
    cLuxMap*    mpMap;
    tString     msName;
    bool        mbWasActive;
};

static std::vector<sConsoleDeletedEntity> gvConsoleDeleted;
static int gConsoleUndoArraySize = 1;

// True while a held entry is still real and still in the map it came from.
static bool ConsoleDeletedIsLive(const sConsoleDeletedEntity& aEntry)
{
    if (aEntry.mpEntity == nullptr) return false;
    if (gpBase == nullptr || gpBase->mpMapHandler == nullptr) return false;

    return aEntry.mpMap != nullptr &&
           gpBase->mpMapHandler->GetCurrentMap() == aEntry.mpMap;
}

// Destroy the oldest held entities for real until at most alKeep remain.
static void ConsoleTrimUndoHistory(int alKeep)
{
    if (alKeep < 0) alKeep = 0;

    while ((int)gvConsoleDeleted.size() > alKeep)
    {
        sConsoleDeletedEntity& entry = gvConsoleDeleted.front();

        if (ConsoleDeletedIsLive(entry))
            entry.mpMap->DestroyEntity(entry.mpEntity);

        gvConsoleDeleted.erase(gvConsoleDeleted.begin());
    }
}

// ------------------------------------------------------------------
// Aiming, for the commands that act on "whatever I am looking at".
//
// NOT cLuxMapHelper::GetClosestEntity. That reads the entity pointer off the
// PHYSICS body, and a monster does not keep it there -- LuxEnemy.cpp puts it on
// the cCharacterBody instead. So the physics body comes back with no entity,
// falls through to the plain-body branch, and is thrown out by its
// `if(apBody->IsCharacter()) return false`. Monsters were never targetable at
// all; catching a water lurker was hard because the water is irrelevant and the
// ray was only ever hitting it by luck through some other body.
//
// Areas were unreachable for a second reason: they are built with
// SetCollide(false) and iLuxArea::CanInteract returns false, so that helper
// rejects them too -- which made `delete`'s "areas need debugView on" rule
// unreachable, because an area could not be aimed at under any conditions.
//
// The same body -> entity resolution the debug gun already documents, plus the
// area rule.
class cLuxConsoleAimCallback : public iPhysicsRayCallback
{
public:
    void Reset()
    {
        mfClosestDist = -1;
        mpClosestBody = NULL;
        mpClosestEntity = NULL;
    }

    static iLuxEntity* ResolveEntity(iPhysicsBody *apBody)
    {
        //A character body carries it; everything else keeps it on the body.
        //Players have none on either, so this stays NULL for them rather than
        //handing back something that is not an iLuxEntity.
        if(apBody->IsCharacter())
        {
            iCharacterBody *pCharBody = apBody->GetCharacterBody();
            return pCharBody ? (iLuxEntity*)pCharBody->GetUserData() : NULL;
        }

        return (iLuxEntity*)apBody->GetUserData();
    }

    bool BeforeIntersect(iPhysicsBody *apBody)
    {
        //Never your own body, and never the other player's -- the ray starts
        //inside one of them.
        if(apBody->IsCharacter())
        {
            if(gpBase->mpPlayer && gpBase->mpPlayer->GetCharacterBody() &&
                apBody == gpBase->mpPlayer->GetCharacterBody()->GetCurrentBody()) return false;

            if(gpBase->mpPlayer2 && gpBase->mpPlayer2->GetCharacterBody() &&
                apBody == gpBase->mpPlayer2->GetCharacterBody()->GetCurrentBody()) return false;
        }

        iLuxEntity *pEntity = ResolveEntity(apBody);

        if(pEntity == NULL)
        {
            //Plain world geometry. Solid stops the ray, the rest is invisible
            //to it -- otherwise you could target through walls.
            return apBody->GetCollide();
        }

        //An area is only a target while you can see it, which is what debug
        //view is. Inactive ones included: they are drawn dashed and are exactly
        //what you might be trying to remove.
        if(pEntity->GetEntityType() == eLuxEntityType_Area)
            return ::ImGuiDebugMenu::GetDebugView();

        return pEntity->IsActive();
    }

    bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams)
    {
        if(mfClosestDist < 0 || apParams->mfDist < mfClosestDist)
        {
            mfClosestDist = apParams->mfDist;
            mpClosestBody = apBody;
            mpClosestEntity = ResolveEntity(apBody);
        }

        return true;
    }

    float mfClosestDist;
    iPhysicsBody *mpClosestBody;
    iLuxEntity *mpClosestEntity;
};

static cLuxConsoleAimCallback gConsoleAimCallback;

// What Player 1 is aiming at, whatever it is. The door version above is this
// plus a type check; the commands that act on anything need the raw answer.
static iLuxEntity* ConsoleGetAimedEntity(const char* asCmd)
{
    if (gpBase == nullptr || gpBase->mpMapHandler == nullptr || gpBase->mpMapHelper == nullptr)
    {
        cImGuiConsole::AddLog("%s: game not ready", asCmd);
        return nullptr;
    }

    if (gpBase->mpMapHandler->GetCurrentMap() == nullptr)
    {
        cImGuiConsole::AddLog("%s: no current map", asCmd);
        return nullptr;
    }

    cLuxPlayer* pPlayer = ConsolePlayer1();
    if (pPlayer == nullptr || pPlayer->GetCamera() == nullptr)
    {
        cImGuiConsole::AddLog("%s: player 1 has no camera", asCmd);
        return nullptr;
    }

    const cVector3f vStart = pPlayer->GetCamera()->GetPosition();
    const cVector3f vDir   = pPlayer->GetCamera()->GetForward();

    iPhysicsWorld* pPhysicsWorld = gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld();

    gConsoleAimCallback.Reset();
    pPhysicsWorld->CastRay(&gConsoleAimCallback, vStart, vStart + vDir * gfConsoleAimRange,
                           true, false, false, true);

    iLuxEntity* pEntity = gConsoleAimCallback.mpClosestEntity;

    if (pEntity == nullptr)
        cImGuiConsole::AddLog("%s: not aiming at anything within %.0fm", asCmd, gfConsoleAimRange);

    return pEntity;
}

// Shared body of `lit` and `unlit`.
static void ConsoleSetAimedLampLit(bool abLit)
{
    const char* sCmd = abLit ? "lit" : "unlit";

    iLuxEntity* pEntity = ConsoleGetAimedEntity(sCmd);
    if (pEntity == nullptr) return;

    if (pEntity->GetEntityType() != eLuxEntityType_Prop)
    {
        cImGuiConsole::AddLog("%s: '%s' is not a lamp", sCmd, pEntity->GetName().c_str());
        return;
    }

    iLuxProp* pProp = static_cast<iLuxProp*>(pEntity);
    if (pProp->GetPropType() != eLuxPropType_Lamp)
    {
        cImGuiConsole::AddLog("%s: '%s' is a prop, but not a lamp", sCmd, pEntity->GetName().c_str());
        return;
    }

    cLuxProp_Lamp* pLamp = static_cast<cLuxProp_Lamp*>(pProp);

    if (pLamp->GetLit() == abLit)
    {
        cImGuiConsole::AddLog("%s: '%s' is already %s", sCmd, pEntity->GetName().c_str(),
                              abLit ? "lit" : "out");
        return;
    }

    //true = run the effects: the flame particles, the light fade and the
    //lamp's own sound. Without it the state changes and nothing looks like it.
    pLamp->SetLit(abLit, true);

    cImGuiConsole::AddLog("%s: '%s' is now %s", sCmd, pEntity->GetName().c_str(),
                          abLit ? "lit" : "out");
}

// Shared body of `lock` and `unlock`. The two door classes disagree about sound
// and have to be handled apart -- see the notes on each branch.
static void ConsoleSetAimedDoorLocked(bool abLocked)
{
    const char* sCmd = abLocked ? "lock" : "unlock";
    const char* sNow = abLocked ? "locked" : "unlocked";

    eLuxPropType propType = eLuxPropType_LastEnum;
    iLuxProp* pProp = ConsoleGetAimedDoor(sCmd, &propType);
    if (pProp == nullptr) return;

    if (propType == eLuxPropType_SwingDoor)
    {
        cLuxProp_SwingDoor* pDoor = static_cast<cLuxProp_SwingDoor*>(pProp);

        //SetLocked early-outs when the state already matches -- silently, sound
        //included -- so say so rather than looking like a command that did nothing.
        if (pDoor->GetLocked() == abLocked)
        {
            cImGuiConsole::AddLog("%s: '%s' is already %s", sCmd, pProp->GetName().c_str(), sNow);
            return;
        }

        //true = play the door's own LockOn/LockOff sound, if its .ent defines one.
        pDoor->SetLocked(abLocked, true);

        //Plenty of doors define neither, and a lock command you cannot hear is not
        //much of one.
        //
        //The fallbacks differ on purpose. Locking can borrow the rattle a locked
        //door makes when you try it -- that IS the sound of a door refusing to
        //open. Unlocking cannot: the rattle means "still locked", so playing it
        //to confirm an unlock says the opposite of what happened. That one gets
        //the game's own unlock sound instead.
        const tString& sOwnSound = abLocked ? pDoor->GetLockOnSound() : pDoor->GetLockOffSound();
        if (sOwnSound == "")
        {
            if (abLocked)
                pDoor->PlaySound("ConsoleLock", pDoor->GetInteractLockedSound(), true, false);
            else
                ConsolePlayUnlockSound(pDoor);
        }
    }
    else
    {
        cLuxProp_LevelDoor* pDoor = static_cast<cLuxProp_LevelDoor*>(pProp);

        if (pDoor->GetLocked() == abLocked)
        {
            cImGuiConsole::AddLog("%s: '%s' is already %s", sCmd, pProp->GetName().c_str(), sNow);
            return;
        }

        //cLuxProp_LevelDoor::SetLocked is a bare flag setter -- no effects argument,
        //no sound of any kind -- so the sound has to be played here. The only one a
        //level door owns is the one for trying it locked.
        pDoor->SetLocked(abLocked);

        //Does nothing on an empty filename, so a door with none configured is silent
        //rather than broken. Same split as the swing door above: the locked
        //rattle confirms a lock but contradicts an unlock.
        if (abLocked)
            pDoor->PlaySound("ConsoleLock", pDoor->GetLockedSound(), true, false);
        else
            ConsolePlayUnlockSound(pDoor);
    }

    cImGuiConsole::AddLog("%s: '%s' is now %s", sCmd, pProp->GetName().c_str(), sNow);
}

void Lux_RegisterConsoleCommands()
{
    // ------------------------------------------------------------
    // spawn <path.ent>
    cImGuiConsole::RegisterCommand(
        "spawn",
        "spawn <entityFile.ent>  (spawns in front of player)",
        [](const cImGuiConsole::tArgs& a)
        {
            if (a.empty())
            {
                cImGuiConsole::AddLog("Usage: spawn entities/enemy/waterlurker/waterlurker.ent");
                return;
            }

            const std::string entFile = a[0];

            // Require ".ent" (case-insensitive)
            if (entFile.size() < 4 || ToLowerLocal(entFile.substr(entFile.size() - 4)) != ".ent")
            {
                cImGuiConsole::AddLog("spawn: expected a .ent file path (got '%s')", entFile.c_str());
                return;
            }

            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr || gpBase->mpPlayer == nullptr)
            {
                cImGuiConsole::AddLog("spawn: gpBase/mapHandler/player not ready");
                return;
            }

            cLuxMap* pMap = gpBase->mpMapHandler->GetCurrentMap();
            if (pMap == nullptr)
            {
                cImGuiConsole::AddLog("spawn: no current map");
                return;
            }

            // Determine spawn transform (in front of player view)
            cVector3f camPos(0, 0, 0);
            cVector3f forward(0, 0, 1);

            if (gpBase->mpPlayer->GetCamera())
            {
                camPos  = gpBase->mpPlayer->GetCamera()->GetPosition();
                forward = gpBase->mpPlayer->GetCamera()->GetForward();
            }
            else
            {
                camPos = gpBase->mpPlayer->GetCharacterBody()->GetPosition();
                const float yaw = gpBase->mpPlayer->GetCharacterBody()->GetYaw();
                forward = cVector3f(std::sin(yaw), 0.0f, std::cos(yaw));
            }

            if (forward.Length() > 0.0001f) forward.Normalize();

            const float spawnDist = 1.5f;
            cVector3f spawnPos = camPos + forward * spawnDist;
            spawnPos.y += 0.1f; // small nudge to reduce floor interpenetration

            const std::string stem = GetFileStem(entFile);
            const std::string entName =
                "console_" + stem + "_" + cString::ToString(gConsoleSpawnCounter++);

            const cMatrixf mtx = cMath::MatrixTranslate(spawnPos);
            const cVector3f scale(1, 1, 1);

            pMap->CreateEntity(entName, entFile, mtx, scale);

            cImGuiConsole::AddLog("Spawned '%s' from '%s'", entName.c_str(), entFile.c_str());
        }
    );

    // ------------------------------------------------------------
    // coop <0|1>
    cImGuiConsole::RegisterCommand(
        "coop",
        "coop <0|1>  (enable/disable split-screen co-op)",
        [](const cImGuiConsole::tArgs& a)
        {
            if (a.empty())
            {
                cImGuiConsole::AddLog("Usage: coop 1  or  coop 0");
                return;
            }

            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr)
            {
                cImGuiConsole::AddLog("coop: not ready");
                return;
            }

            bool bEnable = false;
            if (!ParseBoolLocal(a[0], bEnable))
            {
                cImGuiConsole::AddLog("coop: expected 0/1/true/false");
                return;
            }

            gpBase->mpMapHandler->SetCoopMode(bEnable);
            if (bEnable && !gpBase->mpMapHandler->MapIsLoaded())
                cImGuiConsole::AddLog("Co-op split-screen: ON (will activate on next map load)");
            else
                cImGuiConsole::AddLog("Co-op split-screen: %s", bEnable ? "ON" : "OFF");
        }
    );

    // ------------------------------------------------------------
    // noclip
    // Toggle noclip permission. First "noclip" enables V-key toggling.
    // If already in noclip flight, "noclip" deactivates it too.
    cImGuiConsole::RegisterCommand(
        "noclip",
        "noclip  (toggle noclip on/off; press V in-game to fly)",
        [](const cImGuiConsole::tArgs& /*a*/)
        {
            if (gpBase == nullptr || gpBase->mpPlayer == nullptr)
            {
                cImGuiConsole::AddLog("noclip: player not ready");
                return;
            }

            const bool wasEnabled = gpBase->mpPlayer->GetNoclipEnabled();

            if (wasEnabled)
            {
                // Turning off: this also deactivates flight if active
                gpBase->mpPlayer->SetNoclipEnabled(false);
                cImGuiConsole::AddLog("Noclip DISABLED (V-key toggle off)");
            }
            else
            {
                gpBase->mpPlayer->SetNoclipEnabled(true);
                cImGuiConsole::AddLog("Noclip ENABLED - press V to toggle flight");
                cImGuiConsole::AddLog("  W/S/A/D = move, Space = up, Shift = fast, Ctrl = slow");
            }
        }
    );

    // ------------------------------------------------------------
    // set <var> <value>
    // Allow-listed runtime tuning
    //
    // sanity_darkness_drain (bool):
    //   true  => drain enabled (normal game)
    //   false => drain disabled
    // Backs onto cLuxPlayer::mbSanityDrainDisabled (inverse).
    cImGuiConsole::RegisterCommand(
        "set",
        "set <var> <value>  |  Vars: fov, splitScreenMode, sanityDrainDisabled",
        [](const cImGuiConsole::tArgs& a)
        {
            if (a.size() < 2)
            {
                cImGuiConsole::AddLog("Usage: set <var> <value>");
                cImGuiConsole::AddLog("Example: set mbSanityDrainDisabled false");
                cImGuiConsole::AddLog("Example: set fov 80");
                cImGuiConsole::AddLog("Allowed vars: mbSanityDrainDisabled, fov");
                return;
            }

            if (gpBase == nullptr || gpBase->mpPlayer == nullptr)
            {
                cImGuiConsole::AddLog("set: player not ready");
                return;
            }

            const std::string var = ToLowerLocal(a[0]);
            const std::string val = a[1];

            if (var == "fov")
            {
                float fDeg = 0;
                try { fDeg = std::stof(val); }
                catch (...) {
                    cImGuiConsole::AddLog("set fov: expected a number (got '%s')", val.c_str());
                    return;
                }

                if (fDeg < 10.0f || fDeg > 170.0f)
                {
                    cImGuiConsole::AddLog("set fov: value out of range (10-170)");
                    return;
                }

                float fRad = cMath::ToRad(fDeg);
                gpBase->mpPlayer->SetBaseFOV(fRad);

                if (gpBase->mpPlayer2)
                    gpBase->mpPlayer2->SetBaseFOV(fRad);

                cImGuiConsole::AddLog("FOV = %.1f degrees", fDeg);
                return;
            }

            if (var == "splitscreenmode" || var == "splitmode" || var == "ssmode")
            {
                int iMode = 0;
                try { iMode = std::stoi(val); }
                catch (...) {
                    cImGuiConsole::AddLog("set splitScreenMode: expected 1-3 (got '%s')", val.c_str());
                    return;
                }

                if (iMode < 1 || iMode > 3)
                {
                    cImGuiConsole::AddLog("set splitScreenMode: value must be 1-3");
                    cImGuiConsole::AddLog("  1 = Left/Right, 2 = Top/Bottom, 3 = Dual Monitor");
                    return;
                }

                if (iMode == 3)
                {
                    cImGuiConsole::AddLog("splitScreenMode is read only for value \"%d\". Use the debug menu (Insert) to configure dual monitor.", iMode);
                    return;
                }

                gpBase->mpMapHandler->SetSplitScreenMode(iMode);

                const char* modeNames[] = { "", "Left/Right", "Top/Bottom", "Dual Monitor" };
                cImGuiConsole::AddLog("Split-screen mode = %d (%s)", iMode, modeNames[iMode]);
                return;
            }

            if (var == "mbsanitydraindisabled" || var == "sanitydrain_darkness" || var == "darkness_sanity_drain")
            {
                bool enableDrain = true;
                if (!ParseBoolLocal(val, enableDrain))
                {
                    cImGuiConsole::AddLog("set %s: expected bool (true/false/1/0/on/off)", var.c_str());
                    return;
                }

                // Inverse storage in LuxPlayer
                gpBase->mpPlayer->SetSanityDrainDisabled(!enableDrain);

                cImGuiConsole::AddLog("mbSanityDrainDisabled = %s", enableDrain ? "true" : "false");
                return;
            }

            cImGuiConsole::AddLog("set: unknown or disallowed var '%s'", a[0].c_str());
        }
    );

    // ------------------------------------------------------------
    // get <var>
    cImGuiConsole::RegisterCommand(
        "get",
        "get <var>  |  Vars: fov, splitScreenMode, sanityDrainDisabled",
        [](const cImGuiConsole::tArgs& a)
        {
            if (a.empty())
            {
                cImGuiConsole::AddLog("Usage: get <var>");
                cImGuiConsole::AddLog("Example: get fov");
                return;
            }

            if (gpBase == nullptr || gpBase->mpPlayer == nullptr)
            {
                cImGuiConsole::AddLog("get: player not ready");
                return;
            }

            const std::string var = ToLowerLocal(a[0]);

            if (var == "fov")
            {
                float fDeg = cMath::ToDeg(gpBase->mpPlayer->GetBaseFOV());
                cImGuiConsole::AddLog("FOV = %.1f degrees", fDeg);
                return;
            }

            if (var == "splitscreenmode" || var == "splitmode" || var == "ssmode")
            {
                int iMode = gpBase->mpMapHandler->GetSplitScreenMode();
                const char* modeNames[] = { "", "Left/Right", "Top/Bottom", "Dual Monitor" };
                cImGuiConsole::AddLog("Split-screen mode = %d (%s)", iMode, modeNames[iMode]);
                return;
            }

            if (var == "mbsanitydraindisabled" || var == "sanitydrain_darkness" || var == "darkness_sanity_drain")
            {
                const bool enableDrain = !gpBase->mpPlayer->GetSanityDrainDisabled();
                cImGuiConsole::AddLog("mbSanityDrainDisabled = %s", enableDrain ? "true" : "false");
                return;
            }

            cImGuiConsole::AddLog("get: unknown or disallowed variable '%s'", a[0].c_str());
        }
    );

    // ------------------------------------------------------------
    // give <item>
    cImGuiConsole::RegisterCommand(
        "give",
        "give <item>  (adds an item to the inventory, as if picked up)",
        [](const cImGuiConsole::tArgs& a)
        {
            if (a.empty())
            {
                cImGuiConsole::AddLog("Usage: give tinderbox   (press Tab to list)");
                return;
            }

            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr || gpBase->mpInventory == nullptr)
            {
                cImGuiConsole::AddLog("give: game not ready");
                return;
            }

            cLuxMap* pMap = gpBase->mpMapHandler->GetCurrentMap();
            if (pMap == nullptr)
            {
                cImGuiConsole::AddLog("give: no current map");
                return;
            }

            std::string sFile = a[0];
            if (sFile.size() < 4 || ToLowerLocal(sFile.substr(sFile.size() - 4)) != ".ent")
                sFile += ".ent";

            // Bare file name on purpose: cFileSearcher resolves by name across
            // every mounted directory, so `give tinderbox` finds it wherever the
            // install (or a custom story) actually keeps it.
            static int lGiveCounter = 0;
            const std::string sEntName = "console_give_" + std::to_string(++lGiveCounter);

            // Same route cLuxScriptHandler::GiveItemFromFile takes: spawn the
            // entity, lift its item data off cLuxProp_Item, hand that to the
            // inventory, throw the entity away. The item therefore arrives with
            // the icon, subtype and amount a real pickup would have.
            pMap->ResetLatestEntity();
            pMap->CreateEntity(sEntName, sFile, cMatrixf::Identity, 1);

            iLuxEntity* pEntity = pMap->GetLatestEntity();
            if (pEntity == nullptr)
            {
                cImGuiConsole::AddLog("give: no entity file '%s'", sFile.c_str());
                return;
            }

            bool bGiven = false;
            if (pEntity->GetEntityType() == eLuxEntityType_Prop)
            {
                iLuxProp* pProp = static_cast<iLuxProp*>(pEntity);
                if (pProp->GetPropType() == eLuxPropType_Item)
                {
                    cLuxProp_Item* pItem = static_cast<cLuxProp_Item*>(pProp);
                    gpBase->mpInventory->AddItem(sEntName, pItem->GetItemType(),
                                                 pItem->GetSubItemTypeName(),
                                                 pItem->GetImageFile(), pItem->GetAmount(), "", "");
                    bGiven = true;
                }
            }

            pMap->DestroyEntity(pEntity);

            if (bGiven)
                cImGuiConsole::AddLog("give: added '%s'", a[0].c_str());
            else
                cImGuiConsole::AddLog("give: '%s' exists but is not an inventory item", a[0].c_str());
        }
    );

    cImGuiConsole::RegisterCompletion(
        "give",
        [](const std::string& asPrefix)
        {
            BuildEntityNameCache();

            const std::string sPrefix = ToLowerLocal(asPrefix);

            // Items first. Only widen to every entity in the game if the prefix
            // matches no item at all, so a typo does not silently offer scenery
            // while a real item was one letter away.
            std::vector<std::string> vOut = MatchPrefix(gConsoleEntityNames.mvItemNames, sPrefix);
            if (vOut.empty())
                vOut = MatchPrefix(gConsoleEntityNames.mvAllNames, sPrefix);

            return vOut;
        }
    );

    // ------------------------------------------------------------
    // lock / unlock  (the door Player 1 is aiming at)
    cImGuiConsole::RegisterCommand(
        "lock",
        "lock    (locks the swing door or level door player 1 is aiming at)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;
            ConsoleSetAimedDoorLocked(true);
        }
    );

    cImGuiConsole::RegisterCommand(
        "unlock",
        "unlock  (unlocks the swing door or level door player 1 is aiming at)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;
            ConsoleSetAimedDoorLocked(false);
        }
    );

    // ------------------------------------------------------------
    // kill  (whatever Player 1 is aiming at)
    cImGuiConsole::RegisterCommand(
        "kill",
        "kill    (kills and ragdolls the monster player 1 is aiming at)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;

            iLuxEntity* pEntity = ConsoleGetAimedEntity("kill");
            if (pEntity == nullptr) return;

            if (pEntity->GetEntityType() != eLuxEntityType_Enemy)
            {
                cImGuiConsole::AddLog("kill: '%s' is not a monster", pEntity->GetName().c_str());
                return;
            }

            iLuxEnemy* pEnemy = static_cast<iLuxEnemy*>(pEntity);

            if (pEnemy->IsKilled())
            {
                cImGuiConsole::AddLog("kill: '%s' is already dead", pEntity->GetName().c_str());
                return;
            }

            //Through GiveDamage so the death state, the sound state and the
            //OnDeath script callback all happen the way they normally do --
            //then OnKilled forced afterwards, because GiveDamage only calls it
            //behind the Allow Killing Monsters option and this command IS the
            //explicit instruction that option exists to withhold.
            //
            //Resistance off and strength well past any toughness value, or the
            //damage gets scaled to nothing before it lands.
            iLuxEnemy::SetIgnoreDamageResistance(true);
            pEnemy->GiveDamage(pEnemy->GetHealth() + 1000.0f, 100);
            iLuxEnemy::SetIgnoreDamageResistance(false);

            if (pEnemy->IsKilled() == false)
                pEnemy->OnKilled();

            //OnKilled starts the ragdoll for models that have one bound; this
            //builds the bodies first for the ones that ship without, which is
            //all of them.
            if (pEnemy->IsRagdolled() == false)
            {
                pEnemy->BuildRagdollBodies();
                pEnemy->StartRagdoll();
            }

            cImGuiConsole::AddLog("kill: '%s' killed", pEntity->GetName().c_str());
        }
    );

    // ------------------------------------------------------------
    // delete  (whatever Player 1 is aiming at)
    cImGuiConsole::RegisterCommand(
        "delete",
        "delete  (removes the entity player 1 is aiming at; areas need debugView on)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;

            iLuxEntity* pEntity = ConsoleGetAimedEntity("delete");
            if (pEntity == nullptr) return;

            //An area is invisible with debug view off, so the ray would be
            //deleting something the player cannot see and did not mean. With it
            //on they are looking straight at the box and it is fair game.
            if (pEntity->GetEntityType() == eLuxEntityType_Area &&
                ::ImGuiDebugMenu::GetDebugView() == false)
            {
                cImGuiConsole::AddLog("delete: '%s' is a script area -- turn debugView on first",
                                      pEntity->GetName().c_str());
                return;
            }

            //Room for one more. Anything this pushes out is destroyed for real,
            //and this is also where a shrunk undoArray finally takes effect.
            const int lFreedBefore = (int)gvConsoleDeleted.size();
            ConsoleTrimUndoHistory(gConsoleUndoArraySize - 1);
            const int lFreed = lFreedBefore - (int)gvConsoleDeleted.size();

            //Switched off rather than destroyed, so undo has something to put
            //back. For a prop this hides the mesh and stops its bodies and
            //lights, which is indistinguishable from gone.
            sConsoleDeletedEntity entry;
            entry.mpEntity    = pEntity;
            entry.mpMap       = gpBase->mpMapHandler->GetCurrentMap();
            entry.msName      = pEntity->GetName();
            entry.mbWasActive = pEntity->IsActive();

            pEntity->SetActive(false);

            gvConsoleDeleted.push_back(entry);

            if (lFreed > 0)
                cImGuiConsole::AddLog("delete: '%s' removed (%d older %s now gone for good, %d undoable)",
                                      entry.msName.c_str(), lFreed,
                                      lFreed == 1 ? "entity" : "entities",
                                      (int)gvConsoleDeleted.size());
            else
                cImGuiConsole::AddLog("delete: '%s' removed (%d undoable)",
                                      entry.msName.c_str(), (int)gvConsoleDeleted.size());
        }
    );

    // ------------------------------------------------------------
    // undo  (puts back the last thing `delete` removed)
    cImGuiConsole::RegisterCommand(
        "undo",
        "undo    (restores the most recently deleted entity)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;

            if (gvConsoleDeleted.empty())
            {
                cImGuiConsole::AddLog("undo: nothing to undo");
                return;
            }

            //Newest first, and keep going back if the top entries turn out to
            //be stale -- a map change leaves the history full of pointers the
            //map already destroyed on its way out.
            while (gvConsoleDeleted.empty() == false)
            {
                sConsoleDeletedEntity entry = gvConsoleDeleted.back();
                gvConsoleDeleted.pop_back();

                if (ConsoleDeletedIsLive(entry) == false)
                {
                    cImGuiConsole::AddLog("undo: '%s' belonged to a map that is no longer loaded",
                                          entry.msName.c_str());
                    continue;
                }

                //Back to whatever it was before, not blindly active: deleting
                //something that was already switched off by script and then
                //undoing should not quietly switch it on.
                entry.mpEntity->SetActive(entry.mbWasActive);

                cImGuiConsole::AddLog("undo: '%s' restored (%d left)",
                                      entry.msName.c_str(), (int)gvConsoleDeleted.size());
                return;
            }

            cImGuiConsole::AddLog("undo: nothing left to undo");
        }
    );

    // ------------------------------------------------------------
    // destroy  (whatever damageable thing Player 1 is aiming at)
    cImGuiConsole::RegisterCommand(
        "destroy",
        "destroy (breaks the barrel, crate, door or other breakable prop player 1 is aiming at)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;

            iLuxEntity* pEntity = ConsoleGetAimedEntity("destroy");
            if (pEntity == nullptr) return;

            //Monsters have their own command, and sending one through the prop
            //damage path would take its health down without any of the death
            //handling. Point at the right tool rather than half-doing it.
            if (pEntity->GetEntityType() == eLuxEntityType_Enemy)
            {
                cImGuiConsole::AddLog("destroy: '%s' is a monster -- use kill", pEntity->GetName().c_str());
                return;
            }

            if (pEntity->GetEntityType() != eLuxEntityType_Prop)
            {
                cImGuiConsole::AddLog("destroy: '%s' is not a prop", pEntity->GetName().c_str());
                return;
            }

            iLuxProp* pProp = static_cast<iLuxProp*>(pEntity);
            const tString sName = pProp->GetName();

            //Not GiveDamage: damage is refused by the DisableBreakable tick a
            //level designer can put on one door or one barrel, and overruling
            //exactly that tick is the point of this command. ForceBreak clears it
            //and then goes through the prop's OWN break, so the broken mesh, the
            //debris impulse, the sound and the particles all still happen.
            //
            //What it will NOT do is break a prop whose type has no break at all.
            //There is nothing to spawn or play for one of those, so forcing it
            //would just make the prop vanish -- which is `delete`, and `delete`
            //has undo.
            if (pProp->ForceBreak() == false)
            {
                cImGuiConsole::AddLog("destroy: '%s' is not breakable -- use delete", sName.c_str());
                return;
            }

            cImGuiConsole::AddLog("destroy: '%s' broken", sName.c_str());
        }
    );

    // ------------------------------------------------------------
    // lit / unlit  (the lamp Player 1 is aiming at)
    cImGuiConsole::RegisterCommand(
        "lit",
        "lit     (lights the candle or lamp player 1 is aiming at)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;
            ConsoleSetAimedLampLit(true);
        }
    );

    cImGuiConsole::RegisterCommand(
        "unlit",
        "unlit   (puts out the candle or lamp player 1 is aiming at)",
        [](const cImGuiConsole::tArgs& a)
        {
            (void)a;
            ConsoleSetAimedLampLit(false);
        }
    );

    // ------------------------------------------------------------
    // loadMap <path.map> [startPos]
    cImGuiConsole::RegisterCommand(
        "loadMap",
        "loadMap <maps/folder/file.map> [startPos]  (change level)",
        [](const cImGuiConsole::tArgs& a)
        {
            if (a.empty())
            {
                cImGuiConsole::AddLog("Usage: loadMap maps/main/ch01/07_archives_cellar.map");
                return;
            }

            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr || gpBase->mpEngine == nullptr)
            {
                cImGuiConsole::AddLog("loadMap: game not ready");
                return;
            }

            //A map change runs through CheckMapChange, which saves the map it is
            //leaving. There has to be one.
            if (gpBase->mpMapHandler->GetCurrentMap() == nullptr)
            {
                cImGuiConsole::AddLog("loadMap: only works in-game");
                return;
            }

            //Both separators: a path copied out of Explorer uses one, a path copied
            //out of a .hps uses the other.
            std::string sPath = a[0];
            std::replace(sPath.begin(), sPath.end(), '\\', '/');

            //cLuxMapHandler resolves a map as msMapFolder + fileName, plain string
            //concatenation, so the folder has to be split off and it has to keep its
            //trailing slash.
            const size_t lSlash = sPath.find_last_of('/');
            const std::string sFolder = (lSlash == std::string::npos) ? std::string("")
                                                                      : sPath.substr(0, lSlash + 1);
            const std::string sFile   = (lSlash == std::string::npos) ? sPath
                                                                      : sPath.substr(lSlash + 1);

            if (sFile.empty())
            {
                cImGuiConsole::AddLog("loadMap: no map file in '%s'", a[0].c_str());
                return;
            }

            //ChangeMap forces the extension itself, but the existence check below
            //needs the real name, so it is forced here too.
            const tString sFullPath = cString::SetFileExt(tString(sFolder + sFile), "map");
            const tString sBareName = cString::SetFileExt(tString(sFile), "map");

            //Checked BEFORE anything is started. A typo otherwise gets as far as
            //CheckMapChange, which has already faded out, saved the old map and torn
            //down the world sounds by the time LoadMap returns NULL -- it then logs
            //and returns, and nothing puts that back.
            //
            //Same searcher LoadFromFile uses to find a map's script. Both spellings
            //are tried so this cannot reject a real map over an indexing detail.
            cFileSearcher* pSearcher = gpBase->mpEngine->GetResources()->GetFileSearcher();
            if (pSearcher->GetFilePath(sFullPath).empty() &&
                pSearcher->GetFilePath(sBareName).empty())
            {
                cImGuiConsole::AddLog("loadMap: no such map '%s'", sFullPath.c_str());
                return;
            }

            //Empty means the map's first PlayerStart, which PlacePlayerAtStartPos
            //falls back to -- after logging an error about it, hence saying so.
            const std::string sStartPos = (a.size() > 1) ? a[1] : std::string("");

            gpBase->mpMapHandler->SetMapFolder(sFolder);
            gpBase->mpMapHandler->ChangeMap(sFile, sStartPos, "", "");

            cImGuiConsole::AddLog("loadMap: changing to '%s' (%s)", sFullPath.c_str(),
                                  sStartPos.empty() ? "first PlayerStart" : sStartPos.c_str());
        }
    );

    // ------------------------------------------------------------
    // VARIABLES
    //
    // Named values rather than actions: type the name to read it, "name value"
    // to write it. They are also what puts a live value beside a name in the
    // type-ahead drop-down, which is usually the reason you went looking for it.
    //
    // set/get above still work and still reach the same state -- these are the
    // same variables, reachable the short way.
    cImGuiConsole::RegisterVariable("fov", "Field of view in degrees (10-170)",
        []() -> std::string
        {
            if (gpBase == nullptr || gpBase->mpPlayer == nullptr) return "n/a";
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f", cMath::ToDeg(gpBase->mpPlayer->GetBaseFOV()));
            return buf;
        },
        [](const std::string& v)
        {
            if (gpBase == nullptr || gpBase->mpPlayer == nullptr)
            {
                cImGuiConsole::AddLog("fov: player not ready");
                return;
            }

            float fDeg = 0;
            try { fDeg = std::stof(v); }
            catch (...) { cImGuiConsole::AddLog("fov: expected a number (got '%s')", v.c_str()); return; }

            if (fDeg < 10.0f || fDeg > 170.0f)
            {
                cImGuiConsole::AddLog("fov: value out of range (10-170)");
                return;
            }

            const float fRad = cMath::ToRad(fDeg);
            gpBase->mpPlayer->SetBaseFOV(fRad);
            if (gpBase->mpPlayer2) gpBase->mpPlayer2->SetBaseFOV(fRad);

            cImGuiConsole::AddLog("fov = %.1f", fDeg);
        });

    cImGuiConsole::RegisterVariable("splitScreenMode", "1 = left/right, 2 = top/bottom, 3 = dual monitor",
        []() -> std::string
        {
            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr) return "n/a";
            const int lMode = gpBase->mpMapHandler->GetSplitScreenMode();
            const char* vNames[] = { "", "1 (Left/Right)", "2 (Top/Bottom)", "3 (Dual Monitor)" };
            return (lMode >= 1 && lMode <= 3) ? vNames[lMode] : "?";
        },
        [](const std::string& v)
        {
            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr)
            {
                cImGuiConsole::AddLog("splitScreenMode: map handler not ready");
                return;
            }

            int lMode = 0;
            try { lMode = std::stoi(v); }
            catch (...) { cImGuiConsole::AddLog("splitScreenMode: expected 1, 2 or 3"); return; }

            if (lMode < 1 || lMode > 3)
            {
                cImGuiConsole::AddLog("splitScreenMode: expected 1, 2 or 3");
                return;
            }

            gpBase->mpMapHandler->SetSplitScreenMode(lMode);
            cImGuiConsole::AddLog("splitScreenMode = %d", lMode);
        });

    cImGuiConsole::RegisterVariable("coop", "Split-screen co-op on or off",
        []() -> std::string
        {
            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr) return "n/a";
            return gpBase->mpMapHandler->GetCoopMode() ? "1" : "0";
        },
        [](const std::string& v)
        {
            if (gpBase == nullptr || gpBase->mpMapHandler == nullptr)
            {
                cImGuiConsole::AddLog("coop: map handler not ready");
                return;
            }

            const std::string s = ToLowerLocal(v);
            const bool bOn = (s == "1" || s == "true" || s == "on" || s == "yes");

            gpBase->mpMapHandler->SetCoopMode(bOn);
            cImGuiConsole::AddLog("coop = %s", bOn ? "1" : "0");
        });

    cImGuiConsole::RegisterVariable("sanityDrainDisabled", "Stop darkness draining sanity",
        []() -> std::string
        {
            if (gpBase == nullptr || gpBase->mpPlayer == nullptr) return "n/a";
            return gpBase->mpPlayer->GetSanityDrainDisabled() ? "1" : "0";
        },
        [](const std::string& v)
        {
            if (gpBase == nullptr || gpBase->mpPlayer == nullptr)
            {
                cImGuiConsole::AddLog("sanityDrainDisabled: player not ready");
                return;
            }

            const std::string s = ToLowerLocal(v);
            const bool bOn = (s == "1" || s == "true" || s == "on" || s == "yes");

            gpBase->mpPlayer->SetSanityDrainDisabled(bOn);
            cImGuiConsole::AddLog("sanityDrainDisabled = %s", bOn ? "1" : "0");
        });

    cImGuiConsole::RegisterVariable("undoArray", "How many deleted entities to keep for undo (1-64)",
        []() -> std::string
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "%d (%d held)",
                     gConsoleUndoArraySize, (int)gvConsoleDeleted.size());
            return buf;
        },
        [](const std::string& v)
        {
            int lSize = 1;
            try { lSize = std::stoi(v); }
            catch (...) { cImGuiConsole::AddLog("undoArray: expected a number"); return; }

            if (lSize < 1)  lSize = 1;
            if (lSize > 64) lSize = 64;

            const int lOld = gConsoleUndoArraySize;
            gConsoleUndoArraySize = lSize;

            //Shrinking deliberately does NOT free anything here. The trim
            //happens on the next delete, so dropping this to 1 mid-session
            //does not throw away nine things you might still want back while
            //you are working out which one you meant.
            if (lSize < lOld && (int)gvConsoleDeleted.size() > lSize)
                cImGuiConsole::AddLog("undoArray = %d (%d still held; the extra are freed on the next delete)",
                                      lSize, (int)gvConsoleDeleted.size());
            else
                cImGuiConsole::AddLog("undoArray = %d", lSize);
        });

    cImGuiConsole::RegisterVariable("consoleUnlockSound", "Sound the console's unlock plays when the door defines none",
        []() -> std::string { return gsConsoleUnlockSound; },
        [](const std::string& v)
        {
            gsConsoleUnlockSound = v;
            cImGuiConsole::AddLog("consoleUnlockSound = %s", gsConsoleUnlockSound.c_str());
        });

    cImGuiConsole::RegisterVariable("debugView", "Draw script areas and other invisible volumes, like the level editor",
        []() -> std::string { return ::ImGuiDebugMenu::GetDebugView() ? "1" : "0"; },
        [](const std::string& v)
        {
            const std::string s = ToLowerLocal(v);
            const bool bOn = (s == "1" || s == "true" || s == "on" || s == "yes");

            ::ImGuiDebugMenu::SetDebugView(bOn);
            cImGuiConsole::AddLog("debugView = %s", bOn ? "1" : "0");
        });

    cImGuiConsole::RegisterVariable("showWaterLurkers", "Draw the water lurker model the game normally hides",
        []() -> std::string { return ::ImGuiDebugMenu::GetShowWaterLurkers() ? "1" : "0"; },
        [](const std::string& v)
        {
            const std::string s = ToLowerLocal(v);
            const bool bOn = (s == "1" || s == "true" || s == "on" || s == "yes");

            ::ImGuiDebugMenu::SetShowWaterLurkers(bOn);
            cImGuiConsole::AddLog("showWaterLurkers = %s", bOn ? "1" : "0");
        });

    cImGuiConsole::RegisterVariable("showCollider", "Draw every physics shape as wireframe, players excepted",
        []() -> std::string { return ::ImGuiDebugMenu::GetShowColliders() ? "1" : "0"; },
        [](const std::string& v)
        {
            const std::string s = ToLowerLocal(v);
            const bool bOn = (s == "1" || s == "true" || s == "on" || s == "yes");

            ::ImGuiDebugMenu::SetShowColliders(bOn);
            cImGuiConsole::AddLog("showCollider = %s", bOn ? "1" : "0");
        });

    cImGuiConsole::AddLog("Lux console commands registered: spawn, give, coop, noclip, set, get, "
                          "lock, unlock, loadMap, kill, destroy, delete, undo, lit, unlit, "
                          "debugView, showCollider, showWaterLurkers, undoArray");
}
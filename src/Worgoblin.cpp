#include "worgoblin_loader.h"
#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "Config.h"
#include "StringFormat.h"

#include <algorithm>
#include <string>

enum Spells
{
    BEST_DEALS_ANYWHERE = 69044,
};

// --- Two Forms ------------------------------------------------------------
//
// Spell 68996 "Two Forms" is complete in both the server's and the client's
// Spell.dbc - real text, real icon, infinite duration - but nothing put it on
// skill line 789 "Racial - Worgen", so no worgen ever learned it. The row is
// added in SQL (data/sql/db-world); the behaviour is here.
//
// In Cataclysm you pick two complete appearances at character creation, and
// the client switches between them through a dedicated aura type
// (WORGEN_ALTERED_FORM). Neither that aura type nor a second customization set
// on the character exists in a 3.3.5 client, so both are built here:
//
//   * The MODEL comes from UNIT_FIELD_DISPLAYID, but the client composes the
//     TEXTURES (skin, face, hair) from the race byte in UNIT_FIELD_BYTES_0
//     looked up in CharSections.dbc. Setting only the display gives a human
//     with worgen skin, so BOTH are set.
//   * The race byte is safe to change on a player: Unit::getRace() returns
//     m_race, not the field, and the only place the field is read is behind
//     an IsPlayer() check that has already returned. _SaveCharacter() saves
//     the race from getRace(true) and the gender from PLAYER_BYTES_3, exactly
//     because "UNIT_BYTES_0 changes with every transform effect" (the core's
//     own comment). The field is rebuilt from the database on every login.
//   * The five appearance values, on the other hand, live in
//     PLAYER_BYTES/PLAYER_BYTES_2, which ARE saved. So worgen_form_appearance
//     holds the human set, the wolf set, and the human sets a save in human
//     form may have left in `characters` (data/sql/db-characters).
//
// The aura is a WISH for human form (WantsHuman()): combat brings the wolf
// out, and the human form comes back when combat ends
// (Worgoblin.TwoForms.CombatShift). There is no "Calm the Wolf" spell in the
// 4.x spell data - only the retired 68951/68952 "zzOld Dan's Altered Form
// On/Off" - so that behaviour lives in Two Forms itself.
//
// Darkflight (68992) also switches to wolf form: its own description in
// Spell.dbc says "Activates your true form".

enum WorgenTwoForms
{
    SPELL_TWO_FORMS      = 68996,
    SPELL_DARKFLIGHT     = 68992,
    RACE_WORGEN_ID       = 12,   // the free 3.3.5 race slot this module uses
    DISPLAY_HUMAN_MALE   = 49,
    DISPLAY_HUMAN_FEMALE = 50,
    NPC_GILNEAN_BARBER   = 9000001,
};

// Index into the two appearance arrays.
enum WorgenAppearanceIndex
{
    A_SKIN       = 0,
    A_FACE       = 1,
    A_HAIR       = 2,
    A_HAIRCOLOR  = 3,
    A_FACIALHAIR = 4,
    A_COUNT      = 5,
};

struct WorgenFormData : public DataMap::Base
{
    uint8 w[A_COUNT]     = { 0, 0, 0, 0, 0 };   // wolf form - the canonical one
    uint8 h[A_COUNT]     = { 0, 0, 0, 0, 0 };   // human form
    uint8 saved[A_COUNT] = { 0, 0, 0, 0, 0 };   // what the last save wrote
    bool  loaded     = false;
    bool  human      = false;   // the fields hold the human set
    bool  savedHuman = false;   // `saved` is a human set
    bool  updating   = false;   // inside UpdateForm()
};

static bool sCombatShift = true;

static WorgenFormData* Forms(Player* player)
{
    return player->CustomData.GetDefault<WorgenFormData>("WorgenFormData");
}

static bool IsWorgenPlayer(Unit const* unit)
{
    // getRace() reads m_race, not UNIT_FIELD_BYTES_0, so it keeps saying
    // "worgen" also while the client draws a human.
    return unit && unit->IsPlayer() && unit->getRace() == RACE_WORGEN_ID;
}

// A shapeshift form with its own model; warrior stances have none.
static bool HasModelForm(Unit* unit)
{
    for (AuraEffect const* eff : unit->GetAuraEffectsByType(SPELL_AURA_MOD_SHAPESHIFT))
        if (unit->GetModelForForm(ShapeshiftForm(eff->GetMiscValue()), eff->GetId()))
            return true;
    return false;
}

// The newest transform other than Two Forms.
static AuraEffect* OtherTransform(Unit* unit)
{
    AuraEffect* other = nullptr;
    for (AuraEffect* eff : unit->GetAuraEffectsByType(SPELL_AURA_TRANSFORM))
        if (eff->GetId() != SPELL_TWO_FORMS)
            other = eff;
    return other;
}

// Never touch the display while a form with its own model (a druid form) or
// another transform (a costume, an illusion) is on. That aura owns the display
// then, and a SetDisplayId() from here would win and leave a "bear" that looks
// like a human. Warrior stances are shapeshift forms without a model, so they
// don't count.
static bool CanChangeForm(Unit* unit)
{
    return IsWorgenPlayer(unit) && !HasModelForm(unit) && !OtherTransform(unit);
}

// getGender() reads UNIT_FIELD_BYTES_0, which SetDisplayId() sets to the
// gender of the model, so a costume changes it. PLAYER_BYTES_3 holds the
// character's.
static uint8 NativeGender(Player* player)
{
    return player->GetByteValue(PLAYER_BYTES_3, 0);
}

static uint32 HumanDisplay(Player* player)
{
    return NativeGender(player) == GENDER_MALE ? DISPLAY_HUMAN_MALE : DISPLAY_HUMAN_FEMALE;
}

static void ReadAppearance(Player* player, uint8* out)
{
    out[A_SKIN]       = player->GetByteValue(PLAYER_BYTES, 0);
    out[A_FACE]       = player->GetByteValue(PLAYER_BYTES, 1);
    out[A_HAIR]       = player->GetByteValue(PLAYER_BYTES, 2);
    out[A_HAIRCOLOR]  = player->GetByteValue(PLAYER_BYTES, 3);
    out[A_FACIALHAIR] = player->GetByteValue(PLAYER_BYTES_2, 0);
}

static void WriteAppearance(Player* player, uint8 const* in)
{
    player->SetByteValue(PLAYER_BYTES, 0, in[A_SKIN]);
    player->SetByteValue(PLAYER_BYTES, 1, in[A_FACE]);
    player->SetByteValue(PLAYER_BYTES, 2, in[A_HAIR]);
    player->SetByteValue(PLAYER_BYTES, 3, in[A_HAIRCOLOR]);
    player->SetByteValue(PLAYER_BYTES_2, 0, in[A_FACIALHAIR]);
}

static bool ClampHuman(Player* player, uint8* a);

static bool SameSet(uint8 const* a, uint8 const* b)
{
    return std::equal(a, a + A_COUNT, b);
}

static std::string SqlSet(uint8 const* a)
{
    if (!a)
        return "NULL,NULL,NULL,NULL,NULL";
    return Acore::StringFormat("{},{},{},{},{}", uint32(a[A_SKIN]), uint32(a[A_FACE]), uint32(a[A_HAIR]),
                               uint32(a[A_HAIRCOLOR]), uint32(a[A_FACIALHAIR]));
}

// hs_ and hp_ are the human sets `characters` may hold until the row is next
// written: hs_ what the last save wrote, hp_ what the save calling this
// writes. No flag, so nothing depends on when the save lands: the row is queued
// before the save's own transaction, and with CharacterDatabase.WorkerThreads
// = 1 (the default) the queue runs in order, so a crash can't leave a human set
// in `characters` that the row doesn't list.
static void SaveForms(Player* player, uint8 const* saving = nullptr)
{
    WorgenFormData* d = Forms(player);
    CharacterDatabase.Execute(
        "REPLACE INTO worgen_form_appearance "
        "(guid,w_skin,w_face,w_hair,w_haircolor,w_facialhair,"
        "h_skin,h_face,h_hair,h_haircolor,h_facialhair,"
        "hs_skin,hs_face,hs_hair,hs_haircolor,hs_facialhair,"
        "hp_skin,hp_face,hp_hair,hp_haircolor,hp_facialhair) "
        "VALUES ({},{},{},{},{})",
        player->GetGUID().GetCounter(), SqlSet(d->w), SqlSet(d->h),
        SqlSet(d->savedHuman ? d->saved : nullptr), SqlSet(saving));
}

static void ReadSet(Field* f, int first, uint8* out)
{
    for (uint8 i = 0; i < A_COUNT; ++i)
        out[i] = f[first + i].Get<uint8>();
}

static void LoadForms(Player* player)
{
    WorgenFormData* d = Forms(player);

    // `characters` holds the wolf set - including barbershop and
    // character-screen changes - unless it holds a human set the row lists.
    uint8 cur[A_COUNT];
    ReadAppearance(player, cur);
    std::copy(cur, cur + A_COUNT, d->w);
    std::copy(cur, cur + A_COUNT, d->h);
    d->human = false;
    d->savedHuman = false;

    QueryResult res = CharacterDatabase.Query(
        "SELECT w_skin,w_face,w_hair,w_haircolor,w_facialhair,"
        "h_skin,h_face,h_hair,h_haircolor,h_facialhair,"
        "hs_skin,hs_face,hs_hair,hs_haircolor,hs_facialhair,"
        "hp_skin,hp_face,hp_hair,hp_haircolor,hp_facialhair "
        "FROM worgen_form_appearance WHERE guid = {}",
        player->GetGUID().GetCounter());

    if (res)
    {
        Field* f = res->Fetch();
        ReadSet(f, A_COUNT, d->h);

        for (int first : { 2 * A_COUNT, 3 * A_COUNT })
        {
            if (f[first].IsNull())
                continue;

            uint8 listed[A_COUNT];
            ReadSet(f, first, listed);
            if (!SameSet(listed, cur))
                continue;

            // The server went down after a save in human form.
            std::copy(cur, cur + A_COUNT, d->saved);
            d->savedHuman = true;
            ReadSet(f, 0, d->w);
            WriteAppearance(player, d->w);
            break;
        }
    }

    // A gender change, or wolf choices with no human equivalent.
    ClampHuman(player, d->h);

    d->loaded = true;
    SaveForms(player);
}

// The barbershop has no script hook: WorldSession::HandleAlterAppearance ->
// Player::ChangeBarberShopStyle calls no scripts. So a haircut is detected by
// comparing. In human form, fields that differ from the human set can only
// come from the barbershop -> adopt them for the human form. The server only
// accepts worgen styles (it checks getRace()), so they are moved to valid
// human values. In wolf form the fields are the wolf set. Returns true if the
// human set changed.
static bool ReadBackFields(Player* player)
{
    WorgenFormData* d = Forms(player);
    if (!d->human)
    {
        ReadAppearance(player, d->w);
        return false;
    }

    uint8 cur[A_COUNT];
    ReadAppearance(player, cur);
    if (SameSet(cur, d->h))
        return false;

    std::copy(cur, cur + A_COUNT, d->h);
    if (ClampHuman(player, d->h))
        WriteAppearance(player, d->h);
    return true;
}

// Only through UpdateForm(), which guards against re-entry. `asked`:
// Unit::RestoreDisplayId() asked Two Forms, so no other aura owns the display.
static void SetWorgenForm(Player* player, bool human, bool asked)
{
    WorgenFormData* d = Forms(player);

    // Last chance to catch a haircut before the fields are overwritten.
    if (ReadBackFields(player))
        SaveForms(player);

    d->human = human;
    player->SetByteValue(UNIT_FIELD_BYTES_0, 0, human ? uint8(RACE_HUMAN) : uint8(RACE_WORGEN_ID));
    WriteAppearance(player, human ? d->h : d->w);

    if (!CanChangeForm(player))
        return;

    // In wolf form only replace our own human display, or one handed to Two
    // Forms: InitDisplayIds() (.modify gender) sets the new display before the
    // new native one.
    uint32 const current = player->GetDisplayId();
    bool const ours = current == DISPLAY_HUMAN_MALE || current == DISPLAY_HUMAN_FEMALE;
    uint32 const display = human ? HumanDisplay(player) : player->GetNativeDisplayId();
    if (current != display && (human || ours || asked))
    {
        // SetDisplayId() resets the scale; keep scale auras (Giant Growth).
        player->SetDisplayId(display);
        player->RecalculateObjectScale();
    }
}

// Human while Two Forms is on, unless another aura owns the display, combat
// brings the wolf out, or the player is logging out: leaving a battleground
// on logout ends combat after OnPlayerBeforeLogout and before the save, and
// the logout save must see the wolf.
static bool WantsHuman(Player* player)
{
    return player->HasAura(SPELL_TWO_FORMS) && CanChangeForm(player) &&
           !(sCombatShift && player->IsInCombat()) && !player->GetSession()->PlayerLogout();
}

static bool IsPlayerModel(Player* player, uint32 display)
{
    return display == player->GetNativeDisplayId() || display == DISPLAY_HUMAN_MALE ||
           display == DISPLAY_HUMAN_FEMALE;
}

// The one place the form is decided, called from every hook that can change
// WantsHuman() and from every display change. `asked`: Unit::RestoreDisplayId()
// asked Two Forms for the display.
static void UpdateForm(Player* player, bool asked = false)
{
    WorgenFormData* d = Forms(player);

    // SetDisplayId() and HandleEffect() below call OnDisplayIdChange.
    if (d->updating)
        return;
    d->updating = true;

    if (d->loaded)
        SetWorgenForm(player, WantsHuman(player), asked);

    // Another transform owns the display, but RestoreDisplayId() only asks the
    // newest transform, and under a warrior stance it sets the native display
    // without asking any. Hand the display back to that transform.
    if (!HasModelForm(player))
        if (AuraEffect* other = OtherTransform(player))
            if (asked || IsPlayerModel(player, player->GetDisplayId()))
                other->HandleEffect(player, AURA_EFFECT_HANDLE_SEND_FOR_CLIENT, true);

    d->updating = false;
}

// --- valid human appearances -----------------------------------------------
//
// A skin is a CharSections row with BaseSection SKIN, variation 0 and the skin
// colour as ColorIndex. A face is BaseSection FACE with the face as
// VariationIndex AND the skin colour as ColorIndex - so a face only exists for
// certain skin colours, and a skin change must validate the face again.
//
// The core has no lookup per race and type, so sCharSectionsStore is walked.
// Skin and face only from rows with SECTION_FLAG_PLAYER - what character
// creation offers; the rest are NPC skins. Rows with SECTION_FLAG_DEATH_KNIGHT
// only for death knights. Hair style and facial hair are what
// the barbershop accepts (BarberShopStyle.dbc), hair colour any CharSections
// row for that style: some barbershop styles only have non-player rows.
//
// The wolf choices are not always valid human ones: female worgen have facial
// hair 0-11 and skin 10-11, human females facial hair 0-6 and no skin 10-11.

static bool HumanSectionExists(Player* player, CharSectionType section, uint8 type, uint8 color,
                               bool playerOnly = true)
{
    bool const dk = player->getClass() == CLASS_DEATH_KNIGHT;
    for (CharSectionsEntry const* e : sCharSectionsStore)
        if (e->RaceID == RACE_HUMAN && e->SexID == NativeGender(player) &&
            e->BaseSection == uint32(section) && e->VariationIndex == type && e->ColorIndex == color &&
            (!playerOnly || ((e->Flags & SECTION_FLAG_PLAYER) && (dk || !(e->Flags & SECTION_FLAG_DEATH_KNIGHT)))))
            return true;
    return false;
}

static bool HumanSkinExists(Player* player, uint8 skin)
{
    return HumanSectionExists(player, SECTION_TYPE_SKIN, 0, skin);
}

static bool HumanFaceExists(Player* player, uint8 face, uint8 skin)
{
    return HumanSectionExists(player, SECTION_TYPE_FACE, face, skin);
}

// BarberShopStyle type: 0 hair style, 2 facial hair.
static bool HumanBarberStyleExists(Player* player, uint32 type, uint8 id)
{
    for (BarberShopStyleEntry const* e : sBarberShopStyleStore)
        if (e->type == type && e->race == RACE_HUMAN && e->gender == NativeGender(player) && e->hair_id == id)
            return true;
    return false;
}

// Steps forward (dir=1) or backward (dir=-1) to the next valid value. The loop
// is bounded by the 256 possible indices; if there is no other valid value,
// it stays put.
template<typename Valid>
static uint8 NextValid(uint8 cur, int dir, Valid valid)
{
    for (int step = 1; step < 256; ++step)
    {
        uint8 cand = uint8(((int(cur) + dir * step) % 256 + 256) % 256);
        if (valid(cand))
            return cand;
    }
    return cur;
}

static uint8 CycleSkin(Player* player, uint8 cur, int dir)
{
    return NextValid(cur, dir, [player](uint8 v) { return HumanSkinExists(player, v); });
}

static uint8 CycleFace(Player* player, uint8 cur, uint8 skin, int dir)
{
    return NextValid(cur, dir, [player, skin](uint8 v) { return HumanFaceExists(player, v, skin); });
}

// Moves every invalid value to the next valid one. Returns true if anything
// changed.
static bool ClampHuman(Player* player, uint8* a)
{
    uint8 const old[A_COUNT] = { a[0], a[1], a[2], a[3], a[4] };

    if (!HumanSkinExists(player, a[A_SKIN]))
        a[A_SKIN] = CycleSkin(player, a[A_SKIN], 1);
    if (!HumanFaceExists(player, a[A_FACE], a[A_SKIN]))
        a[A_FACE] = CycleFace(player, a[A_FACE], a[A_SKIN], 1);

    auto hairValid = [player](uint8 v) { return HumanBarberStyleExists(player, 0, v); };
    if (!hairValid(a[A_HAIR]))
        a[A_HAIR] = NextValid(a[A_HAIR], 1, hairValid);

    uint8 const hair = a[A_HAIR];
    auto colorValid = [player, hair](uint8 v) { return HumanSectionExists(player, SECTION_TYPE_HAIR, hair, v, false); };
    if (!colorValid(a[A_HAIRCOLOR]))
        a[A_HAIRCOLOR] = NextValid(a[A_HAIRCOLOR], 1, colorValid);

    auto facialValid = [player](uint8 v) { return HumanBarberStyleExists(player, 2, v); };
    if (!facialValid(a[A_FACIALHAIR]))
        a[A_FACIALHAIR] = NextValid(a[A_FACIALHAIR], 1, facialValid);

    return !std::equal(a, a + A_COUNT, old);
}

class worgoblin : public PlayerScript
{
public:
    worgoblin() : PlayerScript("worgoblin") { }

    void OnPlayerLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("Announce.enable", true))
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the Worgoblin module.");

        if (!IsWorgenPlayer(player))
            return;

        LoadForms(player);

        // The aura was re-applied during _LoadAuras, BEFORE this hook, when
        // the appearance was not loaded yet.
        UpdateForm(player);
    }

    // Called from WorldSession::LogoutPlayer before SaveToDB(), with
    // PlayerLogout() already true: the wolf set goes back into the fields, so
    // `characters` - and the character list - keeps the canonical form.
    void OnPlayerBeforeLogout(Player* player) override
    {
        if (IsWorgenPlayer(player))
            UpdateForm(player);
    }

    // After the logout save. A row written now no longer lists the human set
    // an earlier save wrote, so a later character-screen change can't match it.
    void OnPlayerLogout(Player* player) override
    {
        if (IsWorgenPlayer(player) && Forms(player)->loaded)
            SaveForms(player);
    }

    void OnPlayerSave(Player* player) override
    {
        if (!IsWorgenPlayer(player))
            return;

        WorgenFormData* d = Forms(player);
        if (!d->loaded)
            return;

        ReadBackFields(player);

        // Exactly what _SaveCharacter() writes right after this hook.
        uint8 cur[A_COUNT];
        ReadAppearance(player, cur);
        SaveForms(player, d->human ? cur : nullptr);

        std::copy(cur, cur + A_COUNT, d->saved);
        d->savedHuman = d->human;
    }

    void OnPlayerDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        trans->Append("DELETE FROM worgen_form_appearance WHERE guid = {}", guid);
    }

    void OnPlayerGetReputationPriceDiscount(Player const* player, FactionTemplateEntry const* factionTemplate, float& discount) override
    {
        if (!factionTemplate || !factionTemplate->faction)
            return;

        if (player->HasSpell(BEST_DEALS_ANYWHERE))
            discount *= 0.8;
    }

    void OnPlayerEnterCombat(Player* player, Unit* /*enemy*/) override
    {
        if (IsWorgenPlayer(player))
            UpdateForm(player);
    }

    void OnPlayerLeaveCombat(Player* player) override
    {
        if (IsWorgenPlayer(player))
            UpdateForm(player);
    }
};

class spell_rocket_barrage : public SpellScript
{
    PrepareSpellScript(spell_rocket_barrage);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        int32 basePoints = 0 + caster->GetLevel() * 2;
        basePoints += caster->SpellBaseDamageBonusDone(GetSpellInfo()->GetSchoolMask()) * 0.429; //BM=0.429 here, don't ask me how.
        basePoints += caster->GetTotalAttackPowerValue(caster->getClass() != CLASS_HUNTER ? BASE_ATTACK : RANGED_ATTACK) * 0.25; // 0.25=BonusCoefficient, hardcoding it here
        SetEffectValue(basePoints);
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_rocket_barrage::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_worgen_two_forms : public SpellScript
{
    PrepareSpellScript(spell_worgen_two_forms);

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!IsWorgenPlayer(caster))
            return SPELL_FAILED_DONT_REPORT;

        // The spell already has SPELL_ATTR0_CANT_USED_IN_COMBAT; this is belt
        // and braces, so a GM-granted cast cannot get around it.
        if (caster->IsInCombat())
            return SPELL_FAILED_AFFECTING_COMBAT;

        // Toggle. Without it a second cast would just refresh the aura, and
        // the player could never get back to wolf form on their own.
        if (caster->HasAura(SPELL_TWO_FORMS))
        {
            caster->RemoveAurasDueToSpell(SPELL_TWO_FORMS);
            return SPELL_FAILED_DONT_REPORT;
        }

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_worgen_two_forms::CheckCast);
    }
};

class spell_worgen_two_forms_aura : public AuraScript
{
    PrepareAuraScript(spell_worgen_two_forms_aura);

    // Two things here are not decoration.
    //
    // 1. PreventDefaultAction() in BOTH directions. Otherwise
    //    SPELL_AURA_TRANSFORM sets the display to the illusion creature 20708
    //    (Human Female Illusion) - female for everyone, and a creature display
    //    hides the player's gear.
    // 2. Registration on AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK and not only
    //    REAL. Unit::RestoreDisplayId() calls HandleEffect() again with
    //    AURA_EFFECT_HANDLE_SEND_FOR_CLIENT every time another transform or
    //    shapeshift aura falls off. Without the mask the core would take the
    //    display back behind our back and apply the illusion anyway.
    //
    // The aura survives logout (Aura::CanBeSaved() says yes: not passive, not
    // channeled, self-cast, infinite duration), so the form is remembered.
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes mode)
    {
        PreventDefaultAction();

        Player* target = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        if (IsWorgenPlayer(target))
            UpdateForm(target, mode == AURA_EFFECT_HANDLE_SEND_FOR_CLIENT);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        PreventDefaultAction();

        Player* target = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        if (IsWorgenPlayer(target))
            UpdateForm(target);
    }

    void Register() override
    {
        OnEffectApply  += AuraEffectApplyFn(spell_worgen_two_forms_aura::HandleApply,
                                            EFFECT_0, SPELL_AURA_TRANSFORM,
                                            AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK);
        OnEffectRemove += AuraEffectRemoveFn(spell_worgen_two_forms_aura::HandleRemove,
                                             EFFECT_0, SPELL_AURA_TRANSFORM,
                                             AURA_EFFECT_HANDLE_SEND_FOR_CLIENT_MASK);
    }
};

// Darkflight describes itself as "Activates your true form", so it ends the
// human form. The same behaviour as in Cataclysm.
class spell_worgen_darkflight : public SpellScript
{
    PrepareSpellScript(spell_worgen_darkflight);

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (IsWorgenPlayer(caster))
            caster->RemoveAurasDueToSpell(SPELL_TWO_FORMS);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_worgen_darkflight::HandleAfterCast);
    }
};

// --- Gilnean Barber -------------------------------------------------------
//
// The barbershop only does hair style, hair colour and facial hair. Skin and
// face are set at character creation in WotLK, so this NPC handles them.
// Everything is applied live, so the menu itself is the preview - which is why
// you have to be in human form to use it.

enum GilneanBarberActions
{
    GB_SKIN_NEXT = GOSSIP_ACTION_INFO_DEF + 1,
    GB_SKIN_PREV,
    GB_FACE_NEXT,
    GB_FACE_PREV,
    GB_RESET,
    GB_DONE,
};

class npc_gilnean_barber : public CreatureScript
{
public:
    npc_gilnean_barber() : CreatureScript("npc_gilnean_barber") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!IsWorgenPlayer(player))
        {
            creature->Whisper("I only cut Gilnean hair, I'm afraid.", LANG_UNIVERSAL, player);
            CloseGossipMenuFor(player);
            return true;
        }

        WorgenFormData* d = Forms(player);
        if (!d->loaded || !d->human)
        {
            creature->Whisper("Take your human form first, so I can see what I am doing. Use Two Forms.",
                              LANG_UNIVERSAL, player);
            CloseGossipMenuFor(player);
            return true;
        }

        BuildMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        WorgenFormData* d = Forms(player);
        if (!IsWorgenPlayer(player) || !d->loaded || !d->human)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        switch (action)
        {
            case GB_SKIN_NEXT:
            case GB_SKIN_PREV:
            {
                int dir = (action == GB_SKIN_NEXT) ? 1 : -1;
                d->h[A_SKIN] = CycleSkin(player, d->h[A_SKIN], dir);
                // Faces are tied to the skin colour; the old one may not exist
                // for the new skin, so move to the nearest one that does.
                if (!HumanFaceExists(player, d->h[A_FACE], d->h[A_SKIN]))
                    d->h[A_FACE] = CycleFace(player, d->h[A_FACE], d->h[A_SKIN], 1);
                break;
            }
            case GB_FACE_NEXT:
            case GB_FACE_PREV:
            {
                int dir = (action == GB_FACE_NEXT) ? 1 : -1;
                d->h[A_FACE] = CycleFace(player, d->h[A_FACE], d->h[A_SKIN], dir);
                break;
            }
            case GB_RESET:
                d->h[A_SKIN] = d->w[A_SKIN];
                d->h[A_FACE] = d->w[A_FACE];
                ClampHuman(player, d->h);
                break;
            case GB_DONE:
            default:
                SaveForms(player);
                creature->Whisper("Dressed to walk among humans again.", LANG_UNIVERSAL, player);
                CloseGossipMenuFor(player);
                return true;
        }

        WriteAppearance(player, d->h);   // live preview
        SaveForms(player);
        BuildMenu(player, creature);
        return true;
    }

private:
    static void BuildMenu(Player* player, Creature* creature)
    {
        WorgenFormData* d = Forms(player);

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "Next skin tone (now: " + std::to_string(uint32(d->h[A_SKIN])) + ")",
            GOSSIP_SENDER_MAIN, GB_SKIN_NEXT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Previous skin tone",
            GOSSIP_SENDER_MAIN, GB_SKIN_PREV);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            "Next face (now: " + std::to_string(uint32(d->h[A_FACE])) + ")",
            GOSSIP_SENDER_MAIN, GB_FACE_NEXT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Previous face",
            GOSSIP_SENDER_MAIN, GB_FACE_PREV);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Reset to my wolf choices",
            GOSSIP_SENDER_MAIN, GB_RESET);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "That looks right",
            GOSSIP_SENDER_MAIN, GB_DONE);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
};

class worgoblin_config : public WorldScript
{
public:
    worgoblin_config() : WorldScript("worgoblin_config", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sCombatShift = sConfigMgr->GetOption<bool>("Worgoblin.TwoForms.CombatShift", true);
    }
};

// Any display change can leave the fields and the display out of step with
// WantsHuman(): a costume or a polymorph ending under a warrior stance (then
// Unit::RestoreDisplayId() sets the native display), or ending in combat, or a
// druid form or costume starting in human form.
class worgoblin_display : public UnitScript
{
public:
    worgoblin_display() : UnitScript("worgoblin_display", true, { UNITHOOK_ON_DISPLAYID_CHANGE }) { }

    void OnDisplayIdChange(Unit* unit, uint32 /*displayId*/) override
    {
        if (IsWorgenPlayer(unit))
            UpdateForm(unit->ToPlayer());
    }
};

void Add_Worgoblin()
{
    new worgoblin();
    new worgoblin_config();
    new worgoblin_display();
    new npc_gilnean_barber();
    RegisterSpellScript(spell_rocket_barrage);
    RegisterSpellAndAuraScriptPair(spell_worgen_two_forms, spell_worgen_two_forms_aura);
    RegisterSpellScript(spell_worgen_darkflight);
}

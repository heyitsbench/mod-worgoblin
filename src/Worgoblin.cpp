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
//     holds both sets and is the source of truth (data/sql/db-characters).
//
// The aura is a WISH for human form: combat brings the wolf out, and the human
// form comes back when combat ends (Worgoblin.TwoForms.CombatShift). There is
// no "Calm the Wolf" spell in the 4.x spell data - only the retired
// 68951/68952 "zzOld Dan's Altered Form On/Off" - so that behaviour lives in
// Two Forms itself.
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
    uint8 w[A_COUNT] = { 0, 0, 0, 0, 0 };   // wolf form - the canonical one
    uint8 h[A_COUNT] = { 0, 0, 0, 0, 0 };   // human form
    bool  loaded     = false;
    bool  human      = false;               // is the player in human form now
};

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

// Never touch the form while a druid is shapeshifted. The shapeshift aura owns
// the display then, and a SetDisplayId() from here would win and leave a
// "bear" that looks like a human.
static bool CanChangeForm(Unit const* unit)
{
    return IsWorgenPlayer(unit) && unit->GetShapeshiftForm() == FORM_NONE;
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

static void SaveForms(Player* player)
{
    WorgenFormData* d = Forms(player);
    CharacterDatabase.Execute(
        "REPLACE INTO worgen_form_appearance "
        "(guid,w_skin,w_face,w_hair,w_haircolor,w_facialhair,"
        "h_skin,h_face,h_hair,h_haircolor,h_facialhair) "
        "VALUES ({},{},{},{},{},{},{},{},{},{},{})",
        player->GetGUID().GetCounter(),
        uint32(d->w[A_SKIN]), uint32(d->w[A_FACE]), uint32(d->w[A_HAIR]),
        uint32(d->w[A_HAIRCOLOR]), uint32(d->w[A_FACIALHAIR]),
        uint32(d->h[A_SKIN]), uint32(d->h[A_FACE]), uint32(d->h[A_HAIR]),
        uint32(d->h[A_HAIRCOLOR]), uint32(d->h[A_FACIALHAIR]));
}

static void LoadForms(Player* player)
{
    WorgenFormData* d = Forms(player);

    // Starting point: what is in `characters` right now.
    ReadAppearance(player, d->w);
    for (uint8 i = 0; i < A_COUNT; ++i)
        d->h[i] = d->w[i];
    d->human = false;

    QueryResult res = CharacterDatabase.Query(
        "SELECT w_skin,w_face,w_hair,w_haircolor,w_facialhair,"
        "h_skin,h_face,h_hair,h_haircolor,h_facialhair "
        "FROM worgen_form_appearance WHERE guid = {}",
        player->GetGUID().GetCounter());

    if (res)
    {
        Field* f = res->Fetch();
        for (uint8 i = 0; i < A_COUNT; ++i)
            d->w[i] = f[i].Get<uint8>();
        for (uint8 i = 0; i < A_COUNT; ++i)
            d->h[i] = f[i + A_COUNT].Get<uint8>();

        // The table is the source of truth. If the server died while the
        // player was in human form, `characters` holds the human set - this
        // puts it right.
        WriteAppearance(player, d->w);
    }
    else
        SaveForms(player);

    d->loaded = true;
}

// The barbershop has no script hook: WorldSession::HandleAlterAppearance ->
// Player::ChangeBarberShopStyle calls no scripts. So a haircut is detected by
// comparing. In human form, fields that differ from the saved human set can
// only come from the barbershop -> adopt them. That way the REAL barbershop
// styles the form you are in, as in Cataclysm.
static void AdoptBarbershopChanges(Player* player)
{
    WorgenFormData* d = Forms(player);
    if (!d->loaded || !d->human)
        return;

    uint8 cur[A_COUNT];
    ReadAppearance(player, cur);

    bool changed = false;
    for (uint8 i = 0; i < A_COUNT; ++i)
        if (cur[i] != d->h[i])
        {
            d->h[i] = cur[i];
            changed = true;
        }

    if (changed)
        SaveForms(player);
}

static void SetWorgenForm(Player* player, bool human)
{
    WorgenFormData* d = Forms(player);

    // Last chance to catch a haircut before the fields are overwritten.
    if (!human)
        AdoptBarbershopChanges(player);

    if (human)
    {
        player->SetByteValue(UNIT_FIELD_BYTES_0, 0, RACE_HUMAN);
        player->SetDisplayId(player->getGender() == GENDER_MALE
                             ? DISPLAY_HUMAN_MALE : DISPLAY_HUMAN_FEMALE);
        if (d->loaded)
            WriteAppearance(player, d->h);
    }
    else
    {
        player->SetByteValue(UNIT_FIELD_BYTES_0, 0, RACE_WORGEN_ID);
        player->SetDisplayId(player->GetNativeDisplayId());
        if (d->loaded)
            WriteAppearance(player, d->w);
    }

    d->human = human;
}

// When 0, the human form stays also in combat.
static bool CombatShiftEnabled()
{
    return sConfigMgr->GetOption<bool>("Worgoblin.TwoForms.CombatShift", true);
}

// --- valid human appearances -----------------------------------------------
//
// A skin is a CharSections row with BaseSection SKIN, variation 0 and the skin
// colour as ColorIndex. A face is BaseSection FACE with the face as
// VariationIndex AND the skin colour as ColorIndex - so a face only exists for
// certain skin colours, and a skin change must validate the face again.
//
// The core has no lookup per race and type, so sCharSectionsStore is walked.
// Only rows with SECTION_FLAG_PLAYER - what character creation offers; the
// rest are NPC skins.

static bool HumanSectionExists(Player* player, CharSectionType section, uint8 type, uint8 color)
{
    for (CharSectionsEntry const* e : sCharSectionsStore)
        if (e->RaceID == RACE_HUMAN && e->SexID == player->getGender() &&
            e->BaseSection == uint32(section) && e->VariationIndex == type && e->ColorIndex == color &&
            (e->Flags & SECTION_FLAG_PLAYER))
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

// Steps forward (dir=1) or backward (dir=-1) to the next valid value. The loop
// is bounded by the 256 possible indices; if there is no other valid value,
// it stays put.
static uint8 CycleSkin(Player* player, uint8 cur, int dir)
{
    for (int step = 1; step < 256; ++step)
    {
        uint8 cand = uint8(((int(cur) + dir * step) % 256 + 256) % 256);
        if (HumanSkinExists(player, cand))
            return cand;
    }
    return cur;
}

static uint8 CycleFace(Player* player, uint8 cur, uint8 skin, int dir)
{
    for (int step = 1; step < 256; ++step)
    {
        uint8 cand = uint8(((int(cur) + dir * step) % 256 + 256) % 256);
        if (HumanFaceExists(player, cand, skin))
            return cand;
    }
    return cur;
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
        // the appearance was not loaded yet. Set the form again now that it is.
        SetWorgenForm(player, player->HasAura(SPELL_TWO_FORMS) && CanChangeForm(player));
    }

    void OnPlayerBeforeLogout(Player* player) override
    {
        if (!IsWorgenPlayer(player))
            return;

        WorgenFormData* d = Forms(player);
        if (!d->loaded || !d->human)
            return;

        AdoptBarbershopChanges(player);

        // Write the wolf appearance back BEFORE SaveToDB. The hook is called
        // from WorldSession::LogoutPlayer before _player->SaveToDB(), so the
        // row in `characters` - and the character list - keeps the canonical
        // form.
        WriteAppearance(player, d->w);
    }

    void OnPlayerGetReputationPriceDiscount(Player const* player, FactionTemplateEntry const* factionTemplate, float& discount) override
    {
        if (!factionTemplate || !factionTemplate->faction)
            return;

        if (player->HasSpell(BEST_DEALS_ANYWHERE))
            discount *= 0.8;
    }

    // The wolf takes over in combat ...
    void OnPlayerEnterCombat(Player* player, Unit* /*enemy*/) override
    {
        if (!CombatShiftEnabled() || !CanChangeForm(player) || !player->HasAura(SPELL_TWO_FORMS))
            return;

        SetWorgenForm(player, false);
    }

    // ... and lets go again when combat is over.
    void OnPlayerLeaveCombat(Player* player) override
    {
        if (!CombatShiftEnabled() || !CanChangeForm(player) || !player->HasAura(SPELL_TWO_FORMS))
            return;

        SetWorgenForm(player, true);
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
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        PreventDefaultAction();

        Player* target = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        if (!target || !CanChangeForm(target))
            return;

        // The aura survives logout (Aura::CanBeSaved() says yes: not passive,
        // not channeled, self-cast, infinite duration), so the form is
        // remembered. Logging in in the middle of a fight shows the wolf.
        SetWorgenForm(target, !(target->IsInCombat() && CombatShiftEnabled()));
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        PreventDefaultAction();

        Player* target = GetTarget() ? GetTarget()->ToPlayer() : nullptr;
        if (target && CanChangeForm(target))
            SetWorgenForm(target, false);
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

void Add_Worgoblin()
{
    new worgoblin();
    new npc_gilnean_barber();
    RegisterSpellScript(spell_rocket_barrage);
    RegisterSpellAndAuraScriptPair(spell_worgen_two_forms, spell_worgen_two_forms_aura);
    RegisterSpellScript(spell_worgen_darkflight);
}

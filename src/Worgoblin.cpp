#include "worgoblin_loader.h"
#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "Config.h"

class announce : public PlayerScript {

public:
    announce() : PlayerScript("announce") { }

    void OnLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("Announce.enable", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the Worgoblin module."); // Hell yea
        }
    }
};

class rocket_barrage : public SpellScriptLoader //ID=69041
{
public:
    rocket_barrage() : SpellScriptLoader("spell_rocket_barrage") { }

    class rocket_barrage_script : public SpellScript
    {
        PrepareSpellScript(rocket_barrage_script);

        void HandleDamage(SpellEffIndex /*effIndex*/)
        {
            Unit* caster = GetCaster();
            int32 basePoints = 0 + caster->getLevel() * 2;
            basePoints += caster->SpellBaseDamageBonusDone(GetSpellInfo()->GetSchoolMask()) * 0.429; //BM=0.429 here, don't ask me how.
            basePoints += caster->GetTotalAttackPowerValue(caster->getClass() != CLASS_HUNTER ? BASE_ATTACK : RANGED_ATTACK) * 0.25; // 0.25=BonusCoefficient, hardcoding it here
            SetEffectValue(basePoints);
        }

        void Register() override
        {
            OnEffectLaunchTarget += SpellEffectFn(rocket_barrage_script::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new rocket_barrage_script;
    }
};

void Add_Worgoblin()
{
    new announce();
    new rocket_barrage();
}

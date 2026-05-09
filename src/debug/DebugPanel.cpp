#include "debug/DebugPanel.hpp"

namespace majo {

DebugConsoleLayout makeDefaultDebugConsoleLayout()
{
    return {{
        DebugTabDefinition{
            "tab_game",
            "タブ1",
            {
                DebugGroupDefinition{
                    "progress",
                    "ゲーム進行",
                    {
                        {DebugControlKind::Button, "reset_data", "データ初期化", "game reset-data"},
                        {DebugControlKind::Button, "return_base", "拠点へ", "game return-base"},
                    },
                },
                DebugGroupDefinition{
                    "game_data",
                    "ゲームデータ",
                    {
                        {DebugControlKind::Button, "money_10000", "所持金 +10000", "game money add 10000"},
                        {DebugControlKind::Button, "materials_100", "強化素材 +100", "game materials add 100"},
                    },
                },
                DebugGroupDefinition{
                    "combat_data",
                    "戦闘データ",
                    {
                        {DebugControlKind::Button, "hp_full", "HP最大", "game hp full"},
                        {DebugControlKind::Button, "hp_one", "HP1", "game hp set 1"},
                    },
                },
            },
        },
    }};
}

}

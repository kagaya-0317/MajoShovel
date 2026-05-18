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
                        {DebugControlKind::Dropdown, "launch_mode", "起動モード", "game launch-mode", 0, 0, {"タイトル前から", "拠点から", "ダンジョンから", "敵テスト"}, {"pre-title", "base", "dungeon", "enemy-test"}},
                        {DebugControlKind::Dropdown, "stage_unlock", "ステージ解放状態", "game stage-unlock", 0, 0, {"初期状態", "ステージ2解放", "ステージ3解放"}, {"initial", "stage2", "stage3"}},
                        {DebugControlKind::Button, "reset_data", "データ初期化", "game reset-data"},
                        {DebugControlKind::Button, "return_base", "拠点へ", "game return-base"},
                        {DebugControlKind::Button, "unlock_all_warps", "ワープポイント全開放", "game warp-points unlock-all"},
                        {DebugControlKind::Button, "save_data", "セーブ", "game save"},
                    },
                },
                DebugGroupDefinition{
                    "game_data",
                    "ゲームデータ",
                    {
                        {DebugControlKind::Button, "money_10000", "所持金 +10000", "game money add 10000"},
                        {DebugControlKind::Button, "materials_100", "強化素材 +100", "game materials add 100"},
                        {DebugControlKind::Button, "random_items_8", "ランダムアイテム +8", "game items random8"},
                        {DebugControlKind::Button, "item_picker", "任意アイテム追加", "game items picker"},
                    },
                },
                DebugGroupDefinition{
                    "combat_data",
                    "戦闘データ",
                    {
                        {DebugControlKind::Button, "hp_full", "HP最大", "game hp full"},
                        {DebugControlKind::Button, "hp_one", "HP1", "game hp set 1"},
                        {DebugControlKind::Button, "level_up", "レベルアップ", "game level-up"},
                        {DebugControlKind::Button, "enemy_test", "敵テスト", "game enemy-test"},
                    },
                },
            },
        },
        DebugTabDefinition{
            "tab_tools",
            "タブ2",
            {
                DebugGroupDefinition{
                    "edit_tools",
                    "編集ツール",
                    {
                        {DebugControlKind::Button, "base_edit_toggle", "拠点編集", "game base-edit toggle"},
                        {DebugControlKind::Button, "obj_image_scale_toggle", "画像サイズ編集", "game obj-image-scale toggle"},
                    },
                },
                DebugGroupDefinition{
                    "build_config",
                    "Build Config",
                    {
                        {DebugControlKind::Button, "build_debug", "Build: Debug", "dev build-config debug"},
                        {DebugControlKind::Button, "build_release", "Build: Release", "dev build-config release"},
                    },
                },
            },
        },
    }};
}

}

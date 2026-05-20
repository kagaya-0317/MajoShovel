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
                        {DebugControlKind::Dropdown, "launch_mode", "起動モード", "game launch-mode", 0, 0, {"タイトル前から", "拠点から", "ダンジョンから", "敵テスト", "エンディング後紙芝居", "エンディング後拠点"}, {"pre-title", "base", "dungeon", "enemy-test", "ending-kamishibai", "post-ending-base"}},
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
                        {DebugControlKind::Button, "codex_reset", "図鑑リセット", "game codex reset"},
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
                        {DebugControlKind::Button, "audio_bgm_edit", "BGM編集", "game audio-edit bgm"},
                        {DebugControlKind::Button, "audio_se_edit", "効果音編集", "game audio-edit se"},
                    },
                },
                DebugGroupDefinition{
                    "rematch_test",
                    "再戦確認",
                    {
                        {DebugControlKind::Dropdown, "rematch_stage", "対象ステージ", "game rematch target", 0, 0, {"ステージ1", "ステージ2", "ステージ3", "星間廃坑"}, {"stage1", "stage2", "stage3", "stage4"}},
                        {DebugControlKind::Button, "rematch_unlock_warps", "全ワープ発見済み", "game rematch unlock-warps"},
                        {DebugControlKind::Button, "rematch_mark_clear", "クリア済みにする", "game rematch mark-clear"},
                        {DebugControlKind::Button, "rematch_setup_regen", "再生成可能状態へ", "game rematch setup-regenerate"},
                        {DebugControlKind::Button, "rematch_boss_add", "捕獲ボス付与", "game rematch captured-boss add"},
                        {DebugControlKind::Button, "rematch_boss_remove", "捕獲ボス削除", "game rematch captured-boss remove"},
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
        DebugTabDefinition{
            "tab_test",
            "タブ3",
            {
                DebugGroupDefinition{
                    "test",
                    "テスト",
                    {
                        {DebugControlKind::Button, "story_event_test", "イベントテスト", "game story-test events"},
                        {DebugControlKind::Button, "story_tutorial_test", "チュートリアルテスト", "game story-test tutorials"},
                    },
                },
                DebugGroupDefinition{
                    "boss_flow",
                    "ボス確認",
                    {
                        {DebugControlKind::Dropdown, "boss_flow_stage", "対象ステージ", "game boss-flow target", 0, 0, {"ステージ1", "ステージ2", "ステージ3"}, {"stage1", "stage2", "stage3"}},
                        {DebugControlKind::Button, "boss_flow_before", "ボス直前へ", "game boss-flow before"},
                        {DebugControlKind::Button, "boss_flow_defeated", "撃破演出へ", "game boss-flow defeated"},
                        {DebugControlKind::Button, "boss_flow_clear", "クリア結果へ", "game boss-flow clear"},
                    },
                },
            },
        },
        DebugTabDefinition{
            "tab_astral_rogue",
            "ローグライク",
            {
                DebugGroupDefinition{
                    "astral_run",
                    "ラン・移動",
                    {
                        {DebugControlKind::NumberInput, "astral_depth", "深度", "game astral depth", 1, 9},
                        {DebugControlKind::Dropdown, "astral_move_target", "移動先", "game astral move-target", 1, 0, {"入口", "指定深度", "ボス前", "帰還口"}, {"entrance", "depth", "boss", "return"}},
                        {DebugControlKind::Button, "astral_start_new", "新規ラン開始", "game astral start-new"},
                        {DebugControlKind::Button, "astral_regenerate", "現在ランを再生成", "game astral regenerate"},
                        {DebugControlKind::Button, "astral_warp", "指定地点へ移動", "game astral warp"},
                        {DebugControlKind::Button, "astral_return_base", "拠点へ戻る", "game astral return-base"},
                    },
                },
                DebugGroupDefinition{
                    "astral_generation",
                    "生成・歪み",
                    {
                        {DebugControlKind::Dropdown, "astral_distortion", "歪みモード", "game astral distortion", 0, 0, {"深度自動", "なし固定", "星明かり固定", "星硬化固定", "残響湧き固定"}, {"auto", "none", "fading-starlight", "star-hardened", "echo-spawn"}},
                        {DebugControlKind::Dropdown, "astral_room", "特殊部屋", "game astral room", 0, 0, {"鉱石", "安全", "コイン", "宝物", "敵"}, {"ore", "safe", "coin", "treasure", "enemy"}},
                        {DebugControlKind::NumberInput, "astral_room_index", "部屋番号", "game astral room-index", 1, 20},
                        {DebugControlKind::Button, "astral_warp_room", "特殊部屋へ移動", "game astral room-warp"},
                        {DebugControlKind::Button, "astral_report_generation", "生成レポート", "game astral report-generation"},
                    },
                },
                DebugGroupDefinition{
                    "astral_result",
                    "終了・リザルト",
                    {
                        {DebugControlKind::Dropdown, "astral_result_kind", "終了結果", "game astral result", 0, 0, {"帰還成功", "死亡", "星脈竜撃破"}, {"returned", "died", "dragon-defeated"}},
                        {DebugControlKind::Toggle, "astral_stat_override", "統計オーバーライド", "game astral stat-override", 0, 1},
                        {DebugControlKind::NumberInput, "astral_stat_kills", "撃破数", "game astral stat kills", 0, 9999},
                        {DebugControlKind::NumberInput, "astral_stat_dug", "掘削数", "game astral stat dug", 0, 99999},
                        {DebugControlKind::NumberInput, "astral_stat_items", "アイテム数", "game astral stat items", 0, 9999},
                        {DebugControlKind::NumberInput, "astral_stat_materials", "素材量", "game astral stat materials", 0, 99999},
                        {DebugControlKind::NumberInput, "astral_stat_money", "お金", "game astral stat money", 0, 999999},
                        {DebugControlKind::Button, "astral_finish", "ラン終了して結果表示", "game astral finish"},
                        {DebugControlKind::Button, "astral_report_stats", "現在統計をログ出力", "game astral report-stats"},
                        {DebugControlKind::Button, "astral_high_score_reset", "ハイスコア初期化", "game astral high-score reset"},
                    },
                },
            },
        },
    }};
}

}

#include "debug/DebugPanel.hpp"

#include "game/DungeonEventDefinition.hpp"

#include <utility>

namespace majo {

namespace {

DebugControlDefinition makeDungeonEventPlacementControl()
{
    DebugControlDefinition control;
    control.kind = DebugControlKind::DropdownButton;
    control.id = "dungeon_event_place";
    control.label = "イベント配置";
    control.command = "game dungeon-event place";
    for (const DungeonEventDefinition& definition : dungeonEventDefinitions()) {
        if (!definition.debugPlaceable) {
            continue;
        }
        control.options.emplace_back(definition.displayName.data(), definition.displayName.size());
        control.optionCommands.emplace_back(definition.id.data(), definition.id.size());
    }
    return control;
}

DebugControlDefinition makeNumberControl(
    std::string id,
    std::string label,
    std::string command,
    int minValue,
    int maxValue,
    int initialValue)
{
    DebugControlDefinition control;
    control.kind = DebugControlKind::NumberInput;
    control.id = std::move(id);
    control.label = std::move(label);
    control.command = std::move(command);
    control.minValue = minValue;
    control.maxValue = maxValue;
    control.initialValue = initialValue;
    control.hasInitialValue = true;
    return control;
}

} // namespace

DebugConsoleLayout makeDefaultDebugConsoleLayout()
{
    return {{
        DebugTabDefinition{
            "tab_progress",
            "実行・進行",
            {
                DebugGroupDefinition{
                    "launch",
                    "起動",
                    {
                        {DebugControlKind::Dropdown, "launch_mode", "起動モード", "game launch-mode", 0, 0, {"タイトル前から", "拠点から", "ダンジョンから", "敵テスト", "弾テスト", "エンディング後紙芝居", "エンディング後拠点"}, {"pre-title", "base", "dungeon", "enemy-test", "projectile-test", "ending-kamishibai", "post-ending-base"}},
                        {DebugControlKind::Button, "return_base", "拠点へ", "game return-base"},
                        {DebugControlKind::Button, "save_data", "セーブ", "game save"},
                        {DebugControlKind::Button, "named_save_data", "名前を付けてセーブ", "game debug-save named"},
                        {DebugControlKind::Button, "load_named_save_data", "ロード", "game debug-save load"},
                        {DebugControlKind::Button, "reset_data", "データ初期化", "game reset-data"},
                    },
                },
                DebugGroupDefinition{
                    "progress_state",
                    "進行状態",
                    {
                        {DebugControlKind::Dropdown, "stage_unlock", "ステージ解放状態", "game stage-unlock", 0, 0, {"初期状態", "ステージ2解放", "ステージ3解放"}, {"initial", "stage2", "stage3"}},
                        {DebugControlKind::Button, "unlock_all_warps", "ワープポイント全開放", "game warp-points unlock-all"},
                        {DebugControlKind::DropdownButton, "codex_state", "図鑑状態", "game codex", 0, 0, {"リセット", "完成"}, {"reset", "complete"}},
                    },
                },
            },
        },
        DebugTabDefinition{
            "tab_player_items",
            "プレイヤー・所持品",
            {
                DebugGroupDefinition{
                    "player",
                    "プレイヤー",
                    {
                        {DebugControlKind::Dropdown, "hp_state", "HP設定", "game hp", 0, 0, {"最大", "1"}, {"full", "set 1"}},
                        makeNumberControl("hp_value", "HP値", "game debug hp-value", 1, 999, 1),
                        {DebugControlKind::Button, "hp_set_value", "HP値を適用", "game hp set-debug"},
                        {DebugControlKind::Button, "level_up", "レベル +1", "game level-up"},
                        makeNumberControl("target_level", "目標Lv", "game debug target-level", 1, 100, 1),
                        {DebugControlKind::Button, "level_set_target", "目標Lvに設定", "game level set-debug"},
                    },
                },
                DebugGroupDefinition{
                    "currency_materials",
                    "通貨・素材",
                    {
                        makeNumberControl("money_amount", "所持金加算額", "game debug money-amount", 1, 999999, 10000),
                        {DebugControlKind::Button, "money_add_amount", "所持金を加算", "game money add-debug"},
                        {DebugControlKind::Button, "money_reset", "所持金リセット", "game money reset"},
                        makeNumberControl("material_amount", "素材加算量", "game debug material-amount", 1, 99999, 100),
                        {DebugControlKind::Button, "materials_add_amount", "強化素材を加算", "game materials add-debug"},
                        {DebugControlKind::Button, "materials_reset", "強化素材リセット", "game materials reset"},
                    },
                },
                DebugGroupDefinition{
                    "ring_items",
                    "リング・アイテム",
                    {
                        {DebugControlKind::Button, "ring_workshop_unlock", "リング工房解禁", "game ring-workshop unlock"},
                        {DebugControlKind::Dropdown, "ring_unlock_state", "リング解禁状態", "game ring", 0, 0, {"リング1のみ", "リング2まで", "リング3まで"}, {"unlock reset", "unlock 2", "unlock 3"}},
                        makeNumberControl("random_item_count", "ランダムアイテム数", "game debug random-item-count", 1, 99, 8),
                        {DebugControlKind::Button, "random_items_add", "ランダムアイテム追加", "game items random-debug"},
                        {DebugControlKind::Button, "item_picker", "任意アイテム追加", "game items picker"},
                        {DebugControlKind::Button, "items_reset", "所持アイテムリセット", "game items reset"},
                    },
                },
            },
        },
        DebugTabDefinition{
            "tab_tests",
            "テスト",
            {
                DebugGroupDefinition{
                    "single_tests",
                    "単体テスト",
                    {
                        {DebugControlKind::Button, "enemy_test", "敵テスト", "game enemy-test"},
                        {DebugControlKind::Button, "effect_test", "エフェクトテスト", "game effect-test"},
                        {DebugControlKind::Button, "projectile_test", "弾テスト", "game projectile-test"},
                        {DebugControlKind::Button, "story_event_test", "イベントテスト", "game story-test events"},
                        {DebugControlKind::Button, "story_tutorial_test", "チュートリアルテスト", "game story-test tutorials"},
                        {DebugControlKind::Button, "dungeon_focus_test", "カメラフォーカス", "game dungeon-focus test"},
                        makeDungeonEventPlacementControl(),
                    },
                },
                DebugGroupDefinition{
                    "boss_rematch",
                    "ボス・再戦",
                    {
                        {DebugControlKind::Dropdown, "rematch_stage", "対象ステージ", "game rematch target", 0, 0, {"ステージ1", "ステージ2", "ステージ3", "星間廃坑"}, {"stage1", "stage2", "stage3", "stage4"}},
                        {DebugControlKind::Button, "boss_flow_before", "ボス直前へ", "game boss-flow before"},
                        {DebugControlKind::Button, "boss_flow_defeated", "撃破演出へ", "game boss-flow defeated"},
                        {DebugControlKind::Button, "boss_flow_clear", "クリア結果へ", "game boss-flow clear"},
                        {DebugControlKind::Button, "rematch_unlock_warps", "全ワープ発見済み", "game rematch unlock-warps"},
                        {DebugControlKind::Button, "rematch_mark_clear", "クリア済みにする", "game rematch mark-clear"},
                        {DebugControlKind::Button, "rematch_setup_regen", "再生成可能状態へ", "game rematch setup-regenerate"},
                        {DebugControlKind::DropdownButton, "rematch_captured_boss", "捕獲ボス", "game rematch captured-boss", 0, 0, {"付与", "削除"}, {"add", "remove"}},
                    },
                },
                DebugGroupDefinition{
                    "autosim",
                    "オートシミュ",
                    {
                        {DebugControlKind::Dropdown, "autosim_state", "状態", "autosim", 0, 0, {"開始", "停止"}, {"start", "stop"}},
                        {DebugControlKind::Slider, "autosim_speed", "速度", "autosim speed", 1, 16},
                        {DebugControlKind::Button, "autosim_report", "ログ", "autosim report"},
                    },
                },
            },
        },
        DebugTabDefinition{
            "tab_editing",
            "編集",
            {
                DebugGroupDefinition{
                    "map_collision",
                    "マップ・判定",
                    {
                        {DebugControlKind::Button, "base_edit_toggle", "拠点編集", "game base-edit toggle"},
                        {DebugControlKind::Button, "hitbox_toggle", "当たり判定編集", "game hitbox toggle"},
                        {DebugControlKind::Button, "enemy_shadow_toggle", "影編集", "game enemy-shadow toggle"},
                    },
                },
                DebugGroupDefinition{
                    "assets",
                    "アセット",
                    {
                        {DebugControlKind::Button, "obj_image_scale_toggle", "画像サイズ編集", "game obj-image-scale toggle"},
                        {DebugControlKind::Button, "audio_bgm_edit", "BGM編集", "game audio-edit bgm"},
                        {DebugControlKind::Button, "audio_se_edit", "効果音編集", "game audio-edit se"},
                    },
                },
                DebugGroupDefinition{
                    "development",
                    "開発",
                    {
                        {DebugControlKind::Dropdown, "build_config", "Build Config", "dev build-config", 0, 0, {"Debug", "Release"}, {"debug", "release"}},
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
                    "ラン",
                    {
                        {DebugControlKind::NumberInput, "astral_depth_meters", "深度m", "game astral depth", 0, 10000},
                        {DebugControlKind::NumberInput, "astral_depth_rank", "深度ランク", "game astral rank", 1, 60},
                        {DebugControlKind::Dropdown, "astral_move_target", "移動先", "game astral move-target", 1, 0, {"入口", "指定m", "指定ランク", "ボス前", "大穴"}, {"entrance", "meters", "rank", "boss", "hole"}},
                        {DebugControlKind::Button, "astral_start_or_regenerate", "ラン開始/再生成", "game astral start"},
                        {DebugControlKind::Button, "astral_warp", "指定地点へ移動", "game astral warp"},
                        {DebugControlKind::Button, "astral_jump_meters", "指定mエリアへジャンプ", "game astral jump-meters"},
                        {DebugControlKind::Button, "astral_return_base", "拠点へ戻る", "game astral return-base"},
                    },
                },
                DebugGroupDefinition{
                    "astral_generation",
                    "生成",
                    {
                        {DebugControlKind::Dropdown, "astral_distortion", "歪みモード", "game astral distortion", 0, 0, {"深度自動", "なし固定", "星明かり固定", "星硬化固定", "残響湧き固定"}, {"auto", "none", "fading-starlight", "star-hardened", "echo-spawn"}},
                        {DebugControlKind::Dropdown, "astral_room", "部屋", "game astral room", 0, 0, {"鉱石", "コイン", "宝物", "敵", "野良商人", "野良加工職人", "修練者"}, {"ore", "coin", "treasure", "enemy", "merchant", "artisan", "trainer"}},
                        {DebugControlKind::NumberInput, "astral_room_index", "部屋番号", "game astral room-index", 1, 20},
                        {DebugControlKind::Button, "astral_warp_room", "部屋へ移動", "game astral room-warp"},
                        {DebugControlKind::Button, "astral_report_generation", "生成レポート", "game astral report-generation"},
                    },
                },
                DebugGroupDefinition{
                    "astral_result",
                    "リザルト",
                    {
                        {DebugControlKind::Dropdown, "astral_result_kind", "終了結果", "game astral result", 0, 0, {"帰還成功", "死亡", "星脈竜撃破", "10000m到達"}, {"returned", "died", "dragon-defeated", "completed"}},
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

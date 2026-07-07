@event stage_02_boss_after
@title 2面 ボス後
@trigger boss_after:stage_02_junk_magic
@once story_stage_02_boss_after

@say junk_crab ジャンクラブ
ぼ、ぼ、ぼくのコレクションがあァァーー！

# ※爆発し、カニ料理になる。カニ料理は逃げていく
@dungeon_boss_explode_escape crab_dish 1.8

# ※カメラがルネの位置にもどる
@dungeon_camera_focus player 0.7

@wait small

@portrait_expr player 4
@say player ルネ
やったぁ！勝てたよ〜

@portrait_expr chicory 2
@say chicory チコリ
ちこり！

@wait small

@portrait_expr player 8
@say player ルネ
……カニさんのあの魔法、私のスペルリングにちょっと似てたなあ

@portrait_expr player 10
@say player ルネ
でも、違う…あれは道具を回してるっていうより、壊れた道具が勝手に暴れてたみたいだった

@portrait_expr chicory 3
@say chicory チコリ
こり？

@wait small

@say player ルネ
盾も、杖も、ランタンも…
もう壊れて使う人はいないのに、まだ自分の役目をやめられないみたいで……

@portrait_expr chicory 7
@say chicory チコリ
ちぃ…

@wait small

@portrait_expr player 14
@say player ルネ
チコリ？ まだ苦しいの？
大丈夫？

@say chicory チコリ
ちぃ……ちぃ……

@wait small

@portrait_expr player 10
@say player ルネ
ボスは倒したのに、チコリもまだ苦しそうだし…

@say player ルネ
ここ、ただのガラクタ置き場じゃないのかも

@say player ルネ
村長さんなら、ここの魔導具のこと詳しく知ってるかな

@portrait_expr player 1
@say player ルネ
いったん村に戻ろう、チコリ

@portrait_expr chicory 1
@say chicory チコリ
ちぃ…

@dungeon_return_to_base_after_story 0.8

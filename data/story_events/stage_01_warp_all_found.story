@event stage_01_warp_all_found
@title 1面 ワープ全発見
@trigger warp_all_found:stage_01_stardust
@once story_stage_01_warp_all_found

@say chicory チコリ
ちかちか！

@say player ルネ
え？これで、このあたりの時空の歪みは全部発見できたの？

@say chicory チコリ
ちこり！

@say player ルネ
やったあ！よかったね

@wait small

# ※地面が揺れる
@story_shake strong

@say player ルネ
わあ～～！

@say player ルネ
び、ビックリした…

@say chicory チコリ
ちぃ…

@say player ルネ
そうだね、なんだか嫌な気配がするね…

@wait small

# ※電話がかかってくる

@story_phone incoming

@say monica モニカ
ルネ、大丈夫！？今、穴から強い力を感じたの

@say player ルネ
モニカちゃん、こわいよ～

@say monica モニカ
もしかしたら、この先に強敵がいるかも…。気をつけて、ルネ
いったん拠点に帰って、準備を整えたほうがいいかも

@say player ルネ
ひ～ん

@say player ルネ
それじゃあ、またね～

@wait small

# ※電話をきる
@story_phone hangup
@portrait_hide monica

@event tutorial_warp
@title チュートリアル ワープ
@trigger tutorial:warp
@once story_tutorial_warp

# ※ワープポイントにカメラフォーカス

@say player ルネ
あっ！あれはなに！？

@say chicory チコリ
ちかちか！

@say player ルネ
チコリは知ってるの？

@say chicory チコリ
ちこり、ちかちか！

@say player ルネ
「落下した星が生み出した、時空の歪み」…？

@say player ルネ
……？

@say player ルネ
モニカちゃんに電話しないと

# ※電話する

@story_phone outgoing

@say player ルネ
モニカちゃ～ん

@say monica モニカ
ふ〜ん、時空の歪みね…それは転送魔法を利用する際の転送先、いわゆるワープポイントとして利用できるんじゃない？

@say player ルネ
？

@say monica モニカ
見つけたら位置を記録しておいて。拠点とそこを行き来できるようにしてあげる

@say player ルネ
さっすがモニカちゃん！なんでもできるんだね！

@say player ルネ
それじゃあ、またね～

# ※電話を切る

@story_phone hangup
@portrait_hide monica

@wait small

@say player ルネ
なんかね、便利みたいだよ
やったね、チコリ

@say chicory チコリ
ちこり！

@narration
ワープポイントを見つけたら、触れることでHPが全回復します。
また、拠点とワープポイントを行き来することができます。

# ※カメラフォーカスが戻る

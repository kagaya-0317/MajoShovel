@event stage_03_boss_before
@title 3面 ボス前
@presentation manual_camera
@trigger boss_before:stage_03_star_core
@once story_stage_03_boss_before

@portrait_expr player 15
@say player ルネ
な、なにこれ……

@wait small

# ※ルネの位置から、ゆっくりと外殻に包まれた守護星の位置までスクロール
@dungeon_boss_spawn default
@dungeon_camera_focus boss 1.2

@wait small

@portrait_expr player 14
@say player ルネ
守護星ちゃん……大丈夫！？

@portrait_expr chicory 6
@say chicory チコリ
ちぃ～！ちぃ～！

@wait small

# ※ルネの位置にスクロールを戻す
@dungeon_camera_focus player 0.7

@wait small

@say player ルネ
モニカちゃんに電話だっ！

# ※モニカに電話をかける
@story_phone outgoing

@portrait_expr monica 7
@say monica モニカ
ルネ、聞こえる？

@say monica モニカ
中央に守護星の反応……周りに、五つの封印……

@say monica モニカ
ごめん、通信が……ここから先は……

@say player ルネ
モニカちゃん？

@say monica モニカ
……壊さないで……掘って、道を……

# ※電話が切れる
@story_phone hangup
@portrait_hide monica

@portrait_expr player 16
@say player ルネ
モ、モニカちゃん！？モニカちゃ～ん！

@wait small

@say astragna アストラグナ
…深部干渉ヲ検知

@portrait_expr player 15
@say player ルネ
しゃべった……！？

@say astragna アストラグナ
コノ先、危険域…保護プログラム起動中

@say astragna アストラグナ
星封殻ヲ維持スル

@say player ルネ
なんか言ってる…

@wait small

@portrait_expr chicory 1
@say chicory チコリ
ちこり！

@portrait_expr player 8
@say player ルネ
え？守護星ちゃんは無事だって……？

@say player ルネ
あの大きなマシーンは、守護星ちゃんが
もっと下に沈まないように止めてるの？

@say chicory チコリ
ちこり

@portrait_expr player 1
@say player ルネ
そっか……守ってくれてるんだ

@portrait_expr player 18
@say player ルネ
でも、このままだと守護星ちゃんは帰れない

@portrait_expr player 19
@say player ルネ
大きいマシーン！聞いて！
守護星ちゃんを離してほしいの！

@say player ルネ
守ってくれてありがとう！
でも…守護星ちゃんを地上に帰してあげたいの！

@say player ルネ
地下は強い魔法がいっぱい残ってて…
守護星ちゃんやチコリが苦しそうなんだ！

@wait small

@say astragna アストラグナ
我ガ責務ハ、守護星ノ保護ノミ

@say astragna アストラグナ
接近者ハ皆、排除スル

@portrait_expr player 14
@say player ルネ
そんな…言うことを聞いてくれないなんて…

@portrait_expr player 18
@say player ルネ
どうやら…ルネたちが掘るしかなさそうだね！

@portrait_expr chicory 1
@say chicory チコリ
ちこり！

# ※外殻のほうへスクロール
@dungeon_camera_focus boss 0.9

@portrait_expr chicory 1
@say chicory チコリ
ちかちか！

@say player ルネ
なるほど…あの光ってる結晶が、外殻を守ってるんだ

@say player ルネ
先にあの結晶をぜんぶ壊して、それから掘っていけばいいんだね

@say chicory チコリ
ちこり！

@say player ルネ
マシーンを壊すんじゃない…
守護星ちゃんが帰る道を掘って、助けるんだ！

@wait small

@say astragna アストラグナ
我ノ名ハ「星封殻アストラグナ」

@say astragna アストラグナ
警告……警告……

@say astragna アストラグナ
接近者ヲ全力デ排除スル！！

@portrait_expr player 19
@say player ルネ
排除されるわけにはいかないよ！
ルネとチコリは、守護星ちゃんを迎えに来たんだから！

@say player ルネ
チコリ、行こう！

@portrait_expr chicory 2
@say chicory チコリ
ちこりっ！！

# ※ルネの位置にスクロールを戻す
@dungeon_camera_focus player 0.7

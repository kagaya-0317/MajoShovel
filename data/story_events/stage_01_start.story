@event stage_01_start
@title 1面 開始
@trigger stage_start:stage_01_stardust
@once story_stage_01_start

@portrait_expr player 4
@say player ルネ
よし、じゃあ、掘っていくぞ～！

@portrait_expr chicory 2
@say chicory チコリ
ちかちか！

@wait small

@portrait_expr player 10
@say player ルネ
でも、どこから掘っていけばいいんだろう…

@portrait_expr player 17
@say player ルネ
わかんなくなっちゃった～

@portrait_expr chicory 5
@say chicory チコリ
ちぃ…

@say player ルネ
モニカちゃんに電話しよ

@wait small

# ※電話する

@story_phone outgoing

@portrait_expr player 17
@say player ルネ
モニカちゃ～ん

@say monica モニカ
どこから掘ればいいかわかんなくなっちゃったの？

@portrait_expr player 10
@say player ルネ
うん

@say monica モニカ
たぶん星が落ちたほうは、土砂が崩れて塞がったばっかりで、まだ柔らかいと思うよ

@portrait_expr monica 3
@say monica モニカ
柔らかそうな茶色い壁が多いほうを掘っていけばいいんじゃないかな

@portrait_expr player 4
@say player ルネ
さっすがモニカちゃん！いつも頼りになるよ～

@say player ルネ
それじゃあ、またね～

@wait small

# ※電話を切る

@story_phone hangup
@portrait_hide monica

@wait small

@say player ルネ
よ～し！モニカちゃんの言う通り、茶色い壁が多いほうを掘ってみよう！

@portrait_expr chicory 1
@say chicory チコリ
ちこり！

@wait small

@narration
リングに乗せて回すアイテムには「耐久値」があります
壁にぶつけたり、敵にぶつけたりすると少しずつ減っていきます

@wait small

@narration
耐久値が0になると壊れてしまうので注意しましょう！

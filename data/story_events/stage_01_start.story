@event stage_01_start
@title 1面 開始
@trigger stage_start:stage_01_stardust
@once story_stage_01_start

@say player ルネ
よし、じゃあ、掘っていくぞ～！

@say chicory チコリ
ちかちか！

@wait small

@say player ルネ
でも、どこから掘っていけばいいんだろう…

@say player ルネ
わかんなくなっちゃった～

@say chicory チコリ
ちぃ…

@say player ルネ
モニカちゃんに電話しよ

@wait small

# ※電話する

@story_phone outgoing

@say player ルネ
モニカちゃ～ん

@say monica モニカ
どこから掘ればいいかわかんなくなっちゃったの？

@say player ルネ
うん

@say monica モニカ
たぶん星が落ちたほうは、土砂が崩れて塞がったばっかりで、まだ柔らかいと思うよ

@say monica モニカ
柔らかそうな茶色い壁が多いほうを掘っていけばいいんじゃないかな

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

@say chicory チコリ
ちこり！

@wait small

@narration
リングに乗せて回すアイテムには「耐久値」があります
壁にぶつけたり、敵にぶつけたりすると少しずつ減っていきます

@wait small

@narration
耐久値が0になると壊れてしまうので注意しましょう！

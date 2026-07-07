@event stage_03_start
@title 3面 開始
@trigger stage_start:stage_03_star_core
@once story_stage_03_start

@portrait_expr player 8
@say player ルネ
なんだか…地下なのに、星空みたいに光ってるね

@portrait_expr chicory 1
@say chicory チコリ
ちこり…

@portrait_expr player 18
@say player ルネ
この先に守護星ちゃんがいるんだね

@wait small

@portrait_expr player 3
@say player ルネ
あ、モニカちゃんから電話だ

# ※電話が鳴る
@story_phone incoming

@portrait_expr monica 7
@say monica モニカ
ルネ、聞こえる？

@say monica モニカ
そこから先は、地底のかなり深い部分だよ
通信もかなり乱れると思う

@portrait_expr player 1
@say player ルネ
うん…
でも大丈夫！

@say player ルネ
ここまで、ちゃんと自分で進んできたから

@portrait_expr player 18
@say player ルネ
やわらかい壁を掘る！
灯りを切らさない！

@portrait_expr player 18
@say player ルネ
モンスターがいっぱいいたら、逃げ道を作る！

@say player ルネ
新しいものを見つけたら、とりあえず回してみる！

@say chicory チコリ
ちこり！

@portrait_expr monica 1
@say monica モニカ
ルネ、完璧だね！
ここから先は、今まで覚えたこと全部が必要になると思うよ

@say monica モニカ
ルネならきっと行ける…
でも、くれぐれも、気をつけてね

@portrait_expr player 3
@say player ルネ
うん！

# ※電話を切る
@story_phone hangup
@portrait_hide monica

@wait small

@portrait_expr player 11
@say player ルネ
…怖くないって言ったら、ウソになっちゃうけど

@portrait_expr player 18
@say player ルネ
ここまで来たから、もう迷わない！

@say player ルネ
守護星ちゃん、待ってて
私が迎えに行くからね

@wait small

@say player ルネ
チコリ、行こう！

@portrait_expr chicory 2
@say chicory チコリ
ちかちか！

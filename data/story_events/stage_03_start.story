@event stage_03_start
@title 3面 開始
@trigger stage_start:stage_03_star_core
@once story_stage_03_start

@say player ルネ
なんだか…地下なのに、星空みたいに光ってるね

@say chicory チコリ
ちこり…

@say player ルネ
この先に守護星ちゃんがいるんだね

@wait small

@say player ルネ
あ、モニカちゃんから電話だ

# ※電話が鳴る
@story_phone incoming

@say monica モニカ
ルネ、聞こえる？

@say monica モニカ
そこから先は、地底のかなり深い部分だよ
通信もかなり乱れると思う

@say player ルネ
うん…
でも大丈夫！

@say player ルネ
ここまで、ちゃんと自分で進んできたから

@say player ルネ
やわらかい壁を掘る！
灯りを切らさない！

@say player ルネ
モンスターがいっぱいいたら、逃げ道を作る！

@say player ルネ
新しいものを見つけたら、とりあえず回してみる！

@say chicory チコリ
ちこり！

@say monica モニカ
ルネ、完璧だね！
ここから先は、今まで覚えたこと全部が必要になると思うよ

@say monica モニカ
ルネならきっと行ける…
でも、くれぐれも、気をつけてね

@say player ルネ
うん！

# ※電話を切る
@story_phone hangup
@portrait_hide monica

@wait small

@say player ルネ
…怖くないって言ったら、ウソになっちゃうけど

@say player ルネ
ここまで来たから、もう迷わない！

@say player ルネ
守護星ちゃん、待ってて
私が迎えに行くからね

@wait small

@say player ルネ
チコリ、行こう！

@say chicory チコリ
ちかちか！

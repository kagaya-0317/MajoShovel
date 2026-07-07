@event stage_01_clear
@title 1面 クリア
@trigger stage_clear:stage_01_stardust
@once story_stage_01_clear

@base_return_scene begin

@portrait_expr monica 4
@say monica モニカ
おかえりなさい、ルネ！
すごいじゃない！！

@say monica モニカ
ルネなら、やってくれると信じてたよ

@portrait_expr player 4
@say player ルネ
モニカちゃん！ありがとう～！

@portrait_expr player 17
@say player ルネ
でも疲れたよ～。なんか、巨大なモグラで、明らかにカタギではなくてさぁ

@portrait_expr monica 1
@say monica モニカ
カタギ…？とりあえず、あとでゆっくり聞かせてね

@wait small

@portrait_expr monica 7
@say monica モニカ
……穴の奥から、なんだか今までと違う反応がするの

@portrait_expr player 8
@say player ルネ
今までと違う反応？

@wait small

@say elder 村長
地下には、はるか先代の魔女たちが捨てた魔導具が埋まっておる

@say elder 村長
失敗した道具、壊れた道具、使われなくなった道具…
そういうものを、長いあいだ地底へ捨ててきたのじゃ

@portrait_expr monica 1
@say monica モニカ
それが堆積した層……魔導具廃棄層と呼ばれているわ

@say elder 村長
うむ。次に掘り進めるのは、そのエリアだろう

@wait small

@portrait_expr player 12
@say player ルネ
ま、まどーぐ…はいきそー？
なんか危険そう…

@say monica モニカ
でも、ルネの力でここまで来れたんだもの。
次もきっと大丈夫だよ

@portrait_expr player 1
@say player ルネ
うん…そうだね。がんばるぞ！

@portrait_expr player 4
@say player ルネ
えいえいおー！

@portrait_expr chicory 2
@say chicory チコリ
ちこりー！

@wait small

# ※チコリが帽子から飛び出し、ルネの周りを八の字に飛び回る
@base_chicory_figure8 2.2

@say player ルネ
あはは、チコリ、面白い動きしてるね！

@portrait_expr player 9
@say player ルネ
あ、ひらめいた

@portrait_expr monica 2
@say monica モニカ
どうしたの？

@portrait_expr player 1
@say player ルネ
今の私なら、できるかも…

@wait small

# ※ルネがスペルリングを広げる。リング1に加え、リング2も生成される
@base_ring_demo open 2 item_apple 1.15
@wait 2

@portrait_expr monica 5
@say monica モニカ
す、すごい……スペルリングが増えた！

@portrait_expr player 4
@say player ルネ
へへへ、できたよー！

@say chicory チコリ
ちかちか！

@wait small

@say elder 村長
なんと、リングを同時に2つとは…
ルネのスペルリングの素質は恐ろしいな…

@say elder 村長
でも地上でやるとまた色々壊すから、地下でやりなさい

@say player ルネ
は～い

@portraits_hide 0.35

@wait small

# ※リングを閉じる
@base_ring_demo close 0.55

@wait small

@base_return_scene end

@story_jingle ring_unlock

@narration
スペルリングの2つ目が解禁された！
8の字に動き、なおかつ回転する、トリッキーなリングだ！

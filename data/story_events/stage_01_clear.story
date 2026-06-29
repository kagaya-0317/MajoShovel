@event stage_01_clear
@title 1面 クリア
@trigger stage_clear:stage_01_stardust
@once story_stage_01_clear

@base_return_scene begin

@say monica モニカ
おかえりなさい、ルネ！
すごいじゃない！！

@say monica モニカ
ルネなら、やってくれると信じてたよ

@say player ルネ
モニカちゃん！ありがとう～！

@say player ルネ
でも疲れたよ～。なんか、巨大なモグラで、明らかにカタギではなくてさぁ

@say monica モニカ
カタギ…？とりあえず、あとでゆっくり聞かせてね

@wait small

@say monica モニカ
……穴の奥から、なんだか今までと違う反応がするの

@say player ルネ
今までと違う反応？

@wait small

@say elder 村長
地下には、はるか先代の魔女たちが捨てた魔導具が埋まっておる

@say elder 村長
失敗した道具、壊れた道具、使われなくなった道具…
そういうものを、長いあいだ地底へ捨ててきたのじゃ

@say monica モニカ
それが堆積した層……魔導具廃棄層と呼ばれているわ

@say elder 村長
うむ。次に掘り進めるのは、そのエリアだろう

@wait small

@say player ルネ
ま、まどーぐ…はいきそー？
なんか危険そう…

@say monica モニカ
でも、ルネの力でここまで来れたんだもの。
次もきっと大丈夫だよ

@say player ルネ
うん…そうだね。がんばるぞ！

@say player ルネ
えいえいおー！

@say chicory チコリ
ちこりー！

@wait small

# ※チコリが帽子から飛び出し、ルネの周りを八の字に飛び回る
@base_chicory_figure8 2.2

@say player ルネ
あはは、チコリ、面白い動きしてるね！

@say player ルネ
あ、ひらめいた

@say monica モニカ
どうしたの？

@say player ルネ
今の私なら、できるかも…

@wait small

# ※ルネがスペルリングを広げる。リング1に加え、リング2も生成される
@base_ring_demo open 2 item_apple 1.15
@wait 2

@say monica モニカ
す、すごい……スペルリングが増えた！

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

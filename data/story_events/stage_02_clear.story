@event stage_02_clear
@title 2面 クリア
@trigger stage_clear:stage_02_junk_magic
@once story_stage_02_clear

@say monica モニカ
おかえりなさい、ルネ！

@say monica モニカ
無事でよかった……！

@wait small

@say player ルネ
モニカちゃん、ただいま〜

@say chicory チコリ
ちぃ…

@wait small

@say monica モニカ
チコリ、まだ少し苦しそうだね

@say player ルネ
うん…

@say player ルネ
巨大なカニさんを倒したのに、あの場所はまだざわざわしてた…

@wait small

@say player ルネ
あのカニさんも、ただ凶暴だっただけじゃないと思う

@say player ルネ
壊れた道具が、勝手に暴れてるみたいで…
カニさんもそれにつられて暴走してるみたいだった

@wait small

@say elder 村長
……ルネ、おぬしは大事なものを見たようじゃな

@say player ルネ
大事なもの？

@wait small

@say elder 村長
魔法には、3段階ある。…本来魔力とは、土にも水にも空にも、自然の中にほんの少しずつ巡っておるものじゃ

@say elder 村長
その流れを借りるのが、「環流魔法」
流れを読み、乱れを整えるのが、「調律魔法」

@say elder 村長
そして…そこへ『燃えろ』『動け』『従え』と指令を込めると、魔法は強く、速くなる。これが「指令魔法」じゃな

@say monica モニカ
私たちが普段、戦闘訓練で使っている魔法ですね

@say elder 村長
うむ

@wait small

@say player ルネ
もしかして、まどーぐはいきそーの道具は……

@say elder 村長
そう、昔の魔女たちの指令魔法が消えずに残っておるのだろう

@say elder 村長
役目を失った道具が、それでも命令に従い続けようとし、暴れているのじゃ

@say player ルネ
だから、あの場所はまだざわざわしてたんだ……

@say elder 村長
その古い指令は、地下に溜まり続けてきた…

@say elder 村長
その汚染から我々を守り続けてきたのが、守護星なのじゃ
指令の乱れをほどき、地上へ漏れぬようにしてきた、調律の星

@say player ルネ
守護星ちゃんが、ずっと……

@say monica モニカ
守護星の役目は、指令の力を処理することだったのね
だから、私のような指令魔法は近づけるべきではない…

@say elder 村長
だが、ルネのスペルリングは違う

@say elder 村長
ルネのスペルリングは調律魔法じゃ。自然の魔力を上手く律し、流れるように物体を回す

@say elder 村長
これが、守護星の持つ性質と一致しているのじゃ

@say player ルネ
私のスペルリングが……

@wait small

@say player ルネ
村長さん、モニカちゃん

@say player ルネ
…私、守護星ちゃんを助けたい！

@say player ルネ
守護星ちゃんは、ずっと一人でこの村を守ってきてくれたんだ
人知れず、ずっと…

@wait small

@say monica モニカ
ルネ…

@say chicory チコリ
ちこり！

@say monica モニカ
チコリも感じてる…
次のエリアに、守護星の反応があるみたい。守護星はもうすぐだよ

@say chicory チコリ
ちかちか

@wait small

@say player ルネ
……最初は、自分の失敗を取り返さなきゃって思ってた

@say player ルネ
でも今は、それだけじゃない
これは、私にしかできないことなんだ

@say player ルネ
守護星ちゃんは、私が助けるんだ！

@wait small

# ※ルネがスペルリングを広げる。リング1・2に加え、リング3も生成される
@base_ring_demo open 3 item_apple item_ring=3 1.15

@say monica モニカ
ルネ！？
すごい……さらにスペルリングが増えてる！

@say chicory チコリ
ちかちか！

@say elder 村長
なんと、リングを同時に3つとは…
ルネのスペルリングの力が覚醒したようじゃな

# ※リングを閉じる
@base_ring_demo close 0.55

@say monica モニカ
ルネ…本当に、頼もしくなったね

@say elder 村長
…くれぐれも、気をつけるのじゃぞ

@say player ルネ
まかせてよ！ふんすか

@portrait_hide player

@wait small

@narration
スペルリングの3つ目が解禁された！
彗星のように離れた場所をゆっくり回転する、幻想的なリングだ！

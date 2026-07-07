@event stage_02_boss_before
@title 2面 ボス前
@trigger boss_before:stage_02_junk_magic
@once story_stage_02_boss_before

# ※奥から巨大なジャンクラブがゆっくり近づいてくる
@dungeon_boss_spawn walk_in

@wait small

@portrait_expr player 15
@say player ルネ
うわわ〜〜！

@wait small

@say junk_crab ジャンクラブ
フフ、それ、いい殻だね…
光る。硬い。回る。ぜんぶ、背中に合う。

@wait small

@say player ルネ
大きい、カニだ…

@portrait_expr player 14
@say player ルネ
きっと、まどうはいきぶつ…色んなガラクタで、大きな殻を作ってるんだ

@portrait_expr chicory 7
@say chicory チコリ
ちぃ…

@wait small

@say junk_crab ジャンクラブ
ぼくは魔導具コレクターのジャンクラブ
色んな魔導具を集めて持ち歩いてるんです…フフ…いいでしょ？

@say junk_crab ジャンクラブ
まあ、ちょっとばかり大きくなりすぎてしまいましたがね…コレクターという人種の悩みといいますか…フフフ…

@portrait_expr chicory 6
@say chicory チコリ
ちぃ…

@wait small

@say junk_crab ジャンクラブ
君たちのことも、収集していいかな？

@portrait_expr player 15
@say player ルネ
く、来る！

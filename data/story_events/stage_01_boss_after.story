@event stage_01_boss_after
@title 1面 ボス後
@trigger boss_after:stage_01_stardust
@once story_stage_01_boss_after

@say stardust_mole 星くずモグラ
ぐおお……

# ※星くずモグラが倒れる
@dungeon_boss_after_defeat

# ※その後、小さいただのモグラになり、走って逃げる
@dungeon_small_mole_escape

@wait small

@say player ルネ
や…やった～～～！！倒せたよ！！

@say chicory チコリ
ちこり！

@say player ルネ
あんなに大きなモンスター…

@say player ルネ
前だったら絶対逃げてたかも

@say player ルネ
でも、諦めないで…ちゃんと最後まで戦えた……

@wait small

@say player ルネ
えへへ
少しだけ、自信ついたかも

@say player ルネ
チコリと一緒だったおかげだよ。ありがとう、チコリ！

@say chicory チコリ
ちこり！

@wait small

@say player ルネ
あ、それとさ、チコリ

@say chicory チコリ
こり？

@say player ルネ
さっきの話し方、なに？

@say chicory チコリ
こり？？

@say player ルネ
……ううん。なんでもない

@wait small

@say player ルネ
とにかく、先へ進めるようになったね

@say player ルネ
でも、疲れた～。いったん拠点に戻ろっと

@dungeon_return_to_base_after_story 0.8

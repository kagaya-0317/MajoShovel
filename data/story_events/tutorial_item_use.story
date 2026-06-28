@event tutorial_item_use
@title チュートリアル アイテム使用
@trigger tutorial:item_use
@once story_tutorial_item_use

@say player ルネ
わあ、リンゴだあ
ルネ、リンゴ大好き！

@say chicory チコリ
ちかちか！

@say player ルネ
チコリも食べてみたい？
一緒に食べよう！

# ※非ゲームパッドの場合

@narration
リンゴのような使用アイテムは、下部のアイテム欄をクリックすることで使用できます
下部のアイテム欄

# ※ゲームパッドの場合

@narration
リンゴのような使用アイテムは、{act:UseSelectedItem}で使用できます

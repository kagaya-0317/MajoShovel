@event base_hint_merchant_full
@title 通知 商人ワゴン
@trigger base_hint:merchant_full
@once story_base_hint_merchant_full

@portrait_expr monica 3
@say monica モニカ
ルネ、おかえり！
わーっ、お宝いっぱい！
ルネが見つけてきたの！？

@portrait_expr player 4
@say player ルネ
えへへ、そうだよ〜

@base_facility_marker merchant_wagon show
@wait 0.2

@say monica モニカ
お宝は商人のお姉さんに
買い取ってもらうといいよ！

@say monica モニカ
あと、便利な道具も売ってるから
時々覗いてみるといいかも

@base_facility_marker merchant_wagon hide

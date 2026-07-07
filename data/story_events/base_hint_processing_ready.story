@event base_hint_processing_ready
@title 通知 作業台
@trigger base_hint:processing_ready
@once story_base_hint_processing_ready

@portrait_expr monica 3
@say monica モニカ
ルネ、おかえり！
わぁ！{world:enhancement_ore}強化鉱石がいっぱいだね！
ルネが見つけてきたの！？

@portrait_expr player 4
@say player ルネ
えへへ、そうだよ〜

@base_facility_marker processing_table show
@wait 0.2

@say monica モニカ
{world:enhancement_ore}強化鉱石があるなら、作業台で
加工職人さんにアイテムを強化してもらうといいよ！

@say monica モニカ
あと、無償で修理もしてくれるみたいだよ

@base_facility_marker processing_table hide

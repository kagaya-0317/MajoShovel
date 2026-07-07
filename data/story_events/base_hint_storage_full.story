@event base_hint_storage_full
@title 通知 収納箱
@trigger base_hint:storage_full
@once story_base_hint_storage_full

@portrait_expr monica 1
@say monica モニカ
ルネ、おかえり！
いっぱい、色々持ち帰ってきたね
リュックがぱんぱん！

@portrait_expr player 4
@say player ルネ
でへへ〜

@base_facility_marker storage_chest show
@wait 0.2

@say monica モニカ
そこの収納箱に、アイテムをしまっておけるよ
有効活用してね

@base_facility_marker storage_chest hide

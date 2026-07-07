@event base_hint_forge_ready
@title 通知 拠点強化炉
@trigger base_hint:forge_ready
@once story_base_hint_forge_ready

@say monica モニカ
ルネ！お帰り！
いっぱい探索してきたみたいだね！

@portrait_expr player 6
@say player ルネ
えへへ、がっぽりだよ

@base_facility_marker upgrade_forge show
@wait 0.2

@say monica モニカ
お金と素材があれば、「拠点強化炉」で
拠点の機能を拡張できるよ

@say monica モニカ
しかも、なんと拠点に限らず
ルネ自身もここでパワーアップできるみたい！

@say player ルネ
ほぇ〜
やったあ

@say monica モニカ
いちど覗いてみるといいよ！

@base_facility_marker upgrade_forge hide

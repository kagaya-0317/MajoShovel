@event base_hint_diary_save
@title 通知 日記
@trigger base_hint:diary_save
@once story_base_hint_diary_save

@say monica モニカ
ルネ、おかえり！
採掘は進んでる？

@say player ルネ
でへへ、まあまあかな

@say monica モニカ
きちんと進捗は日記に書かなきゃだめだよ
ルネのおうちに日記帳があったよね

@say player ルネ
そうだ、忘れてたあ

@base_facility_marker home_entrance show
@wait 0.2

@narration
日記を調べて、こまめにセーブしよう

@base_facility_marker home_entrance hide

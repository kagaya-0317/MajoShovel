# 施設・取引効果音

`zz_tmp_facility_forge_upgrade.wav`、`zz_tmp_facility_workbench_upgrade.wav`、`zz_tmp_facility_workbench_repair.wav`、`zz_tmp_merchant_transaction.wav`、`zz_tmp_enemy_rat_steal.wav` は、第三者素材を使用せず、リポジトリ内の `tools/generate_placeholder_audio.ps1` によって波形から生成したオリジナルの仮効果音です。

同スクリプトが生成する仮音源は、正式素材とファイル名だけで判別できるよう、すべて `zz_tmp_` から始めます。

再生成コマンド:

```powershell
.\tools\generate_placeholder_audio.ps1 -OnlyMissing -OnlySe -HighQualitySe
```

正式音源へ差し替える場合も `audio_manifest.tsv` の cue ID は維持します。

# 施設・取引効果音

`facility_forge_upgrade.wav`、`facility_workbench_upgrade.wav`、`facility_workbench_repair.wav`、`merchant_transaction.wav` は、第三者素材を使用せず、リポジトリ内の `tools/generate_placeholder_audio.ps1` によって波形から生成したオリジナルの仮効果音です。

再生成コマンド:

```powershell
.\tools\generate_placeholder_audio.ps1 -OnlyMissing -OnlySe -HighQualitySe
```

正式音源へ差し替える場合も `audio_manifest.tsv` の cue ID は維持します。

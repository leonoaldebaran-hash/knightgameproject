# Análise das músicas

Gerado automaticamente por `tools/analyze_music.py`. São medições, não uma audição:
andamento, tom estimado, energia e brilho ajudam a escolher o lugar de cada faixa.

| Faixa | Duração | BPM | Tom | Energia | Brilho | Notas por segundo | Percussão | Dinâmica | Intensidade ao longo da faixa |
|---|---|---|---|---|---|---|---|---|---|
| after_future | 4:09 | 103 | Fá# menor | baixa | médio (3008 Hz) | 2.7 | 0.70 | 12 dB | `▇█▇▆▆▇▇▆▄▂▅▆▂▅▄▁` |
| boas_vindas_primavera | 2:43 | 108 | Ré maior | alta | escuro (1231 Hz) | 5.0 | 0.15 | 11 dB | `▇▇█▇▇▅▇▅▇▄▂▇▅▄▅▁` |
| bubbles | 3:08 | 117 | Ré maior | alta | escuro (1486 Hz) | 4.2 | 0.08 | 9 dB | `▁▃▅▄▅▅▅▆▄▄▄▃▇█▇▂` |
| crimson_sky_requiem | 4:16 | 161 | Dó menor | média | escuro (2416 Hz) | 3.2 | 0.29 | 12 dB | `▁▄▃▆█▇▆▆▆▆▆▁▅▇▅▅` |
| ember_heart | 3:36 | 112 | Dó maior | média | brilhante (3267 Hz) | 2.5 | 0.13 | 5 dB | `▁▃▁▂▄▅▆▇▃▄▆▅▅▆▆█` |
| ember_heart_2 | 3:57 | 83 | Lá menor | alta | brilhante (3325 Hz) | 3.6 | 0.17 | 4 dB | `▁▅▅▆▄▂▆▇▆▅▅▆█▇▇▁` |
| ember_heart_3 | 3:27 | 83 | Dó menor | média | brilhante (3240 Hz) | 3.4 | 0.16 | 5 dB | `▁▅▅▆▅▅▂▄▇▅▆▇▇▇█▅` |
| get_ready | 5:19 | 172 | Fá menor | baixa | médio (2593 Hz) | 1.1 | 0.49 | 9 dB | `▆▆█▇▅▄▆▆▃▅▄▁▃▄▅▁` |
| paganini_fist | 4:47 | 152 | Lá menor | baixa | médio (3040 Hz) | 1.4 | 0.23 | 5 dB | `▁▄▄▆▃▅▅▆▄▅▆█▁▇▇▅` |

- **Energia:** volume médio + densidade de notas, comparado entre as faixas.
- **Brilho:** centro do espectro; mais alto = som mais agudo e aberto, mais baixo = mais grave e fechado.
- **Percussão:** quanto da energia vem de batidas em relação à parte melódica.
- **Dinâmica:** diferença entre os trechos mais altos e os mais baixos.

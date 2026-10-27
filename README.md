# Dragon Ball PSP (fan game 2D)

Jogo de luta 2D para PSP, escrito em C com pspsdk. 28 lutadores (Extreme Butoden + MUGEN), 8 cenarios.

## Compilar (GitHub Actions)
1. Crie um repositório e suba **todo** o conteúdo desta pasta (inclusive `data/` e `.github/`).
2. Aba **Actions** → o workflow "Build PSP" roda sozinho a cada push (ou clique em *Run workflow*).
3. Baixe o artefato **DragonBallPSP** → copie a pasta `PSP` para o cartão de memória (ou use no PPSSPP: abra `PSP/GAME/DragonBallPSP/EBOOT.PBP`).

## Controles
D-Pad/analógico: mover, ↑ pular, ↓ defender · X soco (X,X,X combo) · ■ chute forte · O ki blast (80 ki)
▲ super (500 ki) · R segurar = carregar ki · L dash · START pausa

## Regenerar os dados (opcional)
`python3 tools/pack_assets.py "<pasta do zip original extraída>" data` (requer Pillow).

# Roteiro do projeto (leia primeiro)

Este arquivo é o ponto de partida para retomar o trabalho a qualquer momento, inclusive numa
conversa nova com o Claude. Ele diz **como trabalhamos**, **o que já está pronto**, **o que vem
a seguir** e **onde está cada coisa**. Mantenha-o atualizado ao fim de cada tarefa.

- História, personagens, templos e estética: [GDD.md](GDD.md)
- Estrutura do motor (atores, salas, colisão, câmera, flags): [ARQUITETURA.md](ARQUITETURA.md)
- Instrumentos gravados da trilha: [SOUNDBANK.md](SOUNDBANK.md)
- Medições das músicas do autor: [MUSICA_ANALISE.md](MUSICA_ANALISE.md) · transcrições em [transcricao/](transcricao/)

## Diretriz principal (decisão do autor)
**A história, o mundo, os personagens e a arte são originais. Toda a parte técnica segue o Ocarina of
Time, estudado no código do Ship of Harkinian** (https://github.com/HarbourMasters/Shipwright):
movimento, câmera, combate, mira, colisão, atores, cenas e salas, áudio, HUD, save, cutscenes e o
jeito de construir dungeons. Antes de implementar um sistema, o Claude lê como o Ship of Harkinian faz
e reproduz a **técnica e o comportamento** com código próprio. **Nunca copia código nem assets**:
o código do Ship of Harkinian vem da decompilação de um jogo da Nintendo.

Escala: o Link adulto mede 68 unidades no Ocarina e o Mateo ~1,75 m, então 1 unidade ≈ 0,026 m.
O Ocarina roda a lógica a 20 quadros por segundo. As macros `OOT_DIST`, `OOT_SPEED`, `OOT_ACCEL`,
`OOT_TURN` e `OOT_FRAMES` em `src/game.h` convertem os números do Ocarina para o nosso mundo.

### Sistemas já no modelo do Ocarina
| Sistema | O que segue do Ocarina | Arquivo |
|---|---|---|
| Movimento | corrida 6,0/quadro, acelera 2,0, freia 1,5, giro 2000/quadro, gravidade 1,0 | `player.c` |
| Borda | sempre sai pulando: >5,9 pula 7,5; devagar 1,25 + 0,2 x velocidade | `player.c` |
| Esquivas | salto lateral 8,5 / 3,5; mortal 6,0 / 5,8; rolamento 1,5x a velocidade (mín. 3), 16 quadros | `player.c` |
| Bordas e escalada | até 18 sobe andando; 18–41 pulinho; 41–59 escalada baixa; 59–79,4 alta; 79,4–111 pula e agarra; empurrar 3/6 quadros | `player.c`, `world.c` |
| Golpe com pulo | mira + ação para a frente: impulso 6,0 / 5,8, dano dobrado (giratório também é dobrado) | `player.c` |
| Câmera | NORMAL (distância elástica 200–300, 10°, FOV 60, gira só andando), PARALELO (250, FOV 45), BATALHA (180, FOV 50) | `camera.c` |
| Atores | perfil (init/update/draw), categorias (inimigo, objeto, NPC...), marcadores (mirável, hostil), lista por categoria, Actor_Spawn/Kill | `actor.c` |
| Colisões | AT (ataque), AC (leva dano, AC_HARD = escudo faz ricochetear), OC (empurrão por massa), lâmina como quadrilátero varrido, tabela de dano por ator; checagem depois de todos atualizarem | `collision.c` |
| Música | tocadores principal e de inimigo, volume pela distância (150–500), modo parado, retomada | `music.c` |

**Ainda falta trazer para o modelo do Ocarina:** ficar pendurado ao cair de costas de uma borda,
andar para trás com a mira, virada rápida (quando o direcional inverte), colisão do cenário por polígonos
com tipos de superfície, cenas e salas, HUD com os botões, fanfarras.

**Assets que o autor gera em outras IAs, quando o Claude pedir:** músicas (Suno, com prompt escrito
pelo Claude) e imagens, como fundos pré-renderizados (salas internas e lojas, como no Ocarina),
texturas e artes conceituais.

## Como trabalhamos
1. O Claude implementa cada tarefa e faz commit e push direto na branch `main`.
2. O GitHub Actions compila e publica o jogo em **https://leonoaldebaran-hash.github.io/knightgameproject/**
3. O autor testa no navegador (confira o número da versão no canto inferior direito) e pede ajustes.
4. Nada precisa ser instalado no PC do autor. O Claude espera o deploy e avisa; o autor abre o jogo
   para testar. Se o Claude abrir o jogo no navegador interno, **fecha a aba depois**, porque o jogo toca música.
5. O R36S só será testado perto do fim. O build para o R36S já funciona e está guardado na branch `r36s-port`.

## Tecnologia
- C + **raylib 5.5**. O mesmo código compila para a web (WebAssembly) e para o R36S (ARM64).
- `.github/workflows/web.yml`: build web e deploy no GitHub Pages. Os erros do compilador aparecem como anotações.
- Orçamento do R36S: 640×480, 30 fps, ~10–20 mil triângulos por cena, texturas de 64–128 px.

### Código (`src/`)
| Arquivo | O que faz |
|---|---|
| `main.c` | Loop do jogo, HUD (corações, faixas de cinema), ordem de update/draw |
| `game.h` | Tipos e funções compartilhadas por todos os módulos |
| `input.c` | Teclado e controle (mapeamento abaixo) |
| `player.c` | Mateo: máquina de estados (normal, ataque, escudo, dano, esquiva), combo, vida |
| `camera.c` | Câmera em terceira pessoa e câmera de mira travada |
| `target.c` | Mira travada (Z-target): escolha do alvo, marcador, faixas de cinema |
| `world.c` | Cenário de teste e colisão (caixas e árvores) |
| `actor.c` | Sistema de atores: perfis, categorias, criar e remover, atualizar e desenhar |
| `collision.c` | Colisores AT/AC/OC no modelo do Ocarina |
| `soldier.c` | Ator EnSoldier: patrulha, perseguição, aviso, golpe (lâmina AT), brecha, dano pela tabela (corpo AC) |
| `dummy.c` | Ator EnDummy: boneco de treino imóvel e mirável |
| `sfx.c` | Efeitos sonoros e grunhidos de voz sintetizados |
| `synth.c` | Sequenciador estilo N64: partitura em MML tocada ao vivo com instrumentos gravados, estéreo, reverb e EQ |
| `songs.c` | As músicas, escritas em MML (sintaxe explicada no topo do arquivo) |
| `music.c` | Carrega o banco de instrumentos, toca a música, camada de combate, tecla de silenciar |

### Música (decisão do autor)
- **A trilha são as músicas do autor feitas no Suno**, em `assets/music_src/*.mp3`. O build converte
  cada uma para OGG com tratamento estilo N64 (32 kHz, agudos suavizados, volume igualado) em `public/music/`.
- **Precisa de música nova?** O Claude escreve o **prompt para o Suno** (clima, instrumentos, andamento,
  tom, referências), o autor gera a faixa e manda o arquivo, e o Claude a coloca no jogo.
- Distribuição das faixas pelos lugares: tabela na seção "Trilha sonora" do [GDD](GDD.md).
- `src/music.c` segue a lógica do Ocarina: dois tocadores (lugar + inimigo) tocando juntos; o volume da
  música de inimigo depende da distância do inimigo hostil mais próximo (4,5 a 15 = 150 a 500 unidades do
  Ocarina), e a do lugar no inverso; a de inimigo sempre começa do início; modo parado abaixa a música do
  lugar; cada faixa continua de onde parou; loop suave cruzando duas cópias.
- Ainda falta, no mesmo modelo: **fanfarras** (item obtido, segredo) num terceiro tocador com as outras em
  silêncio, e **ambiência de natureza à noite** no lugar da música.
- Padrão pedido pelo autor: épico, cinematográfico e progressivo (Zelda, Nightwish), grave forte,
  nada de som genérico de videogame, nada de caixa estridente.
- Tentativa descartada: compor nota a nota no motor sequenciado (`src/synth.c`, `src/songs.c`,
  banco CC0 `assets/soundbank.bin`). O resultado não agradou. O motor continua no código e pode servir
  para vinhetas curtas (baú, item obtido, segredo).
- Vozes: só grunhidos curtos. **Todo diálogo é texto.**

### Controles atuais
| Ação | Teclado | Controle |
|---|---|---|
| Andar | WASD / setas | Analógico esquerdo / D-pad |
| Câmera | Q / E | Analógico direito |
| Mira travada (segurar) / centralizar câmera | C | L1 |
| Espada (combo de 3, cancelável) | J | B |
| Escudo (segurar) | K | R1 |
| Golpe giratório | Segure J e solte quando brilhar | Segure B e solte |
| Rolar (sem a mira) | Espaço + direção | A + direção |
| Salto lateral / mortal (com a mira) | Espaço + direção | A + direção |
| Ligar/desligar música | M | Select |

## Feito
- [x] Tarefa 1: projeto, build web automático e página versionada
- [x] Tarefa 2: câmera em terceira pessoa e movimento com peso
- [x] Tarefa 3: código em módulos e colisão
- [x] Tarefa 4: combo de espada cancelável, escudo e bonecos de treino
- [x] Tarefa 5: soldado inimigo com IA, vida em corações, escudo defendendo
- [x] Tarefa 6: mira travada, andar em volta do alvo, salto lateral e mortal para trás
- [x] Som: grunhidos e efeitos; trilha com as músicas do autor (After Future no campo, Paganini Fist no combate)
      com loop suave (duas cópias se cruzando) e silêncio do começo e do fim cortado
- [x] Tarefa 7: rolamento e golpe giratório carregado
- [x] Tarefa 8: alturas (degraus até 0,45 sobem andando), gravidade, pulo automático ao sair correndo
      de uma borda, percurso de teste ao lado do início; troca campo/combate mais suave
      (combate só de perto, mistura de ~2 s, campo pausa e continua de onde parou)

## Próximas tarefas
**Fase 2: terminar o combate**
- [ ] 9: arqueiro e cavaleiro pesado com escudo

**Fase 3: sistemas**
- [ ] 10: diálogos em caixa de texto e NPCs
- [ ] 11: baús, itens, moeda e a vinheta de "item obtido"
- [ ] 12: inventário e 3 itens rápidos
- [ ] 13: portas, chaves e o sistema de cenas e salas
- [ ] 14: salvar e carregar
- [ ] 15: tela título

**Fase 4: fatia vertical**
- [ ] 16: modelo 3D e animações reais do Mateo
- [ ] 17: Black Waters Village (Johana, Vó Copélia, Claude)
- [ ] 18: cutscene da Grande Vazante
- [ ] 19–25: Templo da Terra completo (puzzles, mini-chefe, martelo, Guardião Titânico)

**Depois:** templos 2 a 8, Thalassa e extras, povos e Aion, Castelo Imperial, Kairós a cavalo,
transformação, Cronos no ar, epílogo, trilha de cada lugar e port final para o R36S.

**Pendências de música:** prompts do Suno para as faixas que faltam (Thalassa, Gaia, templos da Terra,
Ar, Sombras, Luz, Caos e Ordem, Johana/Reia, Hespérios, Cronos, final épico). Ver a tabela no GDD.

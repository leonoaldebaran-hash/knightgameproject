# Roteiro do projeto (leia primeiro)

Este arquivo é o ponto de partida para retomar o trabalho a qualquer momento, inclusive numa
conversa nova com o Claude. Ele diz **como trabalhamos**, **o que já está pronto**, **o que vem
a seguir** e **onde está cada coisa**. Mantenha-o atualizado ao fim de cada tarefa.

- História, personagens, templos e estética: [GDD.md](GDD.md)
- Estrutura do motor (atores, salas, colisão, câmera, flags): [ARQUITETURA.md](ARQUITETURA.md)
- Instrumentos gravados da trilha: [SOUNDBANK.md](SOUNDBANK.md)
- Medições das músicas do autor: [MUSICA_ANALISE.md](MUSICA_ANALISE.md) · transcrições em [transcricao/](transcricao/)

## Como trabalhamos
1. O Claude implementa cada tarefa e faz commit e push direto na branch `main`.
2. O GitHub Actions compila e publica o jogo em **https://leonoaldebaran-hash.github.io/knightgameproject/**
3. O autor testa no navegador (**Ctrl + F5**; confira o número da versão no canto inferior direito) e pede ajustes.
4. Nada precisa ser instalado no PC do autor. Antes de entregar, o Claude espera o deploy e confere a página,
   e **fecha o navegador interno** depois de testar, porque o jogo toca música.
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
| `soldier.c` | Soldado inimigo: patrulha, perseguição, aviso, golpe, brecha, dano |
| `dummy.c` | Bonecos de treino |
| `sfx.c` | Efeitos sonoros e grunhidos de voz sintetizados |
| `synth.c` | Sequenciador estilo N64: partitura em MML tocada ao vivo com instrumentos gravados, estéreo, reverb e EQ |
| `songs.c` | As músicas, escritas em MML (sintaxe explicada no topo do arquivo) |
| `music.c` | Carrega o banco de instrumentos, toca a música, camada de combate, tecla de silenciar |

### Música
- A música é **sequenciada como no N64**: notas em `src/songs.c`, tocadas por amostras de orquestra gravada.
- O banco `assets/soundbank.bin` é montado por `tools/build_soundbank.py` (workflow `soundbank.yml`)
  a partir de bibliotecas **CC0** (VSCO 2 CE e VCSL). Para mudar instrumentos, edite a lista no script.
- As músicas do autor (`assets/music_src/*.mp3`) são **inspiração**. `tools/transcribe.py`
  (workflow `transcribe.yml`, rodado à mão com o nome da faixa) extrai melodia, baixo e acordes.
- Vozes: só grunhidos curtos. **Todo diálogo é texto.**
- Padrão de qualidade pedido pelo autor: épico, cinematográfico e progressivo (Zelda, Nightwish),
  com grave forte, nada de som genérico de videogame e nada de caixa estridente.

### Controles atuais
| Ação | Teclado | Controle |
|---|---|---|
| Andar | WASD / setas | Analógico esquerdo / D-pad |
| Câmera | Q / E | Analógico direito |
| Mira travada (segurar) / centralizar câmera | C | L1 |
| Espada (combo de 3, cancelável) | J | B |
| Escudo (segurar) | K | R1 |
| Salto lateral / mortal (com a mira) | Espaço + direção | A + direção |
| Ligar/desligar música | M | Select |

## Feito
- [x] Tarefa 1: projeto, build web automático e página versionada
- [x] Tarefa 2: câmera em terceira pessoa e movimento com peso
- [x] Tarefa 3: código em módulos e colisão
- [x] Tarefa 4: combo de espada cancelável, escudo e bonecos de treino
- [x] Tarefa 5: soldado inimigo com IA, vida em corações, escudo defendendo
- [x] Tarefa 6: mira travada, andar em volta do alvo, salto lateral e mortal para trás
- [x] Som: grunhidos, efeitos, motor de música N64 com orquestra gravada, tema do campo (em ajuste com o autor)

## Próximas tarefas
**Fase 2: terminar o combate**
- [ ] 7: rolamento e golpe giratório carregado
- [ ] 8: pulo automático nas bordas, alturas e plataformas
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

**Pendências de música:** guitarra distorcida e bumbo duplo (pegada Nightwish); temas de
Black Waters, de cada templo, de Kairós e de Cronos.

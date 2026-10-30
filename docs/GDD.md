# Knight Game Project — Game Design Document (GDD)

> Documento vivo. Nomes e detalhes marcados como *(provisório)* podem mudar.

## 1. Visão geral

| Item | Definição |
|---|---|
| Gênero | Ação-aventura 3D em terceira pessoa, com dungeons e quebra-cabeças |
| Inspirações | Zelda: Ocarina of Time e Twilight Princess (estrutura e estética), Cavaleiros do Zodíaco (armaduras — apenas inspiração, designs originais), mitologia grega |
| Plataforma alvo | R36S (ArkOS/dArkOS, ARM64, Mali-G31, 1 GB RAM, tela 640×480) — port nativo |
| Plataforma de testes | Navegador (build WebAssembly no GitHub Pages) |
| Tecnologia | C + raylib (mesmo código para Web e R36S), build via GitHub Actions |
| Estética | Medieval, low-poly estilo N64/OoT, paleta e clima de Twilight Princess |
| Assets | Gerados com auxílio de IA (conceitos, texturas, modelos, música), otimizados para o R36S |

### Orçamento técnico (R36S)
- 30 fps a 640×480
- ~10–20 mil triângulos visíveis por cena
- Texturas 64×64 a 128×128
- Inimigos simultâneos na tela: até ~4–6

## 2. Pilares de jogo
1. **Combate de espada e escudo** com mira travada (Z-target), contra inimigos humanos de armadura.
2. **Dungeons com item obrigatório**: cada templo concede um item necessário para resolvê-lo e para avançar no mundo.
3. **A armadura suprema**: cada templo concede uma peça; completa, ela é usada no castelo final.
4. **Mistério revelado aos poucos**: lendas do vilarejo que se mostram fatos reais (estilo Horizon Zero Dawn).

## 3. Personagens

### Mateo — protagonista
- Loiro, olhos azuis, adulto, cavaleiro de espada e escudo.
- Nome: **魔 "ma"** (japonês, demônio) + **"theós"** (grego, deus). A dualidade demônio/deus é o tema central do personagem.
- Apaixonado por Johana sem perceber; descobre isso ao longo da jornada.
- No clímax, passa por uma transformação de raça: cabelo e olhos prateados, cabelo longo.

### Johana — a melhor amiga
- Cresceu com Mateo em Black Waters Village.
- É a reencarnação de **Reia**, esposa de Cronos.
- No dia da Grande Vazante entra em transe (olhos sem pupila) e sobe aos céus, recebida por Kairós.

### Kairós, o Cavaleiro de Prata — antagonista misterioso
- Cabelo prateado e longo, armadura negra e prateada.
- Arauto de Cronos. Na mitologia, Kairós é o "tempo oportuno", oposto de Cronos, o "tempo que devora".
- **Verdade:** é o Mateo do ciclo anterior, que falhou e foi corrompido (o lado *Ma*).

### Aurel, o Peregrino — aliado misterioso
- Cavaleiro andarilho, sarcástico, que sempre parece saber demais.
- **Verdade:** é **Hipérion**, titã da luz, irmão de Reia. Foi mentor do primeiro Mateo e tentou esconder Reia de Cronos no ciclo anterior.
- Acompanha Mateo no Templo da Luz, onde se revela e morre.

### Cronos — inimigo final
- Titã do tempo. Voltou de outra dimensão, onde criava um novo universo (o nosso mundo real).
- Quer reiniciar o mundo usando o poder de Reia, ao custo de toda a humanidade e da vida de Johana.

### Vó Copélia — anciã do vilarejo
- Guarda e conta as lendas de Black Waters. Sabe mais do que diz.
- O nome é anagrama de **Calíope**, a musa da poesia épica (C-A-L-I-O-P-E → C-O-P-E-L-I-A).

### Claude — o ferreiro
- Amigo de infância e rival de Mateo.
- Forja as melhorias de equipamento ao longo do jogo.
- O nome é uma homenagem ao Claude, a IA que ajuda a forjar este jogo.

### Outros *(provisório)*
- **Astra:** a égua de Mateo.
- **Nyx:** mercadora misteriosa que aparece em todos os biomas (loja do jogo).

## 4. Mundo

### Pangea
Única terra acima d'água que a humanidade conhece. Todos acreditam que é o mundo inteiro.

### Black Waters Village
Vilarejo natal de Mateo e Johana. Tem águas cristalinas, mas o nome vem de uma origem que ninguém lembra: surgiu logo após o apocalipse aquático.

### Gaia
O mundo antigo, submerso, revelado quando as águas baixam. Contém os biomas e os templos.

## 5. História

### A verdade (revelada aos poucos)
1. **O ciclo anterior:** milhões de anos atrás já existiram um Mateo e uma Johana. Cronos tentou reiniciar o mundo usando Reia; Mateo tentou impedir, falhou, e Johana morreu. Cronos ofereceu um pacto: "Sirva-me e um dia a terá de volta." Mateo aceitou e foi corrompido, tornando-se Kairós.
2. **A armadura dividida:** antes de ser corrompido, o primeiro Mateo dividiu sua armadura suprema em 8 templos, como última esperança para o próximo ciclo.
3. **O apocalipse aquático foi um reinício pela metade:** Cronos começou a inundar o mundo para reiniciá-lo, mas partiu no meio do processo para criar outro universo (o nosso). O planeta ficou afogado; só Pangea sobreviveu e Gaia ficou submersa.
4. **O retorno:** Cronos volta, encontra o mundo arruinado e decide terminar o reinício. As águas baixam, Gaia reaparece e Johana desperta como Reia. Ela dará à luz os deuses essenciais para refazer o mundo e morrerá com sua energia esgotada.

### Lendas do vilarejo × verdade
| Lenda | Verdade |
|---|---|
| "O Cavaleiro de Prata que chorou o mar" | Kairós inundou o mundo (e no clímax chora de verdade) |
| "A Mãe que dorme sob as águas" | Reia e Gaia submersa |
| "O Devorador que foi construir outro céu" | Cronos criando o nosso universo |
| "As oito chamas que guardam a armadura do primeiro herói" | Os templos e as peças da armadura |

### Estrutura em atos
**Prólogo** — Vida em Black Waters com Johana, Vó Copélia e Claude. Tutorial de combate.

**A Grande Vazante** — Evento sobrenatural: as águas baixam e Gaia é revelada. Johana entra em transe e sobe aos céus até os braços de Kairós. Mateo parte em jornada.

**A jornada** — 8 templos (ver seção 6).

**O Castelo Imperial** — O castelo afunda até o núcleo da Terra enquanto Reia começa a dar à luz os deuses.
1. **Batalha a cavalo contra Kairós:** Mateo em Astra contra Kairós em seu cavalo negro de prata. Mateo vence mas não o mata. Kairós recupera a memória e chora (a lenda se fecha). Ele se funde a Mateo: o *Ma* aceita o *Teo*.
2. **Transformação:** Mateo ganha cabelo e olhos prateados e longos — o que Kairós deveria ter sido.
3. **Batalha aérea contra Cronos:** com as asas da armadura completa, no espaço-tempo distorcido do núcleo.
4. **Desfecho:** Cronos é selado. Johana desperta, mas o poder dela e o de Mateo agora sustentam o mundo.

**Epílogo** — Mateo e Johana tornam-se constelações e entidades guardiãs de Gaia; não poderão viver no mundo que salvaram. Milênios depois, crianças de um vilarejo ouvem uma nova lenda: "O Cavaleiro de Prata que secou o mar." O ciclo se fecha, agora do lado bom.

## 6. Templos

| Nº | Templo | Bioma em Gaia | Item obrigatório | Peça da armadura | Momento da história |
|---|---|---|---|---|---|
| 1 | Terra | Cânions e ruínas | Martelo | Botas | Primeira descida a Gaia; conhece Aurel |
| 2 | Ar | Picos e pontes suspensas | Arco e flecha | Perneiras | Primeira aparição de Kairós, que humilha Mateo |
| 3 | Fogo | Vulcão | Bombas | Cinturão | Mural mostra "um herói loiro" do passado |
| 4 | Água | Cidade submersa | Máscara d'água (fôlego profundo) | Braceletes | Descoberta do apocalipse e da cidade antiga |
| 5 | Sombras | Floresta morta | Gancho | Ombreiras | Visões de Johana; Mateo entende o que sente |
| 6 | Luz | Deserto de cristal | Lançador de lanças explosivas | Peitoral | Aurel luta junto, revela-se Hipérion e morre |
| 7 | Caos | Ilhas flutuantes partidas | Botas de ferro | Elmo | Kairós tira o elmo: tem o rosto de Mateo |
| 8 | Ordem | Cidade geométrica perfeita | Asas/manto de planar | Asas | Diário do primeiro Mateo; Mateo entende a escolha dele |
| — | Castelo Imperial | Núcleo da Terra | Armadura completa | — | Kairós, transformação, Cronos |

Chefes: os **8 Guardiões Titânicos**, cavaleiros de Cronos com armaduras temáticas de constelação e elemento (designs originais).

### Design de dungeons (fortemente inspirado em Ocarina of Time)
Cada templo segue a mesma espinha dorsal:
1. **Entrada com desafio de acesso** — exige um item de um templo anterior ou uma habilidade do overworld.
2. **Salas com chaves pequenas** — portas trancadas, baús escondidos, inimigos que precisam ser derrotados para abrir a sala.
3. **Mapa e Astrolábio** — o mapa mostra as salas; o Astrolábio (equivalente à bússola) mostra baús e a posição do chefe.
4. **Mini-chefe** — um cavaleiro de elite que guarda o **item obrigatório** do templo.
5. **Segunda metade usa o item** — o item abre caminhos que antes eram impossíveis, e a dungeon "se reorganiza" na cabeça do jogador.
6. **Selo do Guardião** (chave do chefe) — em um baú grande, no fim de um puzzle maior.
7. **Chefe** — o Guardião Titânico, cujo ponto fraco exige o item do templo.
8. **Recompensa** — peça da armadura suprema + Fragmento de Vida completo (aumenta a vida).

Elementos de puzzle recorrentes: blocos para empurrar e puxar, interruptores de pressão, tochas para acender, alvos de olho para acertar com flecha, salas que só abrem após derrotar todos os inimigos, plataformas móveis, paredes rachadas, níveis de água variáveis, espelhos de luz, salas de ilusão e paredes falsas.

| Templo | Assinatura de puzzle |
|---|---|
| Terra | Blocos gigantes, pilares para esmagar com o martelo, plataformas que afundam |
| Ar | Correntes de vento, pontes suspensas, alvos distantes para o arco |
| Fogo | Lava que sobe e desce, muros rachados para bombas, tochas com tempo |
| Água | Labirinto de cachoeiras e tobogãs de água: escolher a correnteza certa leva a salas diferentes; desviar fluxos com comportas; mergulho com a máscara para alcançar túneis no fundo das quedas |
| Sombras | Paredes invisíveis e salas de ilusão, alvos de gancho no escuro |
| Luz | Espelhos e feixes de luz, lanças explosivas em cristais; puzzles em dupla com Aurel |
| Caos | Gravidade e salas que giram; botas de ferro para andar onde tudo desmorona |
| Ordem | Puzzles de padrão e simetria, salas espelhadas; planar entre torres |

## 7. Pangea: overworld, capital e povos

### Overworld
O mundo é dividido em zonas carregadas separadamente (como em OoT), com névoa à distância para caber no orçamento do R36S.

| Região | Descrição | Para cavalgar |
|---|---|---|
| Planície de Pangea | Campo central que liga tudo, com ruínas e árvores solitárias; ciclo de dia e noite | Grande área aberta, o "Hyrule Field" do jogo |
| Pradaria Dourada | Campos de trigo e moinhos ao pôr do sol | Estradas longas e cercas para saltar |
| Costa de Coral | Praias de areia branca e falésias sobre o mar | Galope pela areia na beira d'água |
| Vale dos Ventos | Desfiladeiros com cachoeiras e arcos naturais de pedra | Trilhas sinuosas e pontes |
| Black Waters Village | Vilarejo natal, à beira de um lago cristalino | — |

Em Gaia, cada bioma dos templos tem sua própria área de exploração.

### A capital: Thalassa
*"Thalassa" significa "mar" em grego — a cidade foi erguida pelos sobreviventes do apocalipse aquático, embora ninguém saiba mais disso.*

Uma cidade murada no centro de Pangea, com mercado, castelo, praças e ruas estreitas. Atividades:
- **Galeria de tiro com arco** — prêmio: aljava maior.
- **Boliche de bombas** — prêmio: bolsa de bombas maior.
- **Jogo dos baús** — escolher o baú certo com sorte e memória.
- **Arena dos Cavaleiros** — torneio de combate em rodadas contra soldados de armadura.
- **Hipódromo** — corridas a cavalo com Astra.
- **Lagoa de pesca** — minigame de pesca, com um peixe lendário.
- **Biblioteca das Lendas** — onde se entregam os Fragmentos de Lenda coletados; cada coleção desbloqueia um trecho da verdadeira história do mundo.
- **Lojas** — mercado de poções, equipamentos e itens; a mercadora Nyx aparece aqui à noite.
- **Missões de moradores** — o cão perdido, a criança que quer ser cavaleira, o guarda que perdeu a chave, a troca de itens em cadeia.

### Coletáveis e extras
- **Fragmentos de Vida** — 4 formam um coração extra; escondidos no mundo e em minigames.
- **Escamas do Tempo** — 100 escamas douradas espalhadas pelo mundo e pelos templos, com recompensas a cada 10 (estilo Gold Skulltulas).
- **Fragmentos de Lenda** — pedras gravadas com lendas antigas, entregues na Biblioteca de Thalassa.
- **Frascos** — guardam poções, água e outros itens.
- **Lira de Calíope** *(provisório)* — instrumento entregue pela Vó Copélia; melodias abrem passagens, chamam Astra e mudam o clima.
- **Grutas secretas** — cavernas escondidas sob pedras e paredes rachadas.

### Povos de Pangea
| Povo | Onde vivem | Cultura |
|---|---|---|
| Thalassianos | Thalassa | Comerciantes e soldados; a guarda real veste armaduras de bronze |
| Litos | Cânions de pedra | Mineiros e escultores; esculpem as lendas em pedra; guiam até o Templo da Terra |
| Aerídeos | Montanhas do Vale dos Ventos | Nômades falcoeiros que vivem em tendas penduradas nos penhascos |
| Pírricos | Encostas do vulcão | Ferreiros de forja sagrada; Claude aprende técnicas com eles |
| Marinheiros de Nereu | Costa de Coral | Pescadores e mergulhadores; ensinam a ler as marés |
| **Hespérios** | Falésias do extremo oeste, isolados do mundo | Ver abaixo |

### Os Hespérios e a espada ancestral Aion
- Povo isolado no extremo oeste de Pangea, onde o sol se põe. Não deixam estrangeiros entrar e falam uma língua antiga.
- Guardam **Aion**, uma espada ancestral cravada em pedra num santuário. *Aion*, na mitologia grega, é o tempo eterno e sem fim — o oposto do tempo que devora (Cronos).
- Ninguém nunca conseguiu empunhá-la. A lenda dos Hespérios diz: *"Aion só aceita quem já morreu uma vez."*
- **Quando:** após o Templo do Fogo (3º). Mateo precisa conquistar a confiança dos Hespérios com uma missão própria.
- **Por que Mateo consegue:** ele é a reencarnação do primeiro Mateo, que empunhou Aion no ciclo anterior. A anciã hespéria diz *"Ela já te conhecia"* — uma pista do plot twist muito antes da revelação.
- **No jogo:** Aion é a espada principal a partir dali, necessária para ferir os Guardiões a partir do Templo da Água e para romper as barreiras de Cronos.
- **No clímax:** na batalha a cavalo, Kairós usa a lâmina gêmea, escura, que ele forjou após ser corrompido.

## 8. Inimigos
- Somente humanos de armadura (soldados, arqueiros, cavaleiros pesados, guardiões).
- Armaduras com design inspirado no estilo de Cavaleiros do Zodíaco, sem copiar designs existentes.

## 9. Trilha sonora
- Orquestra épica + coral + guitarra no estilo anos 80, no clima de Cavaleiros do Zodíaco e Ocarina of Time.
- Composições originais (geradas com IA); não reproduzir melodias existentes.

## 10. Pendências
- [ ] Nomes dos 8 Guardiões Titânicos
- [ ] Design visual de Mateo, Johana, Kairós e Aurel
- [ ] Design de cada peça da armadura suprema
- [ ] Mapa do overworld de Pangea e Gaia

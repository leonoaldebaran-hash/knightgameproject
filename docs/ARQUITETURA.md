# Arquitetura do motor

Referência de estrutura: **Ship of Harkinian** (port de PC de Ocarina of Time feito a partir da decompilação do jogo original). Também roda no R36S via PortMaster, o que prova que um jogo do porte de OoT cabe nesse hardware.

> **Regra:** usamos o Ship of Harkinian só como referência de *ideias e organização*. O código dele vem da decompilação de um jogo da Nintendo e os assets saem da ROM do jogador. **Não copiamos código nem assets dele.** Todo o código e todos os assets deste projeto são originais.

## 1. Atores (`actor`)
Tudo que vive na cena é um ator: jogador, inimigos, baús, portas, interruptores, blocos, NPCs, projéteis.

```c
typedef struct Actor {
    int id;              // tipo do ator (ACTOR_SOLDADO, ACTOR_BAU...)
    int category;        // PLAYER, ENEMY, PROP, NPC, ITEM, DOOR...
    Vector3 pos, rot;
    float health;
    int params;          // dados do editor (qual item o baú dá, qual flag usa)
    void (*init)(struct Actor*);
    void (*update)(struct Actor*, float dt);
    void (*draw)(struct Actor*);
    void (*destroy)(struct Actor*);
} Actor;
```

- Cada tipo de ator fica no seu próprio arquivo (`src/actors/bau.c`, `src/actors/soldado.c`).
- As listas são separadas por categoria, o que facilita perguntas como "existem inimigos vivos na sala?".

## 2. Cenas e salas
- Uma **cena** é uma área inteira: o vilarejo, Thalassa, um templo.
- Uma cena é dividida em **salas**. Só a sala atual e as vizinhas ficam carregadas e são desenhadas.
- As portas e as transições conectam as salas.
- **Importante para o R36S:** é assim que mantemos poucos triângulos na tela e pouca RAM em uso.

## 3. Colisão separada do visual
- Cada sala tem uma **malha de colisão** própria, bem mais simples que o modelo visual.
- Cada polígono tem um **tipo de superfície**: chão, parede, água, lava, escalável, alvo de gancho, quebrável por martelo.
- É isso que permite regras como "o gancho só prende em madeira" ou "as botas de ferro afundam na água".

## 4. Jogador como máquina de estados
O jogador está sempre em um estado: parado, andando, correndo, atacando, defendendo, rolando, pulando, pendurado, escalando, nadando, mergulhando, empurrando bloco, mirando item, cavalgando, planando.

- Cada estado decide quais entradas aceita e para quais estados pode ir.
- Os itens (arco, gancho, martelo...) entram como estados novos ou subestados.

## 5. Câmera por configuração
- A câmera tem **modos**: normal, mira travada (Z-target), primeira pessoa (mirar o arco), fixa (salas especiais), chefe, cavalo.
- Cada sala ou região pode definir o modo padrão da câmera.

## 6. Flags de progresso
Cada cena guarda bits de estado, que são gravados no save:
- **Baús:** quais já foram abertos.
- **Interruptores:** portas abertas, blocos movidos, tochas acesas que ficam permanentes.
- **Salas concluídas:** inimigos que já foram derrotados.
- **Coletáveis:** Fragmentos de Vida e Escamas do Tempo já pegos.
- **Eventos globais:** progresso da história, templos concluídos, peças da armadura.

## 7. Itens equipáveis
- A espada e o escudo ficam sempre equipados.
- O R36S tem botões suficientes para **3 itens rápidos** (o equivalente aos botões C).
- Além disso, um menu de inventário, de equipamento e de mapa.

## 8. Cutscenes por roteiro
- As cenas da história são roteiros de dados: mover ator, tocar animação, falar, mover câmera, esperar.
- Assim o mesmo sistema serve para a Grande Vazante, para a morte de Aurel e para o epílogo.

## 9. Desempenho
- **Lógica a 30 Hz fixos e desenho interpolado.** O Ship of Harkinian usa interpolação para rodar acima da taxa original. Aqui fazemos o mesmo, para a física ficar estável e a imagem suave.
- **Orçamento por cena:** ~10–20 mil triângulos visíveis e texturas de 64–128 px.
- **Névoa** para esconder o limite de distância de desenho.

## Estrutura de pastas planejada
```
src/
  main.c
  core/      (loop, entrada, tempo, save)
  render/    (câmera, modelos, névoa)
  world/     (cenas, salas, colisão)
  actors/    (um arquivo por ator)
  player/    (máquina de estados e itens)
  ui/        (HUD, menus, diálogos)
assets/      (modelos, texturas, sons, música)
docs/
```

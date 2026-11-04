# Casas e carros dos jogadores

Cada jogador marca a própria casa em qualquer porta do mapa, guarda seus carros na frente dela e aparece lá ao entrar no
servidor. A ideia veio do servidor de RP em que o host jogava.

## Comandos

| Comando | O que faz |
|---|---|
| `/casa` | Abre o painel: suas casas, seus carros e os comandos |
| `/casa marcar` | Marca uma casa na porta onde você está. A primeira é grátis. Já tendo uma, a tela mostra o preço e você confirma com Y |
| `/casa remover` | Na porta ou dentro da casa: desiste dela e recebe metade do que pagou |
| `/casa principal` | Esta casa vira a principal (a do `/home` e a do começo da sessão) |
| `/casa interior` | Troca o interior desta casa pelo próximo livre |
| `/casa carro` | No volante, com o carro parado onde ele deve ficar: o carro fica seu. O primeiro é grátis, os outros custam o valor do modelo. Num carro que já é seu, atualiza a vaga, as cores e as peças |
| `/casa carro remover` | Dentro do seu carro (ou do lado): desiste dele, com metade do valor de volta |
| `/home` | Leva para a porta da casa principal. Não funciona em missão |

## Como funciona

- **Porta:** a casinha azul girando. Andando até ela, você entra num interior de casa do próprio jogo: as casas do roubo
  noturno, as casas compráveis e mais algumas (são 40). Cada casa do grupo recebe um interior que nenhuma outra tem,
  para ninguém se encontrar lá dentro. A seta lá dentro leva de volta para a porta.
- **Casa do Charlie Harper:** `/casa marcar` no terreno dela marca a própria casa: entra-se pelas portas dela, o
  disquete fica lá dentro, ao lado da porta da frente. Só um jogador do grupo pode tê-la.
- **Visitas:** os amigos veem a porta da sua casa ("Casa de Fulano") e podem entrar.
- **Disquete:** do lado da porta, abre o menu de salvar do jogo, como um disquete normal. Só aparece nas suas casas.
- **Radar:** suas casas aparecem com o ícone de casa de save.
- **Começo da sessão:** quem entra no servidor aparece na porta da casa principal. O host continua aparecendo onde salvou
  (o `/home` leva para casa). Ao morrer, todo mundo continua indo para o hospital.
- **Carros:** cada carro guardado aparece na vaga dele quando você chega a 120 m, com as cores, as peças da oficina e a
  pintura. Se você deixar o carro vazio longe de todo mundo (300 m), ele volta para a vaga. Se ele explodir ou um amigo sair
  com ele, um novo aparece na vaga depois que você se afastar e voltar.
- **Onde fica guardado:** no arquivo `coop_homes.bin`, na pasta de saves do seu jogo. Vale em qualquer servidor e
  continua depois de um jogo novo do host.

## Preços

Uma casa a mais custa como uma casa com terreno em 1992, o ano do jogo, pelo tipo de bairro que o próprio jogo dá ao
lugar:

| Bairro (zona do jogo) | Preço |
|---|---|
| Deserto | $70.000 |
| Fábricas fora da cidade | $100.000 |
| Gueto | $110.000 |
| Zona industrial | $120.000 |
| Interior (campo) | $130.000 |
| Bairro pobre | $140.000 |
| Perto do aeroporto | $150.000 |
| Bairro médio | $240.000 |
| Comércio | $300.000 a $320.000 |
| Centro e escritórios | $350.000 |
| Diversão (cassinos, boates) | $380.000 a $420.000 |
| Perto de parque | $400.000 |
| Praia | $600.000 |
| Clube de golfe | $750.000 |
| Comércio de luxo | $900.000 |
| Bairro rico | $1.500.000 |
| Morro rico isolado (Mulholland) | $3.000.000 |

San Fierro custa 20% a mais e Las Venturas 40% a menos, como era em 1992. Um carro a mais custa o valor que o
próprio jogo dá ao modelo.

## Garagem (próxima parte)

Nas casas comuns do GTA, a porta da garagem faz parte do prédio: não abre e não tem espaço atrás. A ideia é o carro
entrar devagar na porta enquanto a tela escurece e ficar guardado. Ao tirar, ele sai de ré. Nas garagens de verdade
do jogo, a porta sobe.

## Código

- `src/client/features/home/Home.cpp`: casas, interiores, portas, disquete, carros, painel e comandos.
- Mensagem de rede: `G_HOME` (179) em `src/common/api/protocol/Protocol.h`.

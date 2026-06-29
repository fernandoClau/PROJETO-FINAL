# 🚀 Verificador de Usuários com Filtro de Bloom e Tabela Hash

Este projeto foi desenvolvido como um trabalho prático para a disciplina de **Estrutura de Dados**. O sistema consiste em um gerenciador de banco de dados de usuários simulado na memória, projetado para demonstrar o ganho de eficiência ao utilizar um **Filtro de Bloom** como uma camada de otimização (cache probabilístico) antes de realizar consultas em uma **Tabela Hash** com encadeamento externo.

---

## 📌 Funcionalidades do Sistema (Requisitos)

O sistema conta com um menu interativo e atende aos seguintes requisitos funcionais:

* **`1. RF01 - Inserir Novo Usuário`**: Cadastra um identificador único, inserindo-o simultaneamente na Tabela Hash e ativando seus bits no Filtro de Bloom.
* **`2. RF02 - Consultar Usuário`**: Realiza a busca inteligente. O Filtro de Bloom é consultado primeiro; se ele garantir que o usuário não existe, a busca termina imediatamente. Caso contrário, a Tabela Hash é consultada para confirmação.
* **`3. RF03 - Exibir Estatísticas`**: Mostra métricas em tempo real sobre o uso do sistema (total de consultas, buscas evitadas pelo Bloom, taxa de falsos positivos detectados e tempo médio por consulta).
* **`4. RF04 - Inserir em Lote`**: Lê um arquivo de texto (`.txt`) contendo uma lista de usuários e faz a carga em lote para as estruturas.
* **`5. Executar Experimentos`**: Uma bateria automatizada de testes que gera conjuntos de dados (1k, 10k e 100k usuários), compara o tempo de busca com e sem o Filtro de Bloom, e calcula o percentual real de falsos positivos.

---

## 🛠️ Detalhes Técnicos

### Algoritmos de Hash Utilizados
Para garantir uma distribuição uniforme dos dados e minimizar colisões no Filtro de Bloom e na Tabela Hash, foram implementadas manualmente duas funções de espalhamento amplamente consagradas:
1.  **DJB2**: Algoritmo ágil baseado em manipulação de bits e multiplicações por números primos (constante `5381`).
2.  **FNV-1a**: Algoritmo de alta dispersão focado em processar strings byte a byte (`Fowler-Noll-Vo`).

### Dimensionamento Ótimo
O Filtro de Bloom calcula de forma matemática e automática o seu tamanho ideal em bits ($m$) e a quantidade ideal de funções hash ($k$) com base na estimativa de elementos esperados ($n = 100.000$) e na taxa tolerável de falsos positivos ($p = 1\%$), utilizando as seguintes fórmulas clássicas:

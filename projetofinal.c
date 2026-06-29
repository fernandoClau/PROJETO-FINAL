#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MAX_USER_LEN 20
#define HASH_TABLE_SIZE 142857 // Número primo para fator de carga ~0.7 com 100k elementos

// --- ESTRUTURAS DE DADOS ---

// Nodo para encadeamento externo na Tabela Hash
typedef struct NoHash {
    char usuario[MAX_USER_LEN];
    struct NoHash* proximo;
} NoHash;

// Estrutura da Tabela Hash
typedef struct {
    NoHash* tabela[HASH_TABLE_SIZE];
    int quantidade_elementos;
} TabelaHash;

// Estrutura do Filtro de Bloom
typedef struct {
    unsigned char* vetor_bits;
    int tamanho_bits;
    int quantidade_hashes;
} FiltroBloom;

// Estrutura para Controle de Estatísticas do Sistema
typedef struct {
    int total_elementos;
    int total_consultas;
    int consultas_evitadas;
    int falsos_positivos;
    double tempo_total_consultas;
} Estatisticas;

// Estatísticas Globais
Estatisticas stats = {0, 0, 0, 0, 0.0};

// --- FUNÇÕES HASH MANUAIS ---

// Função Hash 1: DJB2
unsigned long hash_djb2(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Função Hash 2: FNV-1a
unsigned long hash_fnv1a(const char* str) {
    unsigned long hash = 14695981039346656037ULL;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Gera várias posições do filtro Bloom usando duas funções hash
unsigned int get_bloom_hash(const char* str, int i, unsigned int max_size) {
    unsigned long h1 = hash_djb2(str);
    unsigned long h2 = hash_fnv1a(str);
    return (unsigned int)((h1 + i * h2) % max_size);
}

// --- IMPLEMENTAÇÃO: TABELA HASH ---

TabelaHash* criar_tabela_hash() {
    TabelaHash* ht = (TabelaHash*)malloc(sizeof(TabelaHash));
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        ht->tabela[i] = NULL;
    }
    ht->quantidade_elementos = 0;
    return ht;
}

void inserir_hash(TabelaHash* ht, const char* usuario) {
    unsigned int indice = hash_djb2(usuario) % HASH_TABLE_SIZE;

    // Verifica se o usuário já existe
    NoHash* atual = ht->tabela[indice];
    while (atual != NULL) {
        if (strcmp(atual->usuario, usuario) == 0) return;
        atual = atual->proximo;
    }

    NoHash* novo_no = (NoHash*)malloc(sizeof(NoHash));
    strcpy(novo_no->usuario, usuario);
    novo_no->proximo = ht->tabela[indice];
    ht->tabela[indice] = novo_no;
    ht->quantidade_elementos++;

    stats.total_elementos = ht->quantidade_elementos;
}

int buscar_hash(TabelaHash* ht, const char* usuario) {
    unsigned int indice = hash_djb2(usuario) % HASH_TABLE_SIZE;
    NoHash* atual = ht->tabela[indice];
    while (atual != NULL) {
        if (strcmp(atual->usuario, usuario) == 0) {
            return 1; // Encontrado
        }
        atual = atual->proximo;
    }
    return 0; // Não encontrado
}

// --- IMPLEMENTAÇÃO: FILTRO DE BLOOM ---

FiltroBloom* criar_filtro_bloom(int elementos_esperados, double taxa_falso_positivo) {
    FiltroBloom* bf = (FiltroBloom*)malloc(sizeof(FiltroBloom));

    // Fórmulas de dimensionamento ótimo
    bf->tamanho_bits = (int)(-(elementos_esperados * log(taxa_falso_positivo)) / (log(2) * log(2)));
    bf->quantidade_hashes = (int)((bf->tamanho_bits / (double)elementos_esperados) * log(2));

    if (bf->quantidade_hashes < 1) bf->quantidade_hashes = 1;

    int size_bytes = (bf->tamanho_bits / 8) + 1;
    bf->vetor_bits = (unsigned char*)calloc(size_bytes, sizeof(unsigned char));

    return bf;
}

void inserir_bloom(FiltroBloom* bf, const char* usuario) {
    for (int i = 0; i < bf->quantidade_hashes; i++) {
        unsigned int bit_pos = get_bloom_hash(usuario, i, bf->tamanho_bits);
        bf->vetor_bits[bit_pos / 8] |= (1 << (bit_pos % 8)); // Ativa o bit
    }
}

int consultar_bloom(FiltroBloom* bf, const char* usuario) {
    for (int i = 0; i < bf->quantidade_hashes; i++) {
        unsigned int bit_pos = get_bloom_hash(usuario, i, bf->tamanho_bits);
        if (!(bf->vetor_bits[bit_pos / 8] & (1 << (bit_pos % 8)))) {
            return 0; // Definitivamente não existe
        }
    }
    return 1; // Possivelmente existe
}

// --- UTILITÁRIOS E GERAÇÃO DE ARQUIVOS DE TESTE ---

void gerar_usuario_aleatorio(char* dest) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 8; i++) {
        dest[i] = chars[rand() % 26];
    }
    for (int i = 8; i < 11; i++) {
        dest[i] = '0' + (rand() % 10);
    }
    dest[11] = '\0';
}

void gerar_arquivo_usuarios(const char* nome_arquivo, int size) {
    FILE* file = fopen(nome_arquivo, "w");
    if (!file) return;
    char user[MAX_USER_LEN];
    for (int i = 0; i < size; i++) {
        gerar_usuario_aleatorio(user);
        fprintf(file, "%s\n", user);
    }
    fclose(file);
}

// --- FLUXO DE CONSULTA OBRIGATÓRIO ---

int consultar_usuario(FiltroBloom* bf, TabelaHash* ht, const char* usuario, int record_stats) {
    if (record_stats) stats.total_consultas++;

    clock_t start = clock();

    // Primeiro consulta o Bloom.
    int bloom_res = consultar_bloom(bf, usuario);

    // Se ele disser que não existe, nem procura na Hash.
    if (bloom_res == 0) {
        if (record_stats) {
            stats.consultas_evitadas++;
            clock_t end = clock();
            stats.tempo_total_consultas += (double)(end - start) / CLOCKS_PER_SEC;
        }
        return 0; // Inexistente
    }

    // Caso indique "possivelmente existe", consulta a Tabela Hash
    int hash_res = buscar_hash(ht, usuario);

    if (record_stats) {
        if (hash_res == 0) {
            stats.falsos_positivos++; // Indicou que podia existir, mas não estava na Hash
        }
        clock_t end = clock();
        stats.tempo_total_consultas += (double)(end - start) / CLOCKS_PER_SEC;
    }

    return hash_res;
}

// Inserção em lote
void carregar_lote(const char* nome_arquivo, FiltroBloom* bf, TabelaHash* ht) {
    FILE* file = fopen(nome_arquivo, "r");
    if (!file) {
        printf("Erro: Arquivo %s não encontrado.\n", nome_arquivo);
        return;
    }
    char user[MAX_USER_LEN];
    while (fscanf(file, "%s", user) != EOF) {
        inserir_hash(ht, user);
        inserir_bloom(bf, user);
    }
    fclose(file);
    printf("Lote carregado com sucesso do arquivo '%s'.\n", nome_arquivo);
}

// --- FUNÇÃO PARA EXECUÇÃO DE EXPERIMENTOS ---

void executar_experimentos() {
    int sizes[] = {1000, 10000, 100000};
    printf("\n=== EXECUTANDO EXPERIMENTOS (PARTE 3) ===\n");
    printf("%-10s | %-18s | %-18s | %-15s\n", "Quantidade", "Tempo sem Bloom(s)", "Tempo com Bloom(s)", "Falsos Positivos");
    printf("-------------------------------------------------------------------------\n");

    for (int s = 0; s < 3; s++) {
        int n = sizes[s];
        char nome_arquivo[30];
        sprintf(nome_arquivo, "usuarios_%d.txt", n);
        gerar_arquivo_usuarios(nome_arquivo, n);

        TabelaHash* ht_exp = criar_tabela_hash();
        FiltroBloom* bf_exp = criar_filtro_bloom(100000, 0.01);

        // Carrega os dados gerados
        FILE* f = fopen(nome_arquivo, "r");
        if (!f) continue;
        
        char** usuarios_carregados = malloc(n * sizeof(char*));
        for(int i=0; i<n; i++) usuarios_carregados[i] = malloc(MAX_USER_LEN);
        
        int indice = 0;
        while(indice < n && fscanf(f, "%s", usuarios_carregados[indice]) != EOF) {
            inserir_hash(ht_exp, usuarios_carregados[indice]);
            inserir_bloom(bf_exp, usuarios_carregados[indice]);
            indice++;
        }
        fclose(f);

        // Gerar consultas que não estão no banco para testar a eficiência do Bloom
        char** usuarios_teste = malloc(n * sizeof(char*));
        for(int i=0; i<n; i++) {
            usuarios_teste[i] = malloc(MAX_USER_LEN);
            gerar_usuario_aleatorio(usuarios_teste[i]);
        }

        // Medição Sem Bloom
        clock_t start_sb = clock();
        for(int i=0; i<n; i++) {
            buscar_hash(ht_exp, usuarios_teste[i]);
        }
        clock_t end_sb = clock();
        double time_without = (double)(end_sb - start_sb) / CLOCKS_PER_SEC;

        // Medição Com Bloom
        int fp_count = 0;
        clock_t start_cb = clock();
        for(int i=0; i<n; i++) {
            int b_res = consultar_bloom(bf_exp, usuarios_teste[i]);
            if (b_res == 1) {
                int h_res = buscar_hash(ht_exp, usuarios_teste[i]);
                if (h_res == 0) fp_count++;
            }
        }
        clock_t end_cb = clock();
        double time_with = (double)(end_cb - start_cb) / CLOCKS_PER_SEC;

        double fp_pct = ((double)fp_count / n) * 100.0;
        printf("%-10d | %-18.6f | %-18.6f | %.2f%%\n", n, time_without, time_with, fp_pct);

        // Limpeza CORRETA de memória para evitar o Segmentation Fault
        for(int i=0; i<n; i++) {
            free(usuarios_carregados[i]);
            free(usuarios_teste[i]);
        }
        free(usuarios_carregados);
        free(usuarios_teste);
        free(bf_exp->vetor_bits); 
        free(bf_exp);

        // Limpa os nós da tabela hash antes de liberá-la
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            NoHash* atual = ht_exp->tabela[i];
            while (atual != NULL) {
                NoHash* aux = atual;
                atual = atual->proximo;
                free(aux);
            }
        }
        free(ht_exp);
    }
}

// --- INTERFACE PRINCIPAL ---

int main() {
    srand(time(NULL));

    // Inicialização das estruturas principais do menu
    TabelaHash* main_ht = criar_tabela_hash();
    FiltroBloom* main_bf = criar_filtro_bloom(100000, 0.01); // Suporta até 100k usuários com taxa de 1%

    int opcao;
    int encontrado;
    char entrada[MAX_USER_LEN];

    do {
        printf("\n================ MENU PRINCIPAL ================\n");
        printf("1. RF01 - Inserir Novo Usuário\n");
        printf("2. RF02 - Consultar Usuário\n");
        printf("3. RF03 - Exibir Estatísticas do Sistema\n");
        printf("4. RF04 - Inserir em Lote (Arquivo txt)\n");
        printf("5. Executar Bateria de Experimentos (Parte 3)\n");
        printf("0. Sair\n");
        printf("Escolha uma opção: ");
        if(scanf("%d", &opcao) != 1) return 0;

        switch (opcao) {
            case 1:
                printf("Digite o identificador do usuário (ex: joao123): ");
                scanf("%s", entrada);
                inserir_hash(main_ht, entrada);
                inserir_bloom(main_bf, entrada);
                printf("Usuário '%s' cadastrado com sucesso!\n", entrada);
                break;
                
            case 2:
                printf("Digite o usuário para consulta: ");
                scanf("%s", entrada);
                encontrado = consultar_usuario(main_bf, main_ht, entrada, 1);
                if (encontrado == 1) {
                    printf("-> Usuário encontrado\n");
                } else {
                    printf("-> Usuário inexistente\n");
                }
                break;
                
            case 3:
                printf("\n--- ESTATÍSTICAS DO SISTEMA (RF03) ---\n");
                printf("Quantidade de elementos armazenados: %d\n", stats.total_elementos);
                printf("Quantidade de consultas realizadas: %d\n", stats.total_consultas);
                printf("Consultas evitadas pela Bloom: %d\n", stats.consultas_evitadas);
                printf("Número de falsos positivos detectados: %d\n", stats.falsos_positivos);
                double fp_rate = stats.total_consultas > 0 ? ((double)stats.falsos_positivos / stats.total_consultas) * 100 : 0.0;
                printf("Taxa percentual de falsos positivos: %.2f%%\n", fp_rate);
                double avg_time = stats.total_consultas > 0 ? (stats.tempo_total_consultas / stats.total_consultas) : 0.0;
                printf("Tempo médio de consulta: %.8f segundos\n", avg_time);
                break;
                
            case 4:
                printf("Digite o nome do arquivo (ex: usuarios.txt): ");
                scanf("%s", entrada);
                carregar_lote(entrada, main_bf, main_ht);
                break;
                
            case 5:
                executar_experimentos();
                break;
                
            case 0:
                printf("Encerrando programa...\n");
                break;
                
            default:
                printf("Opção inválida!\n");
        }
    } while (opcao != 0);

    // Liberação de memória do encerramento
    free(main_bf->vetor_bits);
    free(main_bf);
    free(main_ht);
    return 0;
}
#include <semaphore.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define VAGAS_TOTAL 100
#define VEICULOS_TOTAL 10000
#define CANCELAS_TOTAL 20

// Estado

typedef struct {
    int entrada;
    int saida;
} Estado;

const Estado estados[] = {
        {2, 2},
        {1, 1},
        {2, 1},
        {4, 1},
        {1, 5}
};

typedef enum {
    BALANCEADO_LENTO,
    BALANCEADO_RAPIDO,
    TRANQUILO,
    OCIOSO,
    CRITICO
} OpcaoEstado;

OpcaoEstado estado = CRITICO;

// Vagas

typedef struct vaga {
    bool ocupada;
    struct vaga *proxima;
} Vaga;
pthread_mutex_t vagasListaMutex;
sem_t *vagasSemaforo;
char *nomeSemaforo = "vagas";
int vagasTotal = VAGAS_TOTAL;

Vaga *vagas;

Vaga *primeiraVaga;

void criaVagas() {
    sem_unlink(nomeSemaforo);
    vagasSemaforo = sem_open(nomeSemaforo, O_CREAT, S_IRUSR | S_IWUSR, vagasTotal);

    pthread_mutex_init(&vagasListaMutex, NULL);

    vagas = (Vaga *) malloc(vagasTotal * sizeof(Vaga));

    primeiraVaga = &vagas[0];

    for (int i = 1; i < vagasTotal; i++) {
        vagas[i].ocupada = false;
        vagas[i - 1].proxima = &vagas[i];
    }
}

Vaga *ocupaVaga() {
    sem_wait(vagasSemaforo);

    pthread_mutex_lock(&vagasListaMutex);

    Vaga *vaga = primeiraVaga;
    primeiraVaga = primeiraVaga->proxima;
    vaga->proxima = NULL;
    vaga->ocupada = true;

    pthread_mutex_unlock(&vagasListaMutex);

    return vaga;
}

void liberaVaga(Vaga *vaga) {
    pthread_mutex_lock(&vagasListaMutex);

    if (primeiraVaga != NULL) {
        vaga->proxima = primeiraVaga;
    }

    primeiraVaga = vaga;
    vaga->ocupada = false;

    pthread_mutex_unlock(&vagasListaMutex);

    sem_post(vagasSemaforo);
}

void destroiVagas() {
    sem_close(vagasSemaforo);
    sem_unlink(nomeSemaforo);

    pthread_mutex_destroy(&vagasListaMutex);

    free(vagas);
}

// Veiculos

typedef struct veiculo {
    int tempoEspera;
    pthread_t thread;
    time_t chegada;
    time_t entrada;
    struct vaga *vaga;
    struct veiculo *proximo;
} Veiculo;
pthread_mutex_t veiculosListaMutex;
int veiculosTotal = VEICULOS_TOTAL;
Veiculo *veiculos;

int indiceProximoVeiculo = 0;

int tempoAleatorio(int max) {
    return rand() % (max + 1);
}

void criaVeiculos() {
    pthread_mutex_init(&veiculosListaMutex, NULL);

    veiculos = (Veiculo *) malloc(veiculosTotal * sizeof(Veiculo));

    for (int i = 0; i < veiculosTotal; i++) {
        veiculos[i].tempoEspera = tempoAleatorio(estados[estado].saida);
    }
}

Veiculo *proximoVeiculo() {
    pthread_mutex_lock(&veiculosListaMutex);

    if (indiceProximoVeiculo >= veiculosTotal) {
        pthread_mutex_unlock(&veiculosListaMutex);
        return NULL;
    }

    Veiculo *veiculo = &veiculos[indiceProximoVeiculo++];

    pthread_mutex_unlock(&veiculosListaMutex);

    return veiculo;
}

void *aguarda(void *arg) {
    Veiculo *veiculo = (Veiculo *) arg;
    sleep(veiculo->tempoEspera);
    liberaVaga(veiculo->vaga);
    pthread_exit(0);
}

void destroiVeiculos() {
    pthread_mutex_destroy(&veiculosListaMutex);

    for (int i = 0; i < veiculosTotal; i++) {
        pthread_join(veiculos[i].thread, NULL);
    }

    free(veiculos);
}

// Cancela

typedef struct cancela{
    pthread_t thread;
    pthread_t adicionaThread;
    pthread_t removeThread;
    pthread_mutex_t filaVeiculosMutex;
    struct veiculo *primeiroVeiculo;
    struct veiculo *ultimoVeiculo;
    int veiculosAtendidos;
    int tamanhoFila;
} Cancela;

int cancelasTotal = CANCELAS_TOTAL;
Cancela *cancelas;

void *adicionaVeiculo(void *arg) {
    Cancela *cancela = (Cancela *) arg;

    Veiculo *veiculo;

    while ((veiculo = proximoVeiculo()) != NULL) {
        pthread_mutex_lock(&cancela->filaVeiculosMutex);

        if (cancela->primeiroVeiculo == NULL) {
            cancela->primeiroVeiculo = veiculo;
        }

        if (cancela->ultimoVeiculo != NULL) {
            cancela->ultimoVeiculo->proximo = veiculo;
        }
        cancela->ultimoVeiculo = veiculo;

        cancela->veiculosAtendidos++;
        cancela->tamanhoFila++;

        pthread_mutex_unlock(&cancela->filaVeiculosMutex);

        time(&veiculo->chegada);

        sleep(tempoAleatorio(estados[estado].entrada));
    }

    pthread_exit(0);
}

void *removeVeiculo(void *arg) {
    Cancela *cancela = (Cancela *) arg;

    while (true) {
        pthread_mutex_lock(&cancela->filaVeiculosMutex);
        pthread_mutex_lock(&veiculosListaMutex);

        if (cancela->ultimoVeiculo == NULL && indiceProximoVeiculo >= veiculosTotal) {
            pthread_mutex_unlock(&veiculosListaMutex);
            pthread_mutex_unlock(&cancela->filaVeiculosMutex);

            break;
        }

        pthread_mutex_unlock(&veiculosListaMutex);

        Veiculo *veiculo = cancela->primeiroVeiculo;

        if (veiculo == NULL) {
            pthread_mutex_unlock(&cancela->filaVeiculosMutex);
            continue;
        }

        cancela->primeiroVeiculo = veiculo->proximo;
        veiculo->proximo = NULL;

        if (veiculo == cancela->ultimoVeiculo) {
            cancela->ultimoVeiculo = NULL;
        }

        cancela->tamanhoFila--;

        pthread_mutex_unlock(&cancela->filaVeiculosMutex);

        veiculo->vaga = ocupaVaga();
        time(&veiculo->entrada);

        pthread_create(&veiculo->thread, NULL, aguarda, veiculo);
    }

    pthread_exit(0);
}

void *gerenciaCancela(void *arg) {
    Cancela *cancela = (Cancela *) arg;

    pthread_create(&cancela->adicionaThread, NULL, adicionaVeiculo, cancela);
    pthread_create(&cancela->removeThread, NULL, removeVeiculo, cancela);

    pthread_join(cancela->adicionaThread, NULL);
    pthread_join(cancela->removeThread, NULL);

    pthread_exit(0);
}

void criaCancelas() {
    cancelas = (Cancela *) malloc(cancelasTotal * sizeof(Cancela));

    for (int i = 0; i < cancelasTotal; i++) {
        pthread_mutex_init(&cancelas[i].filaVeiculosMutex, NULL);
        pthread_create(&cancelas[i].thread, NULL, gerenciaCancela, &cancelas[i]);
        cancelas[i].veiculosAtendidos = 0;
        cancelas[i].tamanhoFila = 0;
    }
}

void destroiCancelas() {
    for (int i = 0; i < cancelasTotal; i++) {
        pthread_join(cancelas[i].thread, NULL);
        pthread_mutex_destroy(&cancelas[i].filaVeiculosMutex);
    }

    free(cancelas);
}

// Exibição

bool executando = true;
void *mostraVagas() {
    int maxJ = (int) sqrt(vagasTotal);
    int maxI = vagasTotal / maxJ;

    while (executando) {
        printf("\033[2J\033[H");
        for (int i = 0; i < maxI; i++) {//FIXME número de vagas
            for (int j = 0; j < maxJ; j++) {
                printf("%s", vagas[i * maxI + j].ocupada ? "O" : "_");
            }
            printf("\n");
        }

        printf("\nCancela\t\tAtendidos\tNa fila\n");

        for (int i = 0; i < cancelasTotal; i++) {
            printf("%d\t\t%d\t\t%d\n", i, cancelas[i].veiculosAtendidos, cancelas[i].tamanhoFila);
        }

        printf("\nVeículos restantes: %d\n", veiculosTotal - indiceProximoVeiculo);
        usleep(100000);
    }

    pthread_exit(0);
}

void argumentos(int argc, char *argv[]) {
    // Parse the command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            vagasTotal = atoi(argv[i + 1]);
            vagasTotal = vagasTotal < VAGAS_TOTAL ? vagasTotal : VAGAS_TOTAL;
            i++; // Skip the value that was just processed
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            veiculosTotal = atoi(argv[i + 1]);
            veiculosTotal = veiculosTotal < VEICULOS_TOTAL ? veiculosTotal : VEICULOS_TOTAL;
            i++;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cancelasTotal = atoi(argv[i + 1]);
            cancelasTotal = cancelasTotal < CANCELAS_TOTAL ? cancelasTotal : CANCELAS_TOTAL;
            i++;
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "lento") == 0) {
                estado = BALANCEADO_LENTO;
            } else if (strcmp(argv[i + 1], "rapido") == 0) {
                estado = BALANCEADO_RAPIDO;
            } else if (strcmp(argv[i + 1], "tranquilo") == 0) {
                estado = TRANQUILO;
            } else if (strcmp(argv[i + 1], "ocioso") == 0) {
                estado = OCIOSO;
            } else if (strcmp(argv[i + 1], "critico") == 0) {
                estado = CRITICO;
            }
            i++;
        }
    }
}

int main(int argc, char *argv[]) {
    argumentos(argc, argv);

    sranddev();

    criaVagas();
    criaVeiculos();
    criaCancelas();

    pthread_t exibeVagas;
    pthread_create(&exibeVagas, NULL, mostraVagas, NULL);

    destroiCancelas();
    destroiVeiculos();
    destroiVagas();

    executando = false;

    pthread_join(exibeVagas, NULL);

    return 0;
}

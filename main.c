#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#define VAGAS_TOTAL 100
#define VEICULOS_TOTAL 10000
#define CANCELAS_TOTAL 20

// Estado
typedef struct {
    int entrada;
    int saida;
} Estado;

const Estado BALANCEADO_LENTO = {2, 2};
const Estado BALANCEADO_RAPIDO = {1, 1};
const Estado TRANQUILO = {2, 1};
const Estado OCIOSO = {4, 1};
const Estado CRITICO = {1, 5};

Estado estado;

// Vagas
typedef struct vaga {
    bool ocupada;
    struct vaga *proxima;
} Vaga;
pthread_mutex_t vagasListaMutex;
sem_t *vagasSemaforo;
char *nomeSemaforo = "vagas";

Vaga vagas[VAGAS_TOTAL];

Vaga *primeiraVaga;

void criaVagas() {
    sem_unlink(nomeSemaforo);
    vagasSemaforo = sem_open(nomeSemaforo, O_CREAT, S_IRUSR | S_IWUSR, VAGAS_TOTAL);

    pthread_mutex_init(&vagasListaMutex, NULL);

    primeiraVaga = &vagas[0];

    for (int i = 1; i < VAGAS_TOTAL; i++) {
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
Veiculo veiculos[VEICULOS_TOTAL];

int indiceProximoVeiculo = 0;

int tempoAleatorio(int max) {
    return rand() % (max + 1);
}

void criaVeiculos() {
    pthread_mutex_init(&veiculosListaMutex, NULL);

    for (int i = 0; i < VEICULOS_TOTAL; i++) {
        veiculos[i].tempoEspera = tempoAleatorio(estado.saida);
    }
}

Veiculo *proximoVeiculo() {
    pthread_mutex_lock(&veiculosListaMutex);

    if (indiceProximoVeiculo >= VEICULOS_TOTAL) {
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

    for (int i = 0; i < VEICULOS_TOTAL; i++) {
        pthread_join(veiculos[i].thread, NULL);
    }
}

// Cancela

typedef struct cancela{
    pthread_t thread;
    pthread_t adicionaThread;
    pthread_t removeThread;
    pthread_mutex_t filaVeiculosMutex;
    struct veiculo *primeiroVeiculo;
    struct veiculo *ultimoVeiculo;
} Cancela;

Cancela cancelas[CANCELAS_TOTAL];

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

        pthread_mutex_unlock(&cancela->filaVeiculosMutex);

        time(&veiculo->chegada);

        sleep(tempoAleatorio(estado.entrada));
    }

    pthread_exit(0);
}

void *removeVeiculo(void *arg) {
    Cancela *cancela = (Cancela *) arg;

    while (true) {
        pthread_mutex_lock(&cancela->filaVeiculosMutex);
        pthread_mutex_lock(&veiculosListaMutex);

        if (cancela->ultimoVeiculo == NULL && indiceProximoVeiculo >= VEICULOS_TOTAL) {
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
    for (int i = 0; i < CANCELAS_TOTAL; i++) {
        pthread_mutex_init(&cancelas[i].filaVeiculosMutex, NULL);
        pthread_create(&cancelas[i].thread, NULL, gerenciaCancela, &cancelas[i]);
    }
}

void destroiCancelas() {
    for (int i = 0; i < CANCELAS_TOTAL; i++) {
        pthread_join(cancelas[i].thread, NULL);
        pthread_mutex_destroy(&cancelas[i].filaVeiculosMutex);
    }
}

// Exibição

bool executando = true;
void *mostraVagas() {
    while (executando) {
        printf("\033[2J\033[H");
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                printf("%s", vagas[i * 10 + j].ocupada ? "O" : "_");
            }
            printf("\n");
        }
        printf("%d\n", indiceProximoVeiculo);
        usleep(100000);
    }

    pthread_exit(0);
}

int main(void) {
    estado = CRITICO;

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

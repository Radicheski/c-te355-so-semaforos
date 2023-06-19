#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define TOTAL_ITERACOES 10000
#define TOTAL_VAGAS 100

int tempoEspera(int max) {
    return rand() % (max + 1);
}

// Vagas

sem_t *vagaSemaforo;

typedef struct vaga {
    bool ocupada;
    struct vaga *proxima;
} Vaga;

Vaga vagas[TOTAL_VAGAS];
Vaga *proximaVaga = NULL;

void criaVagas() {
    vagaSemaforo = sem_open("vagas", O_CREAT, S_IRUSR | S_IWUSR, 100);

    proximaVaga = &vagas[0];

    for (int i = 0; i < TOTAL_VAGAS; i++) {
        vagas[i].ocupada = false;
        vagas[i].proxima = i + 1 < TOTAL_VAGAS ? &vagas[i + 1] : NULL;
    }
}

void destroiVagas() {
    sem_close(vagaSemaforo);
    sem_unlink("vagas");
}

Vaga *ocupaVaga() {
    sem_wait(vagaSemaforo);

    if (proximaVaga == NULL) {
        sem_post(vagaSemaforo);
        return NULL;
    }

    Vaga *vaga = proximaVaga;
    proximaVaga = proximaVaga->proxima;
    vaga->proxima = NULL;

    vaga->ocupada = true;

    sem_post(vagaSemaforo);
    return vaga;
}

void liberaVaga(Vaga *vaga) {
    sem_wait(vagaSemaforo);

    vaga->proxima = proximaVaga;
    proximaVaga = vaga;

    vaga->ocupada = false;

    sem_post(vagaSemaforo);
}

void mostraVagas() {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            printf("%s", vagas[10 * i + j].ocupada ? " " : "X");
        }
        printf("\n");
    }
}

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

// Veículos

typedef struct veiculo {
    Vaga *vaga;
    time_t chegada;
    int tempoPermanencia;
    struct veiculo *proximo;
    pthread_t thread;
} Veiculo;

Veiculo veiculos[TOTAL_ITERACOES];
pthread_mutex_t veiculosMutex;
int proximoVeiculo = 0;

void criaVeiculos() {
    pthread_mutex_init(&veiculosMutex, NULL);

    for (int i = 0; i < TOTAL_ITERACOES; i++) {
        veiculos[i].tempoPermanencia = tempoEspera(estado.saida);
    }
}

void destroiVeiculos() {
    pthread_mutex_destroy(&veiculosMutex);
}

Veiculo *chegada() {
    pthread_mutex_lock(&veiculosMutex);

    if (proximoVeiculo >= TOTAL_ITERACOES) {
        return NULL;
    }
    Veiculo *v = &veiculos[proximoVeiculo++];

    pthread_mutex_unlock(&veiculosMutex);

    v->tempoPermanencia = tempoEspera(estado.saida);
    v->proximo = NULL;
    v->chegada = time(NULL);

    return v;
}

pthread_t cancelaEntrada;

typedef struct {
    pthread_mutex_t *filaMutex;
    Veiculo *filaInicio;
    Veiculo *filaFim;
} Cancela;

void *adicionaVeiculo(void *arg) {
    Cancela *cancela = (Cancela *) arg;

    Veiculo *veiculo;

    while ((veiculo = chegada()) != NULL) {
        pthread_mutex_lock(cancela->filaMutex);

        if (cancela->filaInicio == NULL) {
            cancela->filaInicio = veiculo;
        }

        if (cancela->filaFim != NULL) {
            cancela->filaFim->proximo = veiculo;
        }
        cancela->filaFim = veiculo;

        pthread_mutex_unlock(cancela->filaMutex);

        sleep(tempoEspera(estado.entrada));
    }

    return NULL;
}

void *espera(void *arg) {
    Veiculo *veiculo = (Veiculo *) arg;

    sleep(veiculo->tempoPermanencia);
    liberaVaga(veiculo->vaga);
    veiculo->vaga = NULL;

    return NULL;
}

void *removeVeiculo(void *arg) {
    Cancela *cancela = (Cancela *) arg;

    while (cancela->filaFim != NULL || proximoVeiculo < TOTAL_ITERACOES) {
        pthread_mutex_lock(cancela->filaMutex);

        if (cancela->filaInicio != NULL) {
            Veiculo *veiculo = cancela->filaInicio;

            if (veiculo == cancela->filaFim) {
                cancela->filaFim = NULL;
            }

            cancela->filaInicio = cancela->filaInicio->proximo;
            veiculo->proximo = NULL;

            Vaga *vaga = ocupaVaga();
            veiculo->vaga = vaga;

            pthread_create(&veiculo->thread, NULL, espera, veiculo);

            printf("%d\n", proximoVeiculo);
        }

        pthread_mutex_unlock(cancela->filaMutex);
    }

    return NULL;
}

void *cancela() {
    pthread_mutex_t filaMutex;
    pthread_mutex_init(&filaMutex, NULL);

    Cancela cancela = {&filaMutex, NULL, NULL};

    pthread_t fila;
    pthread_t estaciona;

    pthread_create(&fila, NULL, adicionaVeiculo, &cancela);
    pthread_create(&estaciona, NULL, removeVeiculo, &cancela);

    pthread_join(fila, NULL);
    pthread_join(estaciona, NULL);

    return NULL;
}

int main(void) {
    criaVagas();
    criaVeiculos();

    estado = CRITICO;

    pthread_create(&cancelaEntrada, NULL, cancela, NULL);
    pthread_join(cancelaEntrada, NULL);

    for (int i = 0; i < TOTAL_ITERACOES; i++) {
        pthread_join(veiculos[i].thread, NULL);
    }

    destroiVagas();
    destroiVeiculos();

    return 0;
}

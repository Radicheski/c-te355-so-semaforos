#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define TOTAL_ITERACOES 10000

int tempoEspera(int max) {
    return rand() % (max + 1);
}

// Vagas

sem_t *vagaSemaforo;

typedef struct vaga {
    int numero;
    struct vaga *proxima;
} Vaga;

Vaga *proximaVaga = NULL;

int vagas[10][10];

void criaVagas() {
    vagaSemaforo = sem_open("vagas", O_CREAT, S_IRUSR | S_IWUSR, 100);

    for (int i = 99; i >= 0; i--) {
        Vaga *v = (Vaga *) malloc(sizeof(Vaga));
        v->numero = i;
        v->proxima = proximaVaga;
        proximaVaga = v;
    }
}

void destroiVagas() {
    Vaga *vaga = proximaVaga;
    while (vaga != NULL) {
        Vaga *temp = vaga;
        vaga = vaga->proxima;
        free(temp);
    }

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
    proximaVaga = vaga->proxima;
    vaga->proxima = NULL;

    int i = vaga->numero / 10;
    int j = vaga->numero % 10;
    vagas[i][j] = 1;

    sem_post(vagaSemaforo);
    return vaga;
}

void liberaVaga(Vaga *vaga) {
    sem_wait(vagaSemaforo);

    vaga->proxima = proximaVaga;
    proximaVaga = vaga;

    int i = vaga->numero / 10;
    int j = vaga->numero % 10;
    vagas[i][j] = 0;

    sem_post(vagaSemaforo);
}

void mostraVagas() {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            printf("%s", vagas[i][j] == 0 ? " " : "X");
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

            printf("%d\t%d\n", proximoVeiculo, vaga->numero);
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

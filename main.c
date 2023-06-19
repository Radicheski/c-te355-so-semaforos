#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

sem_t *vagaSemaforo;

int vagas[10][10];

void mostraVagas() {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            printf("%s", vagas[i][j] == 0 ? " " : "X");
        }
        printf("\n");
    }
}

int tempoEspera(int max) {
    return rand() % (max + 1);
}

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

typedef struct vaga {
    int numero;
    struct vaga *proxima;
} Vaga;

Vaga *proximaVaga = NULL;

void criaVagas() {
    for (int i = 99; i >= 0; i--) {
        Vaga *v = (Vaga *) malloc(sizeof(Vaga));
        v->numero = i;
        v->proxima = proximaVaga;
        proximaVaga = v;
    }
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
    vaga->proxima = proximaVaga;
    proximaVaga = vaga;

    int i = vaga->numero / 10;
    int j = vaga->numero % 10;
    vagas[i][j] = 0;
}

void destroiVagas() {
    Vaga *vaga = proximaVaga;
    while (vaga != NULL) {
        Vaga *temp = vaga;
        vaga = vaga->proxima;
        free(temp);
    }
}

typedef struct veiculo {
    Vaga *vaga;
    time_t chegada;
    int tempoPermanencia;
    struct veiculo *proximo;
} Veiculo;

pthread_t cancelaEntrada;
pthread_mutex_t iteracoesMutex;
int iteracoes = 10000;

Veiculo *chegada() {
    pthread_mutex_lock(&iteracoesMutex);

    if (iteracoes-- == 0) {
        return NULL;
    }

    pthread_mutex_unlock(&iteracoesMutex);

    Veiculo *v = (Veiculo *) malloc(sizeof(Veiculo));
    v->tempoPermanencia = tempoEspera(estado.saida);
    v->proximo = NULL;
    v->chegada = time(NULL);

    return v;
}

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

void *removeVeiculo(void *arg) {
    Cancela *cancela = (Cancela *) arg;

    while (cancela->filaFim != NULL || iteracoes >= 0) {
        pthread_mutex_lock(cancela->filaMutex);

        if (cancela->filaInicio != NULL) {
            Veiculo *veiculo = cancela->filaInicio;

            if (veiculo == cancela->filaFim) {
                cancela->filaFim = NULL;
            }

            veiculo->proximo = NULL;
            cancela->filaInicio = cancela->filaInicio->proximo;

            Vaga *vaga = ocupaVaga();

            printf("%d\t%d\n", iteracoes, vaga->numero);
            veiculo->vaga = vaga;
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

    estado = CRITICO;

    if (pthread_mutex_init(&iteracoesMutex, NULL) != 0) {
        return 1;
    }

    vagaSemaforo = sem_open("vagas", O_CREAT, S_IRUSR | S_IWUSR, 100);

    pthread_create(&cancelaEntrada, NULL, cancela, NULL);
    pthread_join(cancelaEntrada, NULL);

    destroiVagas();

    sem_close(vagaSemaforo);
    sem_unlink("vagas");

    pthread_mutex_destroy(&iteracoesMutex);

    return 0;
}

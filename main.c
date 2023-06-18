#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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

Vaga* ocupaVaga() {
  if (proximaVaga == NULL) {
    return NULL;
  }

  Vaga *vaga = proximaVaga;
  proximaVaga = vaga->proxima;
  vaga->proxima = NULL;

  int i = vaga->numero / 10;
  int j = vaga->numero % 10;
  vagas[i][j] = 1;

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
  Veiculo *v = (Veiculo *) malloc(sizeof(Veiculo));
  v->tempoPermanencia = tempoEspera(estado.saida);
  v->chegada = time(NULL);
  return v;
}

void *cancela() {
  Veiculo *filaVeiculos;

  while (iteracoes > 0) {
    int tempo = tempoEspera(estado.entrada);

    Veiculo *v = chegada();
    v->proximo = filaVeiculos;

    printf("%ld\n", v->chegada);

    filaVeiculos = v;

    printf("%d\t%d\n", iteracoes, tempo);
    sleep(tempo);

    pthread_mutex_lock(&iteracoesMutex);
    iteracoes--;
    pthread_mutex_unlock(&iteracoesMutex);
  }

  return NULL;
}

int main(void) {
  criaVagas();

  estado = CRITICO;

  if (pthread_mutex_init(&iteracoesMutex, NULL) != 0) {
    return 1;
  }

  pthread_create(&cancelaEntrada, NULL, cancela, NULL);
  pthread_join(cancelaEntrada, NULL);

  destroiVagas();

  pthread_mutex_destroy(&iteracoesMutex);

  return 0;
}


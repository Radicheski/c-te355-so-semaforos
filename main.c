#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

pthread_t cancelaEntrada;

void *cancela() {

  for (int i = 0; i < 10000; i++) {
    printf("%d\t%d\n", i, tempoEspera(estado.entrada));
  }

  return NULL;
}

int main(void) {
  criaVagas();

  estado = CRITICO;

  pthread_create(&cancelaEntrada, NULL, cancela, NULL);
  pthread_join(cancelaEntrada, NULL);

  destroiVagas();

  return 0;
}


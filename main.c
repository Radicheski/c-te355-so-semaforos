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
  return rand() % max;
}

int main(void) {
  srand(time(NULL));
  for (int i = 0; i < 5; i++) {
    printf("%d\n", tempoEspera(5));
  }
  return 0;
}


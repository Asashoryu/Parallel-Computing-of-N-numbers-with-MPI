#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"

int main(int argc, char *argv[]) {
    int menum, nproc;
    int n, nloc, tag, i, sum, sum_parz, sum_tmp;
    int *x, *xloc, tmp;
    int strategy;
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &menum);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    if (menum == 0) {
        // Controllo se ci sono abbastanza argomenti
        if (argc != 3) {
            fprintf(stderr, "Usage: %s n strategy\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
        
        // Converti gli argomenti in interi
        n = atoi(argv[1]);
        strategy = atoi(argv[2]);

        // Verifica e inizializza le variabili in base alla strategia
        if (strategy == 1) {
            // Inizializza variabili per la strategia 1
        } else if (strategy == 2) {
            // Inizializza variabili per la strategia 2
        } else if (strategy == 3) {
            // Inizializza variabili per la strategia 3
        } else {
            fprintf(stderr, "Invalid strategy: %d\n", strategy);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        x = (int *)malloc(n * sizeof(int));
        // Inizializzazione di x con "i"
        for (i = 0; i < n; i++) {
            x[i] = 1;
        }
    }

    // Distribuzione di n a tutti i processi
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&strategy, 1, MPI_INT, 0, MPI_COMM_WORLD);

    nloc = n / nproc;
    int rest = n % nproc;
    if (menum < rest) {
        nloc = nloc + 1;
    }
    xloc = (int *)malloc(nloc * sizeof(int));

    if (menum == 0) {
        xloc = x;
        tmp = nloc;
        int start = 0;
        for (i = 1; i < nproc; i++) {
            start = start + tmp;
            tag = 22 + i;
            if (i == rest) {
                tmp = nloc - 1;  // Modifica tmp se i == rest
            }
            MPI_Send(&x[start], tmp, MPI_INT, i, tag, MPI_COMM_WORLD);
        }
    } else {
        tag = 22 + menum;
        MPI_Recv(xloc, nloc, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
    }

    sum = 0;

    // Scegli la strategia in base a n e p
    if (strategy == 1) {
        // Utilizza la strategia 1
        if (menum == 0) {
            for (i = 0; i < nloc; i++) {
                sum += x[i];
            }
            for (i = 1; i < nproc; i++) {
                tag = 80 + i;
                MPI_Recv(&sum_parz, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &status);
                sum += sum_parz;
            }
            printf("La somma totale è %d\n", sum);
        } else {
            for (i = 0; i < nloc; i++) {
                sum += xloc[i];
            }
            tag = menum + 80;
            MPI_Send(&sum, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
        }
    } else if (strategy == 2) {
        // Utilizza la strategia 2
        for (i = 0; i < nloc; i++) {
            sum += xloc[i];
        }

        int log2nproc = (int)(log(nproc) / log(2)); // Calcolo del logaritmo in base 2
        for (i = 0; i < log2nproc; i++) {
            if ((menum % (1 << i)) == 0) {
                if ((menum % (1 << (i + 1)) == 0)) {
                    // Chi partecipa alla comunicazione
                    int partner = menum + (1 << i);
                    if (partner < nproc) {
                        int recv_tag = 300 + partner;
                        MPI_Recv(&sum_parz, 1, MPI_INT, partner, recv_tag, MPI_COMM_WORLD, &status);
                        sum += sum_parz;
                    }
                } else {
                    // Chi riceve
                    int partner = menum - (1 << i);
                    int send_tag = 300 + menum;
                    MPI_Send(&sum, 1, MPI_INT, partner, send_tag, MPI_COMM_WORLD);
                }
            }
        }
        // Al termine dell'elaborazione, puoi stampare il risultato
        printf("\nSono il processo %d: Somma totale=%d\n", menum, sum);   
    } else if(strategy == 3) {
        // Utilizza la strategia 3
        for (i = 0; i < nloc; i++) {
            sum += xloc[i];
        }

        int log2nproc = (int)(log(nproc) / log(2)); // Calcolo del logaritmo in base 2
        for (i = 0; i < log2nproc; i++) {
            if ((menum % (1 << i + 1)) < (1 << i)) {
                int partner = menum + (1 << i);
                // Spedisci a menum + 2^i
                int send_tag = 300 + menum;
                MPI_Send(&sum, 1, MPI_INT, partner, send_tag, MPI_COMM_WORLD);
                // Ricevi da menum + 2^i
                int recv_tag = 300 + partner;
                MPI_Recv(&sum_parz, 1, MPI_INT, partner, recv_tag, MPI_COMM_WORLD, &status);
                sum += sum_parz;
            } else {
                int partner = menum - (1 << i);
                sum_tmp = sum;
                // Ricevi da menum - 2^i
                int recv_tag = 300 + partner;
                MPI_Recv(&sum_parz, 1, MPI_INT, partner, recv_tag, MPI_COMM_WORLD, &status);
                sum += sum_parz;
                // Spedisci a menum - 2^i
                int send_tag = 300 + menum;
                MPI_Send(&sum_tmp, 1, MPI_INT, partner, send_tag, MPI_COMM_WORLD);
            }
        }
        // Al termine dell'elaborazione, puoi stampare il risultato
        printf("\nSono il processo %d: Somma totale=%d\n", menum, sum);
    }
 
    MPI_Finalize();
    return 0;
}

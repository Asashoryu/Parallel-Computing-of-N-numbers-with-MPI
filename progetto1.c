#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"

// Definizione dei tipi personalizzati tramite macro, fare attenzione a sostituire entrambi con tipi sensati come:
// - Per il tipo short:  #define MIO_TIPO short e  #define MIO_TIPO_MPI MPI_SHORT
// - Per il tipo int:    #define MIO_TIPO int e    #define MIO_TIPO_MPI MPI_INT
// - Per il tipo float:  #define MIO_TIPO float e  #define MIO_TIPO_MPI MPI_FLOAT
// - Per il tipo double: #define MIO_TIPO double e #define MIO_TIPO_MPI MPI_DOUBLE
#define MIO_TIPO int
#define MIO_TIPO_MPI MPI_INT

int main(int argc, char *argv[]) {
    int menum, nproc, strategy;
    int n, nloc, tag, i;
    MPI_Status status;
    // Dichiarazione delle variabili con tipi personalizzati
    MIO_TIPO *x, *xloc, tmp, sum, sum_parz, sum_tmp;  
    // Dichiarazione dei tipi per le strategie 2 e 3
    int log2nproc, partner, send_tag, recv_tag;

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

        if (strategy != 1 && strategy != 2 && strategy != 3) {
            fprintf(stderr, "Invalid strategy: %d\n", strategy);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        // Dichiarazione e inizializzazione di x in base al tipo personalizzato
        x = (MIO_TIPO *)malloc(n * sizeof(MIO_TIPO));
        for (i = 0; i < n; i++) {
            x[i] = (MIO_TIPO)1.0;  // Inizializza con valori float (cambia se necessario)
        }

    }

    if (strategy == 2 || strategy == 3) {
        log2nproc = (int)(log(nproc) / log(2));
    }

    // Distribuzione di n e strategy a tutti i processi
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);  // Tipo MPI int
    MPI_Bcast(&strategy, 1, MPI_INT, 0, MPI_COMM_WORLD);

    nloc = n / nproc;
    int rest = n % nproc;
    if (menum < rest) {
        nloc = nloc + 1;
    }
    xloc = (MIO_TIPO *)malloc(nloc * sizeof(MIO_TIPO));

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
            MPI_Send(&x[start], tmp, MIO_TIPO_MPI, i, tag, MPI_COMM_WORLD);  // Utilizzo del tipo personalizzato MPI
        }
    } else {
        tag = 22 + menum;
        MPI_Recv(xloc, nloc, MIO_TIPO_MPI, 0, tag, MPI_COMM_WORLD, &status);  // Utilizzo del tipo personalizzato MPI
    }

    sum = 0;

    // Scegli la strategia in base a n e strategy
    if (strategy == 1) {
        // Utilizza la strategia 1
        if (menum == 0) {
            for (i = 0; i < nloc; i++) {
                sum += x[i];
            }
            for (i = 1; i < nproc; i++) {
                tag = 80 + i;
                MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, i, tag, MPI_COMM_WORLD, &status);  // Utilizzo del tipo personalizzato MPI
                sum += sum_parz;
            }
            // printf("La somma totale è %f\n", sum);  // Utilizzo del tipo personalizzato per stampare
        } else {
            for (i = 0; i < nloc; i++) {
                sum += xloc[i];
            }
            tag = menum + 80;
            MPI_Send(&sum, 1, MIO_TIPO_MPI, 0, tag, MPI_COMM_WORLD);  // Utilizzo del tipo personalizzato MPI
        }
    } else if (strategy == 2) {
        // Utilizza la strategia 2
        for (i = 0; i < nloc; i++) {
            sum += xloc[i];
        }

        for (i = 0; i < log2nproc; i++) {
            if ((menum % (1 << i)) == 0) {
                if ((menum % (1 << (i + 1)) == 0)) {
                    // Chi riceve
                    partner = menum + (1 << i);
                    if (partner < nproc) {
                        recv_tag = 300 + partner;
                        MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, partner, recv_tag, MPI_COMM_WORLD, &status);  // Utilizzo del tipo personalizzato MPI
                        sum += sum_parz;
                    }
                } else {
                    // Chi spedisce
                    partner = menum - (1 << i);
                    send_tag = 300 + menum;
                    MPI_Send(&sum, 1, MIO_TIPO_MPI, partner, send_tag, MPI_COMM_WORLD);  // Utilizzo del tipo personalizzato MPI
                }
            }
        }
        // Al termine dell'elaborazione, puoi stampare il risultato
        // printf("\nSono il processo %d: Somma totale=%f\n", menum, sum);  // Utilizzo del tipo personalizzato per stampare
    } else if(strategy == 3) {
        // Utilizza la strategia 3
        for (i = 0; i < nloc; i++) {
            sum += xloc[i];
        }

        for (i = 0; i < log2nproc; i++) {
            if ((menum % (1 << i + 1)) < (1 << i)) {
                partner = menum + (1 << i);
                // Spedisci a menum + 2^i
                send_tag = 300 + menum;
                MPI_Send(&sum, 1, MIO_TIPO_MPI, partner, send_tag, MPI_COMM_WORLD);  // Utilizzo del tipo personalizzato MPI
                // Ricevi da menum + 2^i
                recv_tag = 300 + partner;
                MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, partner, recv_tag, MPI_COMM_WORLD, &status);  // Utilizzo del tipo personalizzato MPI
                sum += sum_parz;
            } else {
                partner = menum - (1 << i);
                sum_tmp = sum;
                // Ricevi da menum - 2^i
                recv_tag = 300 + partner;
                MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, partner, recv_tag, MPI_COMM_WORLD, &status);  // Utilizzo del tipo personalizzato MPI
                sum += sum_parz;
                // Spedisci a menum - 2^i
                send_tag = 300 + menum;
                MPI_Send(&sum_tmp, 1, MIO_TIPO_MPI, partner, send_tag, MPI_COMM_WORLD);  // Utilizzo del tipo personalizzato MPI
            }
        }
        // Al termine dell'elaborazione, puoi stampare il risultato
        // printf("\nSono il processo %d: Somma totale=%f\n", menum, sum);  // Utilizzo del tipo personalizzato per stampare
    }

    // il processo p0 stampa il suo risultato:
    if (menum == 0) {
        printf("\nSono il processo %d: Somma totale=%lf\n", menum, sum);  // Utilizzo del tipo personalizzato per stampare
    }

    // Dealloca le allocazioni dinamiche della memoria
    free(xloc);
    if (menum == 0) {
        free(x);
    }

    MPI_Finalize();
    return 0;
}


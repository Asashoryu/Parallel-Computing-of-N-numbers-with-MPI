#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include "mpi.h"

// Definizione dei tipi personalizzati tramite macro, fare attenzione a sostituire entrambi con tipi sensati come:
// - Per il tipo short:  #define MIO_TIPO short e  #define MIO_TIPO_MPI MPI_SHORT
// - Per il tipo int:    #define MIO_TIPO int e    #define MIO_TIPO_MPI MPI_INT
// - Per il tipo float:  #define MIO_TIPO float e  #define MIO_TIPO_MPI MPI_FLOAT
// - Per il tipo double: #define MIO_TIPO double e #define MIO_TIPO_MPI MPI_DOUBLE
#define MIO_TIPO int
#define MIO_TIPO_MPI MPI_INT

void initializeNumbers(int n, MIO_TIPO *x, int menum);

int main(int argc, char *argv[]) {
    int menum, nproc, strategy;
    int n, nloc, tag, i;
    MPI_Status status;
    // Dichiarazione delle variabili con tipi personalizzati
    MIO_TIPO *x, *xloc, tmp, sum, sum_parz, sum_tmp;  
    // Dichiarazione dei tipi per le strategie 2 e 3
    int log2nproc, partner, send_tag, recv_tag;
    // Dichiarazione delle variabili per i tempi
    double t0, t1, time; /* Servono a tutti i processi */
    double timetot;      /* Serve solo a P0 */

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &menum);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    if (menum == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s n [strategy]\n", argv[0]);
            fprintf(stderr, "Please provide the value of 'n'. You can also specify an optional 'strategy' argument (0, 1, 2, or 3).\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        n = atoi(argv[1]);

        if (n <= 0 || n > INT_MAX) {
            fprintf(stderr, "Invalid value of 'n'. Please provide a positive integer within the range of representable integers.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        if (argc >= 3) {
            strategy = atoi(argv[2]);
            if (strategy != 0 && strategy != 1 && strategy != 2 && strategy != 3) {
                fprintf(stderr, "Invalid strategy: %d\n", strategy);
                MPI_Abort(MPI_COMM_WORLD, 1);
                return 1;
            }

            // Verifica se la strategia è 2 o 3 e se N non è 1 o una potenza di 2 
            if ((strategy == 2 || strategy == 3) && ((n & (n - 1)) != 0 && n != 1)) {
                fprintf(stderr, "Strategy %d requires N to be a power of 2 or 1. Automatically switching to strategy 1.\n", strategy);
                strategy = 1;
            }
        } else {
            // Se non viene specificata una strategia, imposta il valore predefinito a 0
            strategy = 0;
        }

        // Allocazione di memoria per l'array x
        x = (MIO_TIPO *)malloc(n * sizeof(MIO_TIPO));

        if (x == NULL) {
            fprintf(stderr, "Memory allocation of array x failed.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        for (i = 0; i < n; i++) {
            initializeNumbers(n, x, menum);
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
            MPI_Send(&x[start], tmp, MIO_TIPO_MPI, i, tag, MPI_COMM_WORLD);
        }
    } else {
        tag = 22 + menum;
        MPI_Recv(xloc, nloc, MIO_TIPO_MPI, 0, tag, MPI_COMM_WORLD, &status);
    }

    // Inizia il calcolo dei tempi
    MPI_Barrier(MPI_COMM_WORLD);
    t0=MPI_Wtime();
    
    sum = 0;

    // utilizza la strategia 0, ovvero il calcolo della somma in modo sequenziale
    if (strategy == 0) {
        if (menum == 0) {
            for (i = 0; i < n; i++) {
                sum += x[i];
            }
        }
    }
    // Utilizza la strategia 1
    else if (strategy == 1) {
        if (menum == 0) {
            for (i = 0; i < nloc; i++) {
                sum += x[i];
            }
            for (i = 1; i < nproc; i++) {
                tag = 80 + i;
                MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, i, tag, MPI_COMM_WORLD, &status);
                sum += sum_parz;
            }
            // printf("La somma totale è %f\n", sum);  // Utilizzo del tipo personalizzato per stampare
        } else {
            for (i = 0; i < nloc; i++) {
                sum += xloc[i];
            }
            tag = menum + 80;
            MPI_Send(&sum, 1, MIO_TIPO_MPI, 0, tag, MPI_COMM_WORLD);
        }
    // Utilizza la strategia 2
    } else if (strategy == 2) {
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
                        MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, partner, recv_tag, MPI_COMM_WORLD, &status);
                        sum += sum_parz;
                    }
                } else {
                    // Chi spedisce
                    partner = menum - (1 << i);
                    send_tag = 300 + menum;
                    MPI_Send(&sum, 1, MIO_TIPO_MPI, partner, send_tag, MPI_COMM_WORLD);
                }
            }
        }
        // Al termine dell'elaborazione, puoi stampare il risultato
        // printf("\nSono il processo %d: Somma totale=%f\n", menum, sum);  // Utilizzo del tipo personalizzato per stampare
    // Utilizza la strategia 3
    } else if(strategy == 3) {
        for (i = 0; i < nloc; i++) {
            sum += xloc[i];
        }

        for (i = 0; i < log2nproc; i++) {
            if ((menum % (1 << i + 1)) < (1 << i)) {
                partner = menum + (1 << i);
                // Spedisci a menum + 2^i
                send_tag = 300 + menum;
                MPI_Send(&sum, 1, MIO_TIPO_MPI, partner, send_tag, MPI_COMM_WORLD);
                // Ricevi da menum + 2^i
                recv_tag = 300 + partner;
                MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, partner, recv_tag, MPI_COMM_WORLD, &status);
                sum += sum_parz;
            } else {
                partner = menum - (1 << i);
                sum_tmp = sum;
                // Ricevi da menum - 2^i
                recv_tag = 300 + partner;
                MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, partner, recv_tag, MPI_COMM_WORLD, &status);
                sum += sum_parz;
                // Spedisci a menum - 2^i
                send_tag = 300 + menum;
                MPI_Send(&sum_tmp, 1, MIO_TIPO_MPI, partner, send_tag, MPI_COMM_WORLD);
            }
        }
        // Al termine dell'elaborazione, puoi stampare il risultato
        // printf("\nSono il processo %d: Somma totale=%f\n", menum, sum);  // Utilizzo del tipo personalizzato per stampare
    }

    t1=MPI_Wtime();
    time=t1-t0; /*ora ogni processore conosce il proprio tempo*/
    // printf("Sono %d: Tempo impiegato: %e secondi\n", menum, time); 

    MPI_Reduce(&time,&timetot,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);

    // il processo p0 stampa il suo risultato:
    if (menum == 0) {
        printf("\nSono il processo %d: Somma totale=%lf in tempo %e secondi\n", menum, sum, timetot);  // Utilizzo del tipo personalizzato per stampare
        saveResultsToCSV(n, nproc, strategy, timetot);
    }


    // Dealloca le allocazioni dinamiche della memoria
    free(xloc);
    if (menum == 0) {
        free(x);
    }

    MPI_Finalize();
    return 0;
}

void initializeNumbers(int n, MIO_TIPO *x, int menum) {
    if (menum == 0) {
        if (n <= 20) {
            FILE *file = fopen("numbers.txt", "r");
            if (file == NULL) {
                fprintf(stderr, "Error opening file: numbers.txt\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
                return;
            }

            for (int i = 0; i < n; i++) {
                if (fscanf(file, "%d", &x[i]) != 1) {
                    fprintf(stderr, "Error reading numbers from file: numbers.txt\n");
                    fclose(file);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                    return;
                }
            }
            fclose(file);
        } else {
            // Genera numeri casuali per n maggiore di 20
            srand((unsigned int)time(NULL));
            for (int i = 0; i < n; i++) {
                x[i] = rand() % 100;  // range regolabile
            }
        }
    }
}

// Funzione per salvare i risultati dell'elaborazione in un file CSV
void saveResultsToCSV(int n, int p, int strategy, double time) {
    FILE *file = fopen("results.csv", "a"); // Apri il file CSV in modalità append
    if (file == NULL) {
        printf("Errore nell'apertura del file.\n");
        exit(1);
    }

    // Scrivi i dati in formato CSV
    fprintf(file, "%d,%d,%d,%.6f\n", n, p, strategy, time);

    fclose(file); // Chiudi il file
}


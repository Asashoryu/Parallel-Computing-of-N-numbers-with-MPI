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
#define MIO_TIPO double
#define MIO_TIPO_MPI MPI_DOUBLE

void initializeNumbers(int n, MIO_TIPO *x, int menum);
void saveResultsToCSV(int n, int p, int strategy, double time);

int main(int argc, char *argv[]) {
    int menum, nproc, strategy;
    int n, nloc, tag, i, tmp;
    MPI_Status status;
    // Dichiarazione delle variabili con tipi personalizzati
    MIO_TIPO *x = NULL, *xloc = NULL, sum, sum_parz, sum_tmp;  
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

            // Verifica se la strategia è 2 o 3 e se P non è 1 o una potenza di 2 
            if ((strategy == 2 || strategy == 3) && ((nproc & (nproc - 1)) != 0 && nproc != 1)) {
                fprintf(stderr, "Strategy %d requires P to be a power of 2 or 1. Automatically switching to strategy 1.\n", strategy);
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

        initializeNumbers(n, x, menum);
    }

    // Distribuzione di n e strategy a tutti i processi
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);  // Tipo MPI int
    MPI_Bcast(&strategy, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (strategy == 2 || strategy == 3) {
        log2nproc = (int)(log(nproc) / log(2));
    }

    nloc = n / nproc;
    int rest = n % nproc;
    if (menum < rest) {
        nloc = nloc + 1;
    }
    xloc = (MIO_TIPO *)malloc(nloc * sizeof(MIO_TIPO));
    if (xloc == NULL) {
        fprintf(stderr, "Memory allocation of array xloc failed.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    if (menum == 0) {
        free(xloc);
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
            // Processore radice (0) calcola la somma cumulativa delle somme parziali
            for (i = 0; i < nloc; i++) {
                sum += x[i];
            }
            // Processore radice riceve le somme parziali dai processori non zero
            for (i = 1; i < nproc; i++) {
                tag = 300 + i;
                MPI_Recv(&sum_parz, 1, MIO_TIPO_MPI, i, tag, MPI_COMM_WORLD, &status);
                sum += sum_parz;
            }
        } else {
            // Processori non zero inviano le somme locali al processore radice
            for (i = 0; i < nloc; i++) {
                sum += xloc[i];
            }
            tag = menum + 300;
            MPI_Send(&sum, 1, MIO_TIPO_MPI, 0, tag, MPI_COMM_WORLD);
        }
    // Utilizza la strategia 2
    } else if (strategy == 2) {
        // Ogni processore somma i dati locali
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
    // Utilizza la strategia 3
    } else if(strategy == 3) {
        // Ogni processore somma i dati locali
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
    }

    t1=MPI_Wtime();
    time=t1-t0; /*ora ogni processore conosce il proprio tempo*/
    // printf("Sono %d: Tempo impiegato: %e secondi\n", menum, time); 
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Reduce(&time,&timetot,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);

    // il processo p0 stampa il suo risultato:
    if (menum == 0) {
        printf("\nSono il processo %d: Somma totale=%lf in tempo %e secondi\n", menum, (double)sum, timetot);  // Utilizzo del tipo personalizzato per stampare
        saveResultsToCSV(n, nproc, strategy, timetot);
    }


    // Dealloca le allocazioni dinamiche della memoria 
    /*
    free(xloc);
    if (menum == 0) {
        free(x);
    }
    */

    MPI_Finalize();
    return 0;
}

// Questa funzione inizializza l'array di numeri x in base a vari scenari
void initializeNumbers(int n, MIO_TIPO *x, int menum) {
    int i;
    if (menum == 0) {
        if (n <= 20) {
            // Se n è minore o uguale a 20, leggi i dati da un file "numbers.txt"
            FILE *file = fopen("numbers.txt", "r");
            if (file == NULL) {
                // Se si verifica un errore nell'apertura del file, stampa un messaggio di errore e termina il programma
                fprintf(stderr, "Errore nell'apertura del file 'numbers.txt'.\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
                return;
            }
            for (i = 0; i < n; i++) {
                if (fscanf(file, "%lf", &x[i]) != 1) {
                    // Se si verifica un errore nella lettura dei dati dal file, stampa un messaggio di errore, chiudi il file e termina il programma
                    fprintf(stderr, "Errore nella lettura dei numeri dal file 'numbers.txt'.\n");
                    fclose(file);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                    return;
                }
            }
            fclose(file);
        } else {
            // Genera numeri casuali per n maggiore di 20
            srand((unsigned int)time(NULL));
            for (i = 0; i < n; i++) {
                x[i] = (MIO_TIPO)(rand() % 100);  // Genera numeri casuali nel range da 0 a 99 (regolabile)
            }
        }
    }
}


// Questa funzione salva i risultati delle prestazioni in un file CSV
void saveResultsToCSV(int n, int p, int strategy, double time) {
    // Apri il file CSV in modalità append
    FILE *file = fopen("results.csv", "a");
    if (file == NULL) {
        // Se si verifica un errore nell'apertura del file, stampa un messaggio di errore e termina il programma
        printf("Errore nell'apertura del file 'results.csv'.\n");
        exit(1);
    }

    // Scrivi i dati dei risultati nel file CSV in formato "n, p, strategy, time"
    fprintf(file, "%d, %d, %d, %e\n", n, p, strategy, time);

    // Chiudi il file dopo aver scritto i dati
    fclose(file);
}





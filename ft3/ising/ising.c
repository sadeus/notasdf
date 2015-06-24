
/* 
    Simulación de un sistema 2D Ising por medio de un algoritmo
    Metropolis Monte Carlo NVT (canónico)

    Para compilar
    >>> gcc -o3 ising ising.c -lm
    

    Parametros
    ----------------
    -L (int) tamaño de la red LxL
    -T (double) inverso de la temperatura adimensional (beta = J/(k*T))
    -n (int) cantidad de muestras a tomar
    -fs (int) cantidad de pasos entre muestra y muestra
    -nT (int) cantidad de pasos hasta termalizar
    -o (string) archivo donde agrega los datos finalmente
   
    Resultados
    ---------------
    Imprime en pantalla la temperatura, magnetización y la energía,
    además lo guarda en el archivo indicado (por defecto en ./med)
    
    Transfondo
    --------------
    El Hamiltoneano del modelo de Ising es

    H = -J sum_<ij> s_i * s_j,

    donde "sum_<ij>" es la suma sobre los primeros vecinos de la
    posición (i,j). Esta suma depende de la forma de la red, en el
    caso 2D se consideran los primeros vecinos a los spines arriba,
    abajo y a los costados. La proyección de spin  utilizada 
    corresponde a s_i = +/- 1, y J es un parámetro arbitrario de 
    acomplamiento.

*/
#include <stdio.h>
#include <math.h>
#include <stdlib.h>


//Parametros del programa
struct Parametros {
    int L;
    double beta;
    int n_samp;
    int n_term;
    int f_samp;
    int seed;
};


//Computa el cambio de energía debido al giro del spin i,j, 
//que corresponde a la energía asociada a los primeros vecinos
//De esa forma DeltaE puede valor -8,-4,0,4,8 (sin unidades)
int DeltaE(char F[], int L, int i) {
	//Cantidad total de elementos. Matriz de N,N.
	//Para portar a una red genérica se podría usar una clase
    int N = L * L;
	//Condiciones de contorno periodica en ambas direcciones
    int i_izq =  i % L == 0 ? i + (L - 1) : i - 1;  
    int i_der = (i + 1) % L == 0 ? i + 1 - L : i + 1;
    int i_up = i - L + 1 < 0 ? i + N - L : i - L;
    int i_down = i + L + 1 > N ? i + L - N : i + L;
    printf("%d, %d, %d, %d, %d \n", i, i_izq, i_der, i_up, i_down);
    return (int)- 2 * F[i] * (F[i_izq] + F[i_der] + F[i_down] + F[i_up]);
	
}

//Función que mide la magnetización y la energía por sitio
//e y m se pasan por referencia, energía y magnetización
void sampleo(char F[], int L, double * m, double * e){
    int i, i_der, i_down;
    int N = L*L;
    *m = 0.0;
    *e = 0.0;
    for (i = 0; i < N; i++) {
        *m += F[i];
        //Multiplica con los vecinos posteriores.
        i_der = (i + 1) % L ? i + 1 : i - L + 1;
        i_down = i + L < N ? i + L : i + L - N;
        *e +=  F[i] * (F[i_der] + F[i_down]); 
    }
    *m /= N;
    *e /= N;
}


//Genera el array de forma aleatoria
//Usa un entero como un bitfield, si es 1 => 1, 0 => -1
void initRandom(char F[], int L) {
    int i;
    int N = L*L;
    while (N) {
        int r = rand();
        for (i = 0; N && i < sizeof(int); ++i) {
            F[--N] = r >> i & 1 ? -1 : 1;
        }
    }
}

//Genera un array de forma ordenada, todos 1
void initOrd(char F[], int L) {
    int i;
    int N = L * L;
    for (i = 0; i < N; i++) {
        if (i % 2)
            *(F + i) = 1;
        else
            F[i] = -1;
    }
}


void parseParametros(int argc, char * argv[], struct Parametros *param) {
    int i = 0;
    for (i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i],"-L"))
            param->L = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-T"))
            param->beta = atof(argv[++i]);
        else if (!strcmp(argv[i],"-n"))
            param->n_samp = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-nT"))
            param->n_term = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-fs"))
            param->f_samp = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-s"))
            param->seed = (unsigned long)atoi(argv[++i]);
    }
}

void MonteCarlo(char F[], int L, double beta) {
    int de;   //Delta de energía
    int N = L*L;
    double p[2]; //Factor de aceptación
    double r; //Número aleatorio, para comprar con la aceptación
    double m = 0.0, m2_sum = 0.0, m_sum=0.0; //Magentización
    double e=0.0, e2_sum = 0.0, e_sum=0.0; //Energía
    int f_samp = 100;  //Cantidad de pasos entre mediciones
    int n_term = 500; //Cantidad de pasos hasta termalizar
    int n_samp = 100;   //Cantidad de mediciones
    int n_iter = f_samp * n_samp + n_term;
    int i, j, k, c;
    p[0] = exp(-8 * beta); //Prealoco las exponenciales.
    p[1] = exp(-4 * beta); 
    for (c = 0; c < n_iter; c++) {
        //Por cada paso de MC hace N giros de spin.
        for (k = 0; k < N; k++) {
            //i = rand() % N; //posición al azar del array
            de = DeltaE(F, L, k); //Delta de Enegía al posiblemente cambiar el estado.
            if (de < 0) {
                //DeltaE < 0 => Minimiza energía => Invierte el spin.
                F[k] *= -1;
            }
            else if (de > 0) { //Si de > 0 no hacer nada.
                r = (double)rand()/RAND_MAX * 1.0; //Número aleatorio en [0,1].
                if (p[de == 4 ? 0 : 1] > r)
                    F[k] *= -1; //Acepta el cambio => Invierte el spin.
            }
        }
        //Samplea n_samp veces, después de que haya termalizado (n_term pasos).
        if ((c % f_samp) == 0 && c > n_term) {
            sampleo(F, L, &m, &e);
            m_sum += (double)fabs((float) m/N); 
            m2_sum += m*m/(N*N); 
            e_sum += e/N; 
            e2_sum += e/N*e/N;
            n_samp++;
        }
    }
    printf("%f %f \n", beta, m_sum);
    //cout << e_sum << " " <<  e2_sum - e_sum*e_sum << "\n"; //energía más varianza.
}


int main(int argc, char * argv[]) {
    struct Parametros param; //Parametros
    param.L = 4;
    double beta = 1;
    int N = param.L * param.L; //Cantidad de spines
    int L = param.L;
    int i;

    //int n_iter = param.f_samp * param.n_samp + param.n_term; //Pasos de MC, cada uno tiene N cambios de spin
    param.seed = time(NULL);

    //Inicialización
    parseParametros(argc, argv, &param); //Parsea los parámetros.
    L = param.L;
    N = L * L;
    beta = param.beta;
    srand(param.seed);
    char F[N];
    initRandom(F, L);
    for (i = 0; i < N; i++) {
        printf("%d \n", F[i]);
    }
    

    //MonteCarlo(F, L, beta);
   
    return 0;
}
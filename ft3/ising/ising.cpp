
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
#include <fstream>
#include <iostream>
#include <string>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cmath>
using namespace std;

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
	return (int) 2 * F[i] * (F[i_izq] + F[i_der] +
		       F[i_down] + F[i_up]);
	
}

//Función que mide la magnetización y la energía por sitio
//e y m se pasan por referencia, energía y magnetización
void sampleo(char F[], int L, double &m, double &e){
    int i, i_der, i_down;
    int N = L*L;
    m = 0.0;
    e = 0.0;
    for (i = 0; i < N; i++) {
        m += F[i];
        //Multiplica con los vecinos posteriores.
        i_der = (i + 1) % L ? i + 1 : i - L + 1;
        i_down = i + L < N ? i + L : i + L - N;
        e +=  F[i] * (F[i_der] + F[i_down]); 
    }
    m /= N;
    e /= N;
    m = fabs(m);
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
            F[i] = 1;
        else
            F[i] = -1;
    }
}


void parseParametros(int argc, char * argv[], Parametros &param) {
    int i = 0;
    for (i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i],"-L"))
            param.L = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-T"))
            param.beta = 1.0 / atof(argv[++i]);
        else if (!strcmp(argv[i],"-n"))
            param.n_samp = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-nT"))
            param.n_term = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-fs"))
            param.f_samp = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-s"))
            param.seed = (unsigned long)atoi(argv[++i]);
    }
}


int main(int argc, char * argv[]) {
    Parametros param; //Parametros
    param.L = 4;
    param.beta = 1; //Temperatura adimensional  beta = J/(k*T)
    int N = param.L * param.L; //Cantidad de spines
    param.f_samp = 100;  //Cantidad de pasos entre mediciones
    param.n_term = 500; //Cantidad de pasos hasta termalizar
    param.n_samp = 100;   //Cantidad de mediciones
    int n_iter = param.f_samp * param.n_samp + param.n_term; //Pasos de MC, cada uno tiene N cambios de spin
    param.seed = time(NULL);
    //Variables internas de cálculo
    int de;   //Delta de energía
    double p[2]; //Factor de aceptación
    double r; //Número aleatorio, para comprar con la aceptación
    int i,j,a,c;  //Contadores
    //Observables
    double m = 0.0, m2_sum = 0.0, m_sum=0.0; //Magentización
    double e=0.0, e2_sum = 0.0, e_sum=0.0; //Energía
    //Inicialización
    parseParametros(argc, argv, param); //Parsea los parámetros.
   	N = param.L * param.L;  //Recalcula parámetros derivados.
    n_iter = param.f_samp * param.n_samp + param.n_term;
    srand(param.seed); //Genera los números aleatorios a partir de seed.
    char* F = new char[N]; //Aloca la red. Sin el new se puede pisar en el stack al hacer muchas llamadas.
    initOrd(F, param.L); //Inicializa la red. Para aleatorio usar initRandom(F, param.L).
    p[0] = exp(-8 * param.beta); //Prealoco las exponenciales.
    p[1] = exp(-4 * param.beta); 
    // Corrida de MC
    param.n_samp = 0; //Aún cuando es un parámetro, lo vuelvo a contabilizar.
    for (c = 0; c < n_iter; c++) {
        //Por cada paso de MC hace N giros de spin.
        for (i = 0; i < N; i++) {
            de = DeltaE(F, param.L, i); //Delta de Enegía al posiblemente cambiar el estado.
            if (de < 0) {
                //DeltaE < 0 => Minimiza energía => Invierte el spin.
                F[i] *= -1;
            }
            else if (de > 0) { //Si de > 0 no hacer nada.
                r = (double)rand()/RAND_MAX * 1.0; //Número aleatorio en [0,1].
                if (p[de == 4 ? 0 : 1] > r)
                    F[i] *= -1; //Acepta el cambio => Invierte el spin.
            }
        }
        //Samplea n_samp veces, después de que haya termalizado (n_term pasos).
        if ((c % param.f_samp) == 0 && c > param.n_term) {
            sampleo(F, param.L, m, e);
            m_sum += m; 
            m2_sum += m*m; 
            e_sum += e; 
            e2_sum += e*e;
            param.n_samp++;
        }
    }
    //Calcula los promedios.
    m_sum /= param.n_samp;
    e_sum /= param.n_samp;
    e2_sum /= param.n_samp;
    m2_sum /= param.n_samp;
    //Imprime en STDOUT.
    cout << 1.0 / param.beta << " "; //Temperatura adimensional.
    cout << m_sum << " " <<  m2_sum - m_sum * m_sum << " ";  //magnetización más varianza.
    cout << e_sum << " " <<  e2_sum - e_sum*e_sum << "\n"; //energía más varianza.
    return 0;
}
